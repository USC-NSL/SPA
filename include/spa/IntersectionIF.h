/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __IntersectionIF_H__
#define __IntersectionIF_H__

#include <set>

#include <spa/InstructionFilter.h>

namespace SPA {
class IntersectionIF : public InstructionFilter {
private:
  std::set<InstructionFilter *> subFilters;

public:
  IntersectionIF() {}
  IntersectionIF(std::set<InstructionFilter *> _subFilters)
      : subFilters(_subFilters) {}
  void addIF(InstructionFilter *instructionFilter);
  bool checkInstruction(llvm::Instruction *instruction);
};
}

#endif // #ifndef __IntersectionIF_H__
