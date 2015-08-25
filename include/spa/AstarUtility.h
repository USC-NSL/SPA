/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __AstarUtility_H__
#define __AstarUtility_H__

#include "spa/StateUtility.h"
#include "spa/DepthUtility.h"
#include "spa/TargetDistanceUtility.h"
#include "spa/CFG.h"
#include "spa/CG.h"

namespace SPA {
class AstarUtility : public StateUtility {
private:
  DepthUtility depth;
  TargetDistanceUtility targetDistance;

public:
  AstarUtility(llvm::Module *module, CFG &cfg, CG &cg,
               std::set<llvm::Instruction *> &targets)
      : targetDistance(module, cfg, cg, targets) {}
  AstarUtility(llvm::Module *module, CFG &cfg, CG &cg,
               InstructionFilter *filter)
      : targetDistance(module, cfg, cg, filter) {}

  double getUtility(klee::ExecutionState *state) {
    return targetDistance.getUtility(state) - depth.getUtility(state);
  }

  double getStaticUtility(llvm::Instruction *instruction) {
    return targetDistance.getStaticUtility(instruction) -
           depth.getStaticUtility(instruction);
  }

  std::string getColor(CFG &cfg, CG &cg, llvm::Instruction *instruction) {
    return targetDistance.getColor(cfg, cg, instruction);
  }
};
}

#endif // #ifndef __AstarUtility_H__
