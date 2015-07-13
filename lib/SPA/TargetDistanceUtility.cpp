/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/SPA.h>
#include <spa/TargetDistanceUtility.h>
#include <spa/Util.h>

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
llvm::cl::opt<bool> UseShallowDistance(
    "use-shallow-distance",
    llvm::cl::desc("Performs a faster but less precise call-depth analysis."),
    llvm::cl::init(false));

TargetDistanceUtility::TargetDistanceUtility(llvm::Module *module, CFG &cfg,
                                             CG &cg, InstructionFilter &filter)
    : cfg(cfg), cg(cg) {
  std::set<llvm::Instruction *> worklist;

  for (auto it : cfg) {
    if (filter.checkInstruction(it)) {
      distances[it] = std::make_pair(+INFINITY, false);
    } else {
      distances[it] = std::make_pair(0, true);
      propagateChanges(worklist, it);
    }
  }

  assert((!worklist.empty()) && "Filter must exclude something.");
  processWorklist(module, worklist);
}

TargetDistanceUtility::TargetDistanceUtility(
    llvm::Module *module, CFG &cfg, CG &cg,
    std::set<llvm::Instruction *> &targets)
    : cfg(cfg), cg(cg) {
  std::set<llvm::Instruction *> worklist;

  for (auto it : cfg) {
    distances[it] = std::make_pair(+INFINITY, false);
  }

  for (auto it : targets) {
    distances[it] = std::make_pair(0, true);
    propagateChanges(worklist, it);
  }

  processWorklist(module, worklist);
}

