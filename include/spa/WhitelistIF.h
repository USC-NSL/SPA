/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __WhitelistIF_H__
#define __WhitelistIF_H__

#include <set>

#include <llvm/IR/Instruction.h>

#include <spa/InstructionFilter.h>

namespace SPA {
class WhitelistIF : public InstructionFilter {
protected:
  std::set<llvm::Instruction *> whitelist;

public:
  WhitelistIF(std::set<llvm::Instruction *> _whitelist =
                  std::set<llvm::Instruction *>())
      : whitelist(_whitelist) {}
  bool checkInstruction(llvm::Instruction *instruction) {
    return whitelist.count(instruction);
  }

  std::set<llvm::Instruction *> getWhitelist() { return whitelist; }
};
}

#endif // #ifndef __WhitelistIF_H__
