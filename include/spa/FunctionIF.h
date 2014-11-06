/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __FunctionIF_H__
#define __FunctionIF_H__

#include <set>

#include <spa/CFG.h>
#include <spa/CG.h>
#include <spa/InstructionFilter.h>

namespace SPA {
class FunctionIF : public InstructionFilter {
private:
  llvm::Function *function;

public:
  FunctionIF(llvm::Function *_function) : function(_function);
  bool checkInstruction(llvm::Instruction *instruction) {
    return instruction->getParent()->getParent() == function
  }
  ;
};
}

#endif // #ifndef __FunctionIF_H__