void
TargetDistanceUtility::propagateChanges(std::set<llvm::Instruction *> &worklist,
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
    llvm::Module *module, std::set<llvm::Instruction *> &worklist) {

  std::set<llvm::Instruction *> blockedReturns;
  llvm::Function *fn = module->getFunction(SPA_RETURN_ANNOTATION_FUNCTION);
  if (fn) {
    // Find return instructions that succeed spa_return.
    std::set<llvm::Instruction *> spaReturns;
    std::set<llvm::Function *> spaReturnFunctions;
    std::set<llvm::Instruction *> spaReturnWorklist = cg.getDefiniteCallers(fn);
    std::set<llvm::Instruction *> spaReturnSuccessors;
    while (!spaReturnWorklist.empty()) {
      llvm::Instruction *inst = *spaReturnWorklist.begin();
      spaReturnWorklist.erase(spaReturnWorklist.begin());

      spaReturnSuccessors.insert(inst);
      spaReturnFunctions.insert(inst->getParent()->getParent());

      if (cfg.returns(inst)) {
        spaReturns.insert(inst);
        klee::klee_message("      Return affected by spa_return found at: %s.",
                           debugLocation(inst).c_str());
      } else {
        // Process successors.
        for (auto it : cfg.getSuccessors(inst)) {
          if (!spaReturnSuccessors.count(it)) {
            spaReturnWorklist.insert(it);
          }
        }
      }
    }
    // Find return instruction that didn't succeed spa_return.
    for (auto inst : cfg) {
      if (cfg.returns(inst) &&
          spaReturnFunctions.count(inst->getParent()->getParent()) &&
          !spaReturns.count(inst)) {
        blockedReturns.insert(inst);
      }
    }
  } else {
    klee::klee_message("      Found no spa_returns.");
  }

  // Initialize the cost of all instructions.
  std::set<llvm::Instruction *> indirectWorklist;
  klee::klee_message("      Initializing costs.");
  if (NoReturnEqualization) {
    klee::klee_message("      Disabled return equalization.");
  } else {
    klee::klee_message("      Using return equalization.");
  }
  if (UseShallowDistance) {
    klee::klee_message("      Using shallow distance call-depth analysis.");
  } else {
    klee::klee_message("      Using full call-depth analysis.");
  }
  for (auto inst : cfg) {
    if (distances[inst].second)
      continue;
    if (cfg.getSuccessors(inst).empty()) {
      if (blockedReturns.count(inst)) {
        distances[inst] = std::make_pair(+INFINITY, false);
      } else {
        if (NoReturnEqualization) {
          distances[inst] = std::make_pair(1, false);
        } else {
          // Initialize to the function's max depth - the depth of the
          // particular
          // return.
          // This equalizes the cost of reaching all returns.
          distances[inst] =
              std::make_pair(getFunctionDepth(inst->getParent()->getParent(),
                                              std::set<llvm::Function *>()) -
                                 getInstructionDepth(inst),
                             false);
        }
        indirectWorklist.insert(cfg.getPredecessors(inst).begin(),
                                cfg.getPredecessors(inst).end());
      }
    } else {
      distances[inst] = std::make_pair(+INFINITY, false);
    }
  }

  // Process indirect worklist.
  klee::klee_message("      Processing indirect paths.");
  while (!indirectWorklist.empty()) {
    llvm::Instruction *inst = *indirectWorklist.begin();
    indirectWorklist.erase(inst);

    if (distances[inst].second)
      continue;

    // Check cost of successors.
    double minSCost = +INFINITY;
    for (auto it : cfg.getSuccessors(inst)) {
      double d = distances[it].first + 1;
      if (d < minSCost) {
        minSCost = d;
      }
    }
    double minCCost;
    if (cg.getPossibleCallees(inst).empty()) {
      minCCost = 0;
    } else {
      minCCost = +INFINITY;

      for (auto it : cg.getPossibleCallees(inst)) {
        double d = getFunctionDepth(it, std::set<llvm::Function *>()) + 1;
        if (d < minCCost) {
          minCCost = d;
        }
      }
    }

    if ((!distances.count(inst)) ||
        minSCost + minCCost < distances[inst].first) {
      distances[inst] = std::make_pair(minSCost + minCCost, false);
      propagateChanges(indirectWorklist, inst);
    }
  }

  klee::klee_message("      Processing direct paths.");
  std::set<llvm::Function *> pathFunctions;
  while (!worklist.empty()) {
    llvm::Instruction *inst = *worklist.begin();
    worklist.erase(inst);

    // Mark function as on direct path.
    pathFunctions.insert(inst->getParent()->getParent());

    // Check cost of successors.
    if (cg.getPossibleCallees(inst).empty()) {
      // Normal succession. distance = min(successors) + 1
      double minSCost = +INFINITY;
      for (auto it : cfg.getSuccessors(inst)) {
        if (distances[it].second) {
          double d = distances[it].first + 1;
          if (d < minSCost) {
            minSCost = d;
          }
        }
      }
      if ((!distances[inst].second) || minSCost < distances[inst].first) {
        distances[inst] = std::make_pair(minSCost, true);
        propagateChanges(worklist, inst);
      }
    } else {
      // Function call. Check if call is on direct path.
      bool isDirectCall = false;
      for (auto it : cg.getPossibleCallees(inst)) {
        if (pathFunctions.count(it)) {
          isDirectCall = true;
          break;
        }
      }
      if (isDirectCall) {
        // Call on direct path. distance = min(callees) + 1
        double minCCost = +INFINITY;
        for (auto it : cg.getPossibleCallees(inst)) {
          if ((!it->empty()) && distances[&it->front().front()].second) {
            double d = distances[&it->front().front()].first + 1;
            if (d < minCCost) {
              minCCost = d;
            }
          }
        }
        if ((!distances[inst].second) || minCCost < distances[inst].first) {
          distances[inst] = std::make_pair(minCCost, true);
          propagateChanges(worklist, inst);
        }
      } else {
        // Call to function on indirect path.
        // distance = min(successors) + 1 + min(callees) + 1
        // Check cost of successors.
        double minSCost = +INFINITY;
        for (auto it : cfg.getSuccessors(inst)) {
          double d = distances[it].first + 1;
          if (d < minSCost) {
            minSCost = d;
          }
        }
        double minCCost;
        if (cg.getPossibleCallees(inst).empty()) {
          minCCost = 0;
        } else {
          minCCost = +INFINITY;

          for (auto it : cg.getPossibleCallees(inst)) {
            double d = getFunctionDepth(it, std::set<llvm::Function *>()) + 1;
            if (d < minCCost) {
              minCCost = d;
            }
          }
        }

        if ((!distances[inst].second) ||
            minSCost + minCCost < distances[inst].first) {
          distances[inst] = std::make_pair(minSCost + minCCost, true);
          propagateChanges(worklist, inst);
        }
      }
    }
  }

  // Instructions in direct path functions but not on direct path are
  // non-reaching.
  for (auto it : cfg) {
    if (pathFunctions.count(it->getParent()->getParent()) &&
        !distances[it].second) {
      distances[it] = std::make_pair(+INFINITY, true);
    }
  }

  // Fill in call-site successor distances.
  for (auto inst : cfg) {
    if (!cg.getPossibleCallees(inst).empty()) {
      for (auto successor : cfg.getSuccessors(inst)) {
        successorDistances[successor] = distances[inst];
      }
    }
  }

  // Pre-compute min and max distances to normalize colors in CFG.
  for (auto it : cfg) {
    if (getDistance(it) == INFINITY)
      continue;
    if (isFinal(it)) {
      minFinal = std::min(minFinal, getDistance(it));
      maxFinal = std::max(maxFinal, getDistance(it));
    } else {
      if (!minPartial.count(it->getParent()->getParent())) {
        minPartial[it->getParent()->getParent()] = +INFINITY;
      }
      if (!maxPartial.count(it->getParent()->getParent())) {
        maxPartial[it->getParent()->getParent()] = -INFINITY;
      }
      minPartial[it->getParent()->getParent()] =
          std::min(minPartial[it->getParent()->getParent()], getDistance(it));
      maxPartial[it->getParent()->getParent()] =
          std::max(maxPartial[it->getParent()->getParent()], getDistance(it));
    }
  }
}

