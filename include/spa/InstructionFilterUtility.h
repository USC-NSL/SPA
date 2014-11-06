/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __InstructionFilterUtility_H__
#define __InstructionFilterUtility_H__

#include <klee/Internal/Module/KInstruction.h>

#include "spa/StateUtility.h"
#include "spa/InstructionFilter.h"

namespace SPA {
class InstructionFilterUtility : public StateUtility {
private:
  InstructionFilter *instructionFilter;

public:
  InstructionFilterUtility(InstructionFilter *_instructionFilter)
      : instructionFilter(_instructionFilter) {}

  double getUtility(klee::ExecutionState *state) {
    return instructionFilter->checkInstruction(state->pc()->inst)
               ? UTILITY_DEFAULT
               : UTILITY_FILTER_OUT;
  }

  std::string getColor(CFG &cfg, CG &cg, llvm::Instruction *instruction) {
    return "white";
  }
};
}

#endif // #ifndef __InstructionFilterUtility_H__
