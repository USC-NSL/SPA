/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/DbgLineIF.h"

#include "spa/CFG.h"
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
  // Parse from "prefix {dir/file:line | function}"
  prefix_t prefix = AT;
  for (int i = 0; i < KEYWORD_MAX; i++) {
    if (dbgPoint.compare(0, prefixes[i].length(), prefixes[i]) == 0) {
      prefix = (prefix_t) i;
      dbgPoint = dbgPoint.substr(prefixes[i].length());
    }
  }

  std::set<llvm::Instruction *> dbgInsts;
  // Check if specified point is a function.
  if (llvm::Function *fn = module->getFunction(dbgPoint)) {
    for (llvm::BasicBlock &bb : *fn) {
      for (llvm::Instruction &inst : bb) {
        dbgInsts.insert(&inst);
      }
    }
  } else {
    auto b = dbgPoint.find("/");
    std::string dbgDir;
    if (b != std::string::npos) {
      dbgDir = dbgPoint.substr(0, b);
      dbgPoint = dbgPoint.substr(b + 1);
    }

    b = dbgPoint.find(":");
    assert(b != std::string::npos &&
           "Must either specify file name and line number or a function.");
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
            std::string instPath = loc.getFilename().str();
            if ((!instPath.empty()) && instPath[0] != '/') {
              instPath = loc.getDirectory().str() + "/" + instPath;
            }

            std::string instDir = instPath.substr(0, instPath.rfind("/"));
            std::string instFile = instPath.substr(instPath.rfind("/") + 1);
            long instLineNo = loc.getLineNumber();

            // Exact match file and line number.
            // dbgDir must be a suffix of instDir, with care that it comes right
            // after a / or matches exactly.
            if (instFile == dbgFile && instLineNo == dbgLineNo &&
                instDir.length() >= dbgDir.length() &&
                instDir.substr(instDir.length() - dbgDir.length()) == dbgDir &&
                (dbgDir == "" || instDir.length() == dbgDir.length() ||
                 instDir[instDir.length() - dbgDir.length() - 1] == '/')) {
              dbgInsts.insert(&inst);

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
  assert((!dbgInsts.empty()) && "Found no instructions matching criteria.");

  switch (prefix) {
  case AT: {
    whitelist = dbgInsts;
  } break;

  case BEFORE: {
    CFG cfg(module);
    for (auto inst : cfg) {
      if (dbgInsts.count(inst) == 0 && !cfg.getSuccessors(inst).empty()) {
        bool isCandidate = dbgInsts.count(*cfg.getSuccessors(inst).begin()) > 0;
        for (auto successor : cfg.getSuccessors(inst)) {
          assert(
              (dbgInsts.count(successor) > 0) == isCandidate &&
              "Instruction has successors both in and out of specified set.");
        }
        if (isCandidate) {
          whitelist.insert(inst);
        }
      }
    }
  } break;

  case AFTER: {
    CFG cfg(module);
    for (auto inst : cfg) {
      if (dbgInsts.count(inst) == 0 && !cfg.getPredecessors(inst).empty()) {
        bool isCandidate =
            dbgInsts.count(*cfg.getPredecessors(inst).begin()) > 0;
        for (auto successor : cfg.getPredecessors(inst)) {
          assert(
              (dbgInsts.count(successor) > 0) == isCandidate &&
              "Instruction has predecessors both in and out of specified set.");
        }
        if (isCandidate) {
          whitelist.insert(inst);
        }
      }
    }
  } break;

  case REACHING: {
    assert(false && "Not implemented.");
  } break;

  case NOT_REACHING: {
    assert(false && "Not implemented.");
  } break;

  default: {
    assert(false && "Unknown prefix.");
  } break;
  }
}

bool DbgLineIF::checkSyntax(std::string dbgPoint) {
  assert(false && "Not implemented.");
}
}
