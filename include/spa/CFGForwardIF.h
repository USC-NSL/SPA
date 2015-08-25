/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CFGForwardIF_H__
#define __CFGForwardIF_H__

#include <set>

#include <spa/CFG.h>
#include <spa/CG.h>
#include <spa/InstructionFilter.h>

namespace SPA {
class CFGForwardIF : public InstructionFilter {
private:
  bool initialized = false;
  CFG cfg;
  CG cg;
  std::set<llvm::Instruction *> startingPoints;
  std::set<llvm::Instruction *> filterOut;

  void init();

public:
  CFGForwardIF(CFG &cfg, CG &cg, std::set<llvm::Instruction *> startingPoints)
      : cfg(cfg), cg(cg), startingPoints(startingPoints) {}
  bool checkInstruction(llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    return filterOut.count(instruction) == 0;
  }
};
}

#endif // #ifndef __CFGForwardIF_H__
