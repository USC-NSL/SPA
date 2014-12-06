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
   * Checks if a CFG edge matches the filter criteria.
   *
   * @param preInstruction The head instruction.
   * @param postInstruction The tail instruction.
   * @return true if the instruction should be processed;
   *         false if it should be filtered out.
   */
  virtual bool checkStep(llvm::Instruction *preInstruction,
                         llvm::Instruction *postInstruction) = 0;

  virtual bool checkInstruction(llvm::Instruction *instruction) {
    return checkStep(NULL, instruction);
  }

  std::set<std::pair<llvm::Instruction *, llvm::Instruction *> >
  toStepSet(CFG &cfg, CG &cg) {
    std::set<std::pair<llvm::Instruction *, llvm::Instruction *> > result;

    for (auto postInstruction : cfg) {
      // Check control flow edges.
      for (auto preInstruction : cfg.getPredecessors(postInstruction)) {
        if (checkStep(preInstruction, postInstruction)) {
          result.insert(std::make_pair(preInstruction, postInstruction));
        }
      }
      // Check call edges.
      if (postInstruction ==
          &postInstruction->getParent()->getParent()->getEntryBlock().front()) {
        for (auto preInstruction :
             cg.getPossibleCallers(postInstruction->getParent()->getParent())) {
          if (checkStep(preInstruction, postInstruction)) {
            result.insert(std::make_pair(preInstruction, postInstruction));
          }
        }
      }
      // Check singular nodes.
      if (checkStep(NULL, postInstruction)) {
        result.insert(
            std::make_pair((llvm::Instruction *)NULL, postInstruction));
      }
    }

    return result;
  }

  std::set<llvm::Instruction *> toInstructionSet(CFG &cfg) {
    std::set<llvm::Instruction *> result;

    for (auto instruction : cfg) {
      // Check control flow nodes.
      if (checkInstruction(instruction)) {
        result.insert(instruction);
      }
    }

    return result;
  }

protected:
  virtual ~InstructionFilter() {}
};
}

#endif // #ifndef __InstructionFilter_H__
