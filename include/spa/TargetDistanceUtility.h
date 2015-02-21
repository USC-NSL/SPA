/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __TargetDistanceUtility_H__
#define __TargetDistanceUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
class TargetDistanceUtility : public StateUtility {
private:
  // Stores precomputed instruction distances.
  // The bool indicates whether it is partial (false) or final (true).
  // A state's distance is calculated by traversing the stack and adding up
  // partial distances until a final one is found.
  std::map<llvm::Instruction *, std::pair<double, bool> > distances;
  // Used for call sites.
  std::map<llvm::Instruction *, std::pair<double, bool> > successorDistances;

  // CFG and CG
  CFG &cfg;
  CG &cg;
  // Cached function depths.
  std::map<llvm::Function *, double> functionDepths, functionDepthsInContext;
  // Cached instruction depth.
  std::map<llvm::Instruction *, double> instructionDepths,
      instructionDepthsInContext;

  void propagateChanges(std::set<llvm::Instruction *> &worklist,
                        llvm::Instruction *instruction);
  void processWorklist(llvm::Module *module,
                       std::set<llvm::Instruction *> &worklist);
  double getFunctionDepth(llvm::Function *fn, std::set<llvm::Function *> stack);
  double getInstructionDepth(llvm::Instruction *inst) {
    if (!instructionDepths.count(inst)) {
      getFunctionDepth(inst->getParent()->getParent(),
                       std::set<llvm::Function *>());
    }
    assert(instructionDepths.count(inst));
    return instructionDepths[inst];
  }
  double getDistance(llvm::Instruction *instruction) {
    return distances.count(instruction) ? distances[instruction].first
                                        : +INFINITY;
  }
  double getSuccessorDistance(llvm::Instruction *instruction) {
    return successorDistances.count(instruction)
               ? successorDistances[instruction].first
               : +INFINITY;
  }
  bool isFinal(llvm::Instruction *instruction) {
    return distances.count(instruction) ? distances[instruction].second : true;
  }
  bool isSuccessorFinal(llvm::Instruction *instruction) {
    return successorDistances.count(instruction)
               ? successorDistances[instruction].second
               : true;
  }

public:
  TargetDistanceUtility(llvm::Module *module, CFG &cfg, CG &cg,
                        std::set<llvm::Instruction *> &targets);
  TargetDistanceUtility(llvm::Module *module, CFG &cfg, CG &cg,
                        InstructionFilter &filter);
  double getUtility(klee::ExecutionState *state);
  double getStaticUtility(llvm::Instruction *instruction);
  std::string getColor(CFG &cfg, CG &cg, llvm::Instruction *instruction);
};
}

#endif // #ifndef __TargetDistanceUtility_H__
