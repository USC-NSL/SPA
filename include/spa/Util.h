/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>

#include <llvm/IR/Instruction.h>

namespace SPA {
  std::string debugLocation(llvm::Instruction *inst);
}

#endif // #ifndef __UTIL_H__
