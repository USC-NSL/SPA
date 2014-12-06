/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CFGBackwardIF_H__
#define __CFGBackwardIF_H__

#include <set>

#include <spa/CFG.h>
#include <spa/CG.h>
#include <spa/InstructionFilter.h>

namespace SPA {
class CFGBackwardIF : public InstructionFilter, public StateUtility {
private:
  std::map<llvm::Instruction *, bool> reaching;

public:
  CFGBackwardIF(CFG &cfg, CG &cg, std::set<llvm::Instruction *> targets);
  CFGBackwardIF(CFG &cfg, CG &cg, InstructionFilter *filter)
      : CFGBackwardIF(cfg, cg, filter->toInstructionSet(cfg)) {}
  bool checkStep(llvm::Instruction *preInstruction,
                 llvm::Instruction *postInstruction);
  bool checkInstruction(llvm::Instruction *instruction);
  double getUtility(klee::ExecutionState *state);
  std::string getColor(CFG &cfg, CG &cg, llvm::Instruction *instruction);
};
}

#endif // #ifndef __CFGBackwardIF_H__
