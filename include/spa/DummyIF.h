/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __DummyIF_H__
#define __DummyIF_H__

#include <set>

#include <llvm/IR/Instruction.h>

#include <spa/InstructionFilter.h>

namespace SPA {
class DummyIF : public InstructionFilter {
public:
  DummyIF() {}
  bool checkInstruction(llvm::Instruction *instruction) { return true; }
};
}

#endif // #ifndef __DummyIF_H__
