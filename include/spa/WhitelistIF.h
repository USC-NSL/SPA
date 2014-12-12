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
  std::set<std::pair<llvm::Instruction *, llvm::Instruction *> > whitelist;

public:
  WhitelistIF() {}

  WhitelistIF(
      std::set<std::pair<llvm::Instruction *, llvm::Instruction *> > whitelist)
      : whitelist(whitelist) {}

  WhitelistIF(std::set<llvm::Instruction *> _whitelist) {
    for (auto it : _whitelist)
      whitelist.insert(std::make_pair((llvm::Instruction *)NULL, it));
  }

  bool checkStep(llvm::Instruction *preInstruction,
                 llvm::Instruction *postInstruction) {
    return whitelist.count(std::make_pair(preInstruction, postInstruction));
  }

  std::set<std::pair<llvm::Instruction *, llvm::Instruction *> > &
  getWhitelist() {
    return whitelist;
  }
};
}

#endif // #ifndef __WhitelistIF_H__
