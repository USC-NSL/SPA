/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/CFGBackwardIF.h"

#include "llvm/IR/Instructions.h"
#include <../Core/Common.h>
#include <klee/Internal/Module/KInstruction.h>

namespace SPA {
CFGBackwardIF::CFGBackwardIF(CFG &cfg, CG &cg,
                             std::set<llvm::Instruction *> targets) {
  // Use a worklist to add all predecessors of target instruction.
  std::set<llvm::Instruction *> worklist = targets;
  std::set<llvm::Function *> pathFunctions;

  klee::klee_message("      Exploring reverse path.");
  while (!worklist.empty()) {
    std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
    llvm::Instruction *inst = *it;
    worklist.erase(it);

    // Mark instruction as reaching.
    reaching[inst] = true;
    std::set<llvm::Instruction *> p = cfg.getPredecessors(inst);
    // Add all non-reaching predecessors to work list.
    for (it = p.begin(), ie = p.end(); it != ie; it++)
      if (reaching.count(*it) == 0)
        worklist.insert(*it);
    // Mark function as on direct path.
    // 			if ( pathFunctions.count( inst->getParent()->getParent() ) == 0 )
    // 				klee_message( "		Direct path function: " <<
    // inst->getParent()->getParent()->getName().str() );
    pathFunctions.insert(inst->getParent()->getParent());
    // Check if entry instruction.
    if (inst == &(inst->getParent()->getParent()->getEntryBlock().front())) {
      p = cg.getPossibleCallers(inst->getParent()->getParent());
      // Add all non-reaching callers to work list.
      for (it = p.begin(), ie = p.end(); it != ie; it++)
        if (reaching.count(*it) == 0)
          worklist.insert(*it);
    }
  }

  // Define filter out set as opposite of reaching set within analyzed direct
  // path functions.
  for (CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++)
    if (pathFunctions.count((*it)->getParent()->getParent()) &&
        !reaching.count(*it))
      reaching[*it] = false;
}

bool CFGBackwardIF::checkInstruction(llvm::Instruction *instruction) {
  return reaching.count(instruction) && reaching[instruction];
}

double CFGBackwardIF::getUtility(klee::ExecutionState *state) {
  bool knownFound = false;
  // Traverse call stack downward from root to current PC.
  // A filter out will match this regexp: ^[Unknown]*[Reaching]+[!Reaching].*$
  for (klee::ExecutionState::stack_ty::const_reverse_iterator
           it = state->stack.rbegin(),
           ie = state->stack.rend();
       it != ie; it++) {
    if (it->caller) {
      if (reaching.count(it->caller->inst)) {
        if (knownFound && !reaching[it->caller->inst]) // Found non-reaching.
          return UTILITY_FILTER_OUT;
        knownFound = true;
      } else {
        if (knownFound) // Found unknown after reaching.
          return UTILITY_DEFAULT;
      }
    }
  }
  return UTILITY_DEFAULT;
}

std::string CFGBackwardIF::getColor(CFG &cfg, CG &cg,
                                    llvm::Instruction *instruction) {
  if (reaching.count(instruction) == 0)
    return "grey";
  else if (reaching[instruction])
    return "white";
  else
    return "dimgrey";
}
}
