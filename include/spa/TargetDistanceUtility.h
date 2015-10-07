/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __TargetDistanceUtility_H__
#define __TargetDistanceUtility_H__

#include "spa/StateUtility.h"
#include "spa/BlacklistIF.h"

namespace SPA {
class TargetDistanceUtility : public StateUtility {
private:
  bool initialized = false;
  // Stores precomputed instruction distances.
  // The bool indicates whether it is partial (false) or final (true).
  // A state's distance is calculated by traversing the stack and adding up
  // partial distances until a final one is found.
  std::map<llvm::Instruction *, std::pair<double, bool> > distances;
  // Used for call sites.
  std::map<llvm::Instruction *, std::pair<double, bool> > successorDistances;

  // Module data
  llvm::Module *module;
  CFG &cfg;
  CG &cg;
  // Target instructions
  InstructionFilter *filter;
  // Cached function depths.
  std::map<llvm::Function *, double> functionDepths, functionDepthsInContext;
  // Cached instruction depth.
  std::map<llvm::Instruction *, double> instructionDepths,
      instructionDepthsInContext;

  // Min and max distances used to compute colors in visual CFG.
  double minFinal, maxFinal;
  std::map<llvm::Function *, double> minPartial, maxPartial;

  void init();
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
    assert(distances.count(instruction) &&
           "Instruction not processed during initialization.");
    return distances[instruction].first;
  }
  double getSuccessorDistance(llvm::Instruction *instruction) {
    assert(successorDistances.count(instruction) &&
           "Instruction not processed during initialization.");
    return successorDistances[instruction].first;
  }
  bool isFinal(llvm::Instruction *instruction) {
    assert(distances.count(instruction) &&
           "Instruction not processed during initialization.");
    return distances[instruction].second;
  }
  bool isSuccessorFinal(llvm::Instruction *instruction) {
    assert(successorDistances.count(instruction) &&
           "Instruction not processed during initialization.");
    return successorDistances[instruction].second;
  }

public:
  TargetDistanceUtility(llvm::Module *module, CFG &cfg, CG &cg,
                        std::set<llvm::Instruction *> targets)
      : TargetDistanceUtility(module, cfg, cg, new BlacklistIF(targets)) {}
  TargetDistanceUtility(llvm::Module *module, CFG &cfg, CG &cg,
                        InstructionFilter *filter)
      : module(module), cfg(cfg), cg(cg), filter(filter) {}
  double getUtility(klee::ExecutionState *state);
  double getStaticUtility(llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    assert(instruction);
    return -getDistance(instruction);
  }
  std::string getColor(CFG &cfg, CG &cg, llvm::Instruction *instruction);
};
}

#endif // #ifndef __TargetDistanceUtility_H__
