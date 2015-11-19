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

std::string __attribute__((weak)) debugLocation(const llvm::Instruction *inst) {
  if (llvm::MDNode *node = inst->getMetadata("dbg")) {
    llvm::DILocation loc(node);
    return loc.getDirectory().str() + "/" + loc.getFilename().str() + ":" +
           numToStr<long>(loc.getLineNumber());
  } else {
    return "(unknown)";
  }
}

bool __attribute__((weak))
    pathPrefixMatch(std::string longPath, std::string shortPath) {
  auto delim = longPath.rfind("/");
  std::string longDir =
      delim == std::string::npos ? "" : longPath.substr(0, delim);
  std::string longFile =
      delim == std::string::npos ? longPath : longPath.substr(delim + 1);
  delim = shortPath.rfind("/");
  std::string shortDir =
      delim == std::string::npos ? "" : shortPath.substr(0, delim);
  std::string shortFile =
      delim == std::string::npos ? shortPath : shortPath.substr(delim + 1);

  // Exact match file.
  // shortDir must be a suffix of longDir, with care that it comes right
  // after a / or matches exactly.
  return shortFile == longFile && longDir.length() >= shortDir.length() &&
         longDir.substr(longDir.length() - shortDir.length()) == shortDir &&
         (shortDir == "" || shortDir.length() == longDir.length() ||
          longDir[longDir.length() - shortDir.length() - 1] == '/');
}
}

#endif // #ifndef __UTIL_H__
