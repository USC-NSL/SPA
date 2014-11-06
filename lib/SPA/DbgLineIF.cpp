/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/DbgLineIF.h"

#include "spa/Util.h"

typedef enum {
  AT,
  BEFORE,
  AFTER,
  REACHING,
  NOT_REACHING,
  KEYWORD_MAX
} prefix_t;

std::string prefixes[] = { "AT ", "BEFORE ", "AFTER ", "REACHING ",
                           "NOT REACHING " };

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

  // Parse from "prefix dir/file:line"
  prefix_t prefix = AT;
  for (int i = 0; i < KEYWORD_MAX; i++) {
    if (dbgPoint.compare(0, prefixes[i].length(), prefixes[i]) == 0) {
      prefix = (prefix_t) i;
      dbgPoint = dbgPoint.substr(prefixes[i].length());
    }
  }

  auto b = dbgPoint.find("/");
  std::string dbgDir;
  if (b != std::string::npos) {
    dbgDir = dbgPoint.substr(0, b);
    dbgPoint = dbgPoint.substr(b + 1);
  }

  b = dbgPoint.find(":");
  assert(b != std::string::npos && "Must specify file name and line number.");
  std::string dbgFile = dbgPoint.substr(0, b);
  long dbgLineNo = SPA::strToNum<long>(dbgPoint.substr(b + 1));

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

  assert(prefix == AT && "Not implemented.");
}
}
