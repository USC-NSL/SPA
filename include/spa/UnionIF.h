/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __UnionIF_H__
#define __UnionIF_H__

#include <set>

#include <spa/InstructionFilter.h>

namespace SPA {
class UnionIF : public InstructionFilter {
private:
  std::set<InstructionFilter *> subFilters;

public:
  UnionIF() {}

  UnionIF(std::set<InstructionFilter *> _subFilters)
      : subFilters(_subFilters) {}

  void addIF(InstructionFilter *instructionFilter) {
    subFilters.insert(instructionFilter);
  }

  std::set<InstructionFilter *> getSubFilters() { return subFilters; }

  bool checkStep(llvm::Instruction *preInstruction,
                 llvm::Instruction *postInstruction) {
    for (auto it : subFilters) {
      if (it->checkStep(preInstruction, postInstruction)) {
        return true;
      }
    }
    return false;
  }

  bool checkInstruction(llvm::Instruction *instruction) {
    for (auto it : subFilters) {
      if (it->checkInstruction(instruction)) {
        return true;
      }
    }
    return false;
  }
};
}

#endif // #ifndef __UnionIF_H__
