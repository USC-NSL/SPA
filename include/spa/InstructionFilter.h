/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __InstructionFilter_H__
#define __InstructionFilter_H__

#include "llvm/IR/Instruction.h"

#include "spa/CFG.h"

namespace SPA {
class InstructionFilter {
public:
  /**
   * Checks if an instruction should be processed or filtered out.
   *
   * @param instruction The instruction to check.
   * @return true if the instruction should be processed;
   *         false if it should be filtered out.
   */
  virtual bool checkInstruction(llvm::Instruction *instruction) = 0;

  std::set<llvm::Instruction *> toInstructionSet(CFG &cfg) {
    std::set<llvm::Instruction *> result;

    for (auto inst : cfg)
      if (checkInstruction(inst))
        result.insert(inst);

    return result;
  }

protected:
  virtual ~InstructionFilter() {}
};
}

#endif // #ifndef __InstructionFilter_H__
