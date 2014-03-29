/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <sstream>

#include <llvm/IR/Instruction.h>
#include <llvm/DebugInfo.h>

namespace SPA {
  std::string to_string(int n) {
    std::stringstream ss;
    ss << n;
    return ss.str();
  }

  std::string debugLocation(llvm::Instruction *inst) {
    if (llvm::MDNode *node = inst->getMetadata("dbg")) {
      llvm::DILocation loc(node);
      return loc.getDirectory().str() + "/" + loc.getFilename().str() + ":" + to_string(loc.getLineNumber());
    } else {
      return "(unknown)";
    }
  }
}
