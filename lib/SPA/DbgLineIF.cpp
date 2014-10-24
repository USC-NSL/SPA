/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/DbgLineIF.h"

#include "spa/Util.h"

namespace SPA {
DbgLineIF::DbgLineIF(llvm::Module *module, std::string dbgPoint) {
  // Check if specified point is a function.
  if (llvm::Function *fn = module->getFunction(dbgPoint)) {
    for (llvm::BasicBlock &bb : *fn) {
      for (llvm::Instruction &inst : bb) {
        whitelist.insert(&inst);
      }
    }
    return;
  }

  // Parse from dir/file:line
  auto dirFileBoundary = dbgPoint.find("/");
  auto fileLineBoundary = dbgPoint.find(":");
  assert(fileLineBoundary != std::string::npos &&
         "Must specify file name and line number.");

  if (dirFileBoundary == std::string::npos) {
    dirFileBoundary = -1;
  }
  assert(dirFileBoundary < fileLineBoundary &&
         "Must specify file name and line number.");

  std::string dbgDir =
      dirFileBoundary > 0 ? dbgPoint.substr(0, dirFileBoundary) : "";
  std::string dbgFile = dbgPoint.substr(dirFileBoundary + 1,
                                        fileLineBoundary - dirFileBoundary - 1);
  long dbgLineNo = SPA::strToNum<long>(dbgPoint.substr(fileLineBoundary + 1));

  // Canonical debug location of what is found (used to ensure uniqueness).
  std::string foundDbgLoc;

  // Iterate over all instructions in module.
  for (llvm::Function &fn : *module) {
    for (llvm::BasicBlock &bb : fn) {
      for (llvm::Instruction &inst : bb) {
        // Check if instruction's debug location matches.
        if (llvm::MDNode *node = inst.getMetadata("dbg")) {
          llvm::DILocation loc(node);
          std::string instDir = loc.getDirectory().str();
          std::string instFile = loc.getFilename().str();
          long instLineNo = loc.getLineNumber();

          // Exact match file and line number.
          // dbgDir must be a suffix of instDir, with care that it comes right
          // after a / or matches exactly.
          if (instFile == dbgFile && instLineNo == dbgLineNo &&
              instDir.length() >= dbgDir.length() &&
              instDir.substr(instDir.length() - dbgFile.length()) == dbgFile &&
              (instDir.length() == dbgDir.length() ||
               instDir[instDir.length() - dbgDir.length() - 1] == '/')) {
            whitelist.insert(&inst);

            if (foundDbgLoc.empty()) {
              foundDbgLoc = SPA::debugLocation(&inst);
            } else {
              assert(foundDbgLoc == SPA::debugLocation(&inst) &&
                     "Multiple distinct file locations match the specified "
                     "criteria (files with same name, different directory).");
            }
          }
        }
      }
    }
  }
  assert((!foundDbgLoc.empty()) &&
         "Specified filename/line criteria didn't match any known location.");
}
}