double
TargetDistanceUtility::getFunctionDepth(llvm::Function *fn,
                                        std::set<llvm::Function *> stack) {
  // Handle empty functions.
  if (fn->empty()) {
    return 0;
  }

  // Prevent infinite recursion.
  if (stack.count(fn)) {
    return 0;
  }
  std::set<llvm::Function *> newStack = stack;
  newStack.insert(fn);

  if (stack.empty()) {
    if (functionDepths.count(fn)) {
      return functionDepths[fn];
    }

    // Clear context.
    functionDepthsInContext.clear();
    // Initialize all instruction depths to +INFINITY
    for (auto it : cfg) {
      instructionDepthsInContext[it] = +INFINITY;
    }
  } else {
    if (functionDepthsInContext.count(fn)) {
      return functionDepthsInContext[fn];
    }
  }

  std::set<llvm::Instruction *> worklist;
  llvm::Instruction *inst;
  // Calculate all depths with a worklist.
  inst = &fn->getEntryBlock().front();
  instructionDepthsInContext[inst] = 0;
  worklist.insert(cfg.getSuccessors(inst).begin(),
                  cfg.getSuccessors(inst).end());
  while (!worklist.empty()) {
    inst = *worklist.begin();
    worklist.erase(inst);

    // Get min cost of succeeding predecessors.
    for (auto pit : cfg.getPredecessors(inst)) {
      double d = 0;
      // If functions called, get min cost of call.
      if (!cg.getPossibleCallees(pit).empty()) {
        for (auto cit : cg.getPossibleCallees(pit)) {
          double fd = getFunctionDepth(cit, newStack);
          if (fd < d) {
            d = fd;
          }
        }
        // Cost of returning
        d++;
      }
      d += instructionDepthsInContext[pit] + 1;
      if (d < instructionDepthsInContext[inst]) {
        instructionDepthsInContext[inst] = d;
        worklist.insert(cfg.getSuccessors(inst).begin(),
                        cfg.getSuccessors(inst).end());
      }
    }
  }

  // Get the max depth of the function.
  double maxDepth = 0;
  for (auto &bbit : *fn) {
    for (auto &iit : bbit) {
      if (instructionDepthsInContext[&iit] < +INFINITY &&
          instructionDepthsInContext[&iit] > maxDepth) {
        maxDepth = instructionDepthsInContext[&iit];
      }
    }
  }
  assert(maxDepth < +INFINITY);

  if (stack.empty() || UseShallowDistance) {
    // Cache in global cache.
    functionDepths[fn] = maxDepth;
    // Copy relevant part of instruction depths to global cache.
    for (auto &bbit : *fn) {
      for (auto &iit : bbit) {
        instructionDepths[&iit] = instructionDepthsInContext[&iit];
      }
    }
  }
  if (stack.empty()) {
    // Clear local cache.
    functionDepthsInContext.clear();
  } else {
    functionDepthsInContext[fn] = maxDepth;
  }
  return maxDepth;
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
    result -= getSuccessorDistance(it->caller->inst);
    if (isSuccessorFinal(it->caller->inst))
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
    if (minPartial[instruction->getParent()->getParent()] <
            maxPartial[instruction->getParent()->getParent()] &&
        getDistance(instruction) != INFINITY)
      result << "0.67+" << ((maxPartial[instruction->getParent()->getParent()] -
                             getDistance(instruction)) /
                            (maxPartial[instruction->getParent()->getParent()] -
                             minPartial[instruction->getParent()->getParent()]))
             << "+1.0";
    else
      result << "0.67+0.0+1.0";
  }
  return result.str();
}
}
