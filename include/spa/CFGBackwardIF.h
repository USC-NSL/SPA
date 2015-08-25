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
  bool initialized = false;
  CFG cfg;
  CG cg;
  std::set<llvm::Instruction *> targets;
  std::map<llvm::Instruction *, bool> reaching;

  void init();

public:
  CFGBackwardIF(CFG &cfg, CG &cg, std::set<llvm::Instruction *> targets)
      : cfg(cfg), cg(cg), targets(targets) {}
  CFGBackwardIF(CFG &cfg, CG &cg, InstructionFilter *filter)
      : CFGBackwardIF(cfg, cg, filter->toInstructionSet(cfg)) {}
  bool checkStep(llvm::Instruction *preInstruction,
                 llvm::Instruction *postInstruction) {
    if (!initialized) {
      init();
    }
    return checkInstruction(postInstruction);
  }
  bool checkInstruction(llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    return (!reaching.count(instruction)) || reaching[instruction];
  }
  double getUtility(klee::ExecutionState *state);
  std::string getColor(CFG &cfg, CG &cg, llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    if (reaching.count(instruction) == 0)
      return "grey";
    else if (reaching[instruction])
      return "white";
    else
      return "dimgrey";
  }
};
}

#endif // #ifndef __CFGBackwardIF_H__
