/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>
#include <sstream>

#include <llvm/IR/Instruction.h>
#include <llvm/DebugInfo.h>

namespace SPA {
template <typename T> std::string numToStr(T n) {
  std::stringstream ss;
  ss << n;
  return ss.str();
}

template <typename T> T strToNum(const std::string &s) {
  std::stringstream ss(s);
  T result;
  return ss >> result ? result : 0;
}

std::string __attribute__((weak)) debugLocation(llvm::Instruction *inst) {
  if (llvm::MDNode *node = inst->getMetadata("dbg")) {
    llvm::DILocation loc(node);
    return loc.getDirectory().str() + "/" + loc.getFilename().str() + ":" +
           numToStr<long>(loc.getLineNumber());
  } else {
    return "(unknown)";
  }
}
}

#endif // #ifndef __UTIL_H__
