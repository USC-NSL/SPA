/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/SPA.h>
#include <spa/TargetDistanceUtility.h>

#include <sstream>
#include "llvm/Support/CommandLine.h"
#include <llvm/IR/Function.h>
#include <../Core/Common.h>
#include <klee/Internal/Module/KInstruction.h>

namespace SPA {
llvm::cl::opt<bool> NoReturnEqualization(
    "no-return-equalization",
    llvm::cl::desc("Disables return equalization in the distance metric."),
    llvm::cl::init(false));

TargetDistanceUtility::TargetDistanceUtility(llvm::Module *module, CFG &cfg,
                                             CG &cg,
                                             InstructionFilter &filter) {
  std::set<llvm::Instruction *> worklist;

  for (auto it : cfg) {
    if (filter.checkInstruction(it)) {
      distances[it] = std::make_pair(+INFINITY, false);
    } else {
      distances[it] = std::make_pair(0, true);
      propagateChanges(cfg, cg, worklist, it);
    }
  }

  assert((!worklist.empty()) && "Filter must exclude something.");
  processWorklist(module, cfg, cg, worklist);
}

TargetDistanceUtility::TargetDistanceUtility(
    llvm::Module *module, CFG &cfg, CG &cg,
    std::set<llvm::Instruction *> &targets) {
  std::set<llvm::Instruction *> worklist;

  for (auto it : cfg) {
    distances[it] = std::make_pair(+INFINITY, false);
  }

  for (auto it : targets) {
    distances[it] = std::make_pair(0, true);
    propagateChanges(cfg, cg, worklist, it);
  }

  processWorklist(module, cfg, cg, worklist);
}

void
TargetDistanceUtility::propagateChanges(CFG &cfg, CG &cg,
                                        std::set<llvm::Instruction *> &worklist,
                                        llvm::Instruction *instruction) {
  // Propagate changes to predecessors.
  worklist.insert(cfg.getPredecessors(instruction).begin(),
                  cfg.getPredecessors(instruction).end());
  // Check if entry instruction and propagate to callers.
  if ((!instruction->getParent()->getParent()->empty()) &&
      instruction ==
          &(instruction->getParent()->getParent()->getEntryBlock().front()))
    worklist.insert(
        cg.getPossibleCallers(instruction->getParent()->getParent()).begin(),
        cg.getPossibleCallers(instruction->getParent()->getParent()).end());
}

void TargetDistanceUtility::processWorklist(
    llvm::Module *module, CFG &cfg, CG &cg,
    std::set<llvm::Instruction *> &worklist) {
  std::set<llvm::Function *> pathFunctions;
  std::map<llvm::Function *, double> functionDepths;

  klee::klee_message("      Processing direct paths.");
  while (!worklist.empty()) {
    llvm::Instruction *inst = *worklist.begin();
    worklist.erase(inst);

    bool updated = false;

    // Check cost of successors.
    for (auto it : cfg.getSuccessors(inst)) {
      double d = distances[it].first + 1;
      if (distances[inst].first > d) {
        distances[inst] = std::make_pair(d, true);
        updated = true;
      }
    }
    // Check cost of callees.
    for (auto it : cg.getPossibleCallees(inst)) {
      if ((!it->empty()) && distances[inst].first >
                                distances[&it->getEntryBlock().front()].first) {
        distances[inst] =
            std::make_pair(distances[&it->getEntryBlock().front()].first, true);
        updated = true;
      }
    }
    // Mark function as on direct path.
    pathFunctions.insert(inst->getParent()->getParent());

    if (updated)
      propagateChanges(cfg, cg, worklist, inst);
  }

  // Instructions in direct path functions but not on direct path are
  // non-reaching.
  for (auto it : cfg) {
    if (pathFunctions.count(it->getParent()->getParent()) &&
        !distances.count(it)) {
      distances[it] = std::make_pair(+INFINITY, true);
    }
  }

  klee::klee_message("      Processing indirect paths.");
  std::set<llvm::Function *> spaReturnFunctions;
  llvm::Function *fn = module->getFunction(SPA_RETURN_ANNOTATION_FUNCTION);
  if (fn) {
    std::set<llvm::Instruction *> spaReturnSuccessors =
        cg.getDefiniteCallers(fn);
    for (auto it : spaReturnSuccessors) {
      worklist.insert(cfg.getPredecessors(it).begin(),
                      cfg.getPredecessors(it).end());
    }
    while (!spaReturnSuccessors.empty()) {
      llvm::Instruction *inst = *spaReturnSuccessors.begin();
      spaReturnSuccessors.erase(spaReturnSuccessors.begin());
      distances[inst] = std::make_pair(0, false);
      spaReturnSuccessors.insert(cfg.getSuccessors(inst).begin(),
                                 cfg.getSuccessors(inst).end());
      if (!spaReturnFunctions.count(inst->getParent()->getParent())) {
        klee::klee_message(
            "      Found spa_return in function: %s.",
            inst->getParent()->getParent()->getName().str().c_str());
        spaReturnFunctions.insert(inst->getParent()->getParent());
      }
    }
  } else {
    klee::klee_message("      Found no spa_returns.");
  }

  // Find the depths of all instructions in indirect path functions without
  // spa_returns.
  std::map<llvm::Instruction *, double> depths;
  // Initialize to +INFINITY
  for (auto it : cfg) {
    if (pathFunctions.count(it->getParent()->getParent()) == 0 &&
        spaReturnFunctions.count(it->getParent()->getParent()) == 0) {
      depths[it] = +INFINITY;
    }
  }
  // Iterate of indirect path functions without spa_returns.
  for (auto it : cg) {
    if ((!it->empty()) && pathFunctions.count(it) == 0 &&
        spaReturnFunctions.count(it) == 0) {
      std::set<llvm::Instruction *> depthWorklist;
      llvm::Instruction *inst;
      // Calculate all depths with a worklist.
      inst = &it->getEntryBlock().front();
      depths[inst] = 0;
      depthWorklist.insert(cfg.getSuccessors(inst).begin(),
                           cfg.getSuccessors(inst).end());
      while (!depthWorklist.empty()) {
        inst = *depthWorklist.begin();
        depthWorklist.erase(inst);

        for (auto pit : cfg.getPredecessors(inst)) {
          double d = depths[pit] + 1;
          if (d < depths[inst]) {
            depths[inst] = d;
            depthWorklist.insert(cfg.getSuccessors(inst).begin(),
                                 cfg.getSuccessors(inst).end());
          }
        }
      }
    }
  }
  // Calculate each functions max depth.
  if (!NoReturnEqualization) {
    klee::klee_message("      Normalizing returns.");
    for (auto it : cfg) {
      if (pathFunctions.count(it->getParent()->getParent()) == 0 &&
          spaReturnFunctions.count(it->getParent()->getParent()) == 0) {
        if (depths[it] > functionDepths[it->getParent()->getParent()]) {
          functionDepths[it->getParent()->getParent()] = depths[it];
        }
      }
    }
  } else {
    klee::klee_message("      Return normalization disabled.");
  }

  // Initialize costs of return instructions in functions without spa_returns.
  // Initialize to the function's max depth - the depth of the particular
  // return.
  // This equalizes the cost of reaching all returns.
  for (auto it : cfg) {
    if (pathFunctions.count(it->getParent()->getParent()) == 0 &&
        cfg.getSuccessors(it).empty() &&
        spaReturnFunctions.count(it->getParent()->getParent()) == 0) {
      distances[it] = std::make_pair(
          NoReturnEqualization
              ? 0
              : functionDepths[it->getParent()->getParent()] - depths[it],
          false);
      worklist.insert(cfg.getPredecessors(it).begin(),
                      cfg.getPredecessors(it).end());
    }
  }
  while (!worklist.empty()) {
    llvm::Instruction *inst = *worklist.begin();
    worklist.erase(inst);

    if (distances[inst].second)
      continue;

    // Check cost of successors.
    bool updated = false;
    for (auto it : cfg.getSuccessors(inst)) {
      double d = distances[it].first + 1;
      if (distances[inst].first > d) {
        distances[inst] = std::make_pair(d, false);
        updated = true;
      }
    }
    if (updated)
      // Propagate changes to predecessors.
      worklist.insert(cfg.getPredecessors(inst).begin(),
                      cfg.getPredecessors(inst).end());
  }
}

double TargetDistanceUtility::getUtility(klee::ExecutionState *state) {
  assert(state);

  double result = -getDistance(state->pc->inst);
  if (isFinal(state->pc->inst))
    return result > -INFINITY ? result : UTILITY_PROCESS_LAST;

  for (klee::ExecutionState::stack_ty::const_reverse_iterator
           it = state->stack.rbegin(),
           ie = state->stack.rend();
       it != ie && it->caller; it++) {
    result -= getDistance(it->caller->inst);
    if (isFinal(it->caller->inst))
      return result > -INFINITY ? result : UTILITY_PROCESS_LAST;
  }
  // No final state was found.
  return UTILITY_PROCESS_LAST;
}

double TargetDistanceUtility::getStaticUtility(llvm::Instruction *instruction) {
  assert(instruction);
  return -getDistance(instruction);
}

std::string TargetDistanceUtility::getColor(CFG &cfg, CG &cg,
                                            llvm::Instruction *instruction) {
  double minPartial = +INFINITY, maxPartial = -INFINITY, minFinal = +INFINITY,
         maxFinal = -INFINITY;

  for (auto it : cfg) {
    if (getDistance(it) == INFINITY)
      continue;
    if (isFinal(it)) {
      minFinal = std::min(minFinal, getDistance(it));
      maxFinal = std::max(maxFinal, getDistance(it));
    } else if (it->getParent()->getParent() ==
               instruction->getParent()->getParent()) {
      minPartial = std::min(minPartial, getDistance(it));
      maxPartial = std::max(maxPartial, getDistance(it));
    }
  }

  std::stringstream result;
  if (isFinal(instruction)) {
    // Min = White, Max = Green
    if (minFinal < maxFinal && getDistance(instruction) != INFINITY)
      result << "0.33+" << ((maxFinal - getDistance(instruction)) /
                            (maxFinal - minFinal)) << "+1.0";
    else
      result << "0.33+0.0+1.0";
  } else {
    // Min = White, Max = Blue
    if (minPartial < maxPartial && getDistance(instruction) != INFINITY)
      result << "0.67+" << ((maxPartial - getDistance(instruction)) /
                            (maxPartial - minPartial)) << "+1.0";
    else
      result << "0.67+0.0+1.0";
  }
  return result.str();
}
}
