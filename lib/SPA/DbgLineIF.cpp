/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/DbgLineIF.h"

#include "spa/CFG.h"
#include "spa/CFGBackwardIF.h"
#include "spa/NegatedIF.h"
#include "spa/Util.h"

typedef enum {
  BEFORE,
  AFTER,
  REACHING,
  NOT_REACHING,
  SUCCEEDING,
  KEYWORD_MAX
} prefix_t;

std::string prefixes[] = { "BEFORE ", "AFTER ", "REACHING ", "NOT REACHING ",
                           "SUCCEEDING " };

namespace SPA {
DbgLineIF *DbgLineIF::parse(llvm::Module *module, std::string dbgPoint) {
  // Parse from "prefix {dir/file:line | function}"
  prefix_t prefix = BEFORE;
  for (int i = 0; i < KEYWORD_MAX; i++) {
    if (dbgPoint.compare(0, prefixes[i].length(), prefixes[i]) == 0) {
      prefix = (prefix_t) i;
      dbgPoint = dbgPoint.substr(prefixes[i].length());
      break;
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
    auto delim = dbgPoint.find(":");
    // Must either specify file name and line number or a function.
    if (delim == std::string::npos)
      return NULL;
    std::string dbgPath = dbgPoint.substr(0, delim);
    long dbgLineNo = strToNum<long>(dbgPoint.substr(delim + 1));

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
            long instLineNo = loc.getLineNumber();

            if (pathPrefixMatch(instPath, dbgPath) && instLineNo == dbgLineNo) {
              dbgInsts.insert(&inst);

              if (foundDbgLoc.empty()) {
                foundDbgLoc = debugLocation(&inst);
              } else {
                // Check if multiple distinct file locations match the specified
                // criteria (files with same name, different directory).
                if (foundDbgLoc != debugLocation(&inst))
                  return NULL;
              }
            }
          }
        }
      }
    }
    // Check if specified filename/line criteria matched a known location.
    if (foundDbgLoc.empty())
      return NULL;
  }
  // Check if any instructions matched criteria.
  if (dbgInsts.empty())
    return NULL;

  switch (prefix) {
  case BEFORE: {
    return new DbgLineIF(dbgInsts, true);
  } break;

  case AFTER: {
    return new DbgLineIF(dbgInsts, false);
  } break;

  case REACHING: {
    CFG cfg(module);
    std::set<llvm::Instruction *> reachingSet = dbgInsts;
    // Initialize worklist as predecessors of specification.
    std::set<llvm::Instruction *> worklist;
    for (auto inst : dbgInsts) {
      auto predecessors = cfg.getPredecessors(inst);
      worklist.insert(predecessors.begin(), predecessors.end());
    }
    // Process worklist.
    while (!worklist.empty()) {
      auto inst = *worklist.begin();
      worklist.erase(inst);

      // Check if instruction is fully succeeded by reaching set.
      bool reaching = true;
      for (auto successor : cfg.getSuccessors(inst)) {
        if (!reachingSet.count(successor)) {
          reaching = false;
          break;
        }
      }
      if (reaching) {
        reachingSet.insert(inst);
        auto predecessors = cfg.getPredecessors(inst);
        worklist.insert(predecessors.begin(), predecessors.end());
      }
    }
    return new DbgLineIF(reachingSet, true);
  } break;

  case NOT_REACHING: {
    CFG cfg(module);
    CG cg(module);
    CFGBackwardIF reachable(cfg, cg, dbgInsts);
    return new DbgLineIF(reachable.toInstructionSet(cfg), false);
  } break;

  case SUCCEEDING: {
    CFG cfg(module);
    std::set<llvm::Instruction *> succeedingSet;
    // Initialize worklist as successors of specification.
    std::set<llvm::Instruction *> worklist;
    for (auto inst : dbgInsts) {
      auto successors = cfg.getSuccessors(inst);
      worklist.insert(successors.begin(), successors.end());
    }
    // Process worklist.
    while (!worklist.empty()) {
      auto inst = *worklist.begin();
      worklist.erase(inst);

      // Check if instruction is fully preceeded by specification or succeeding
      // set.
      bool succeeding = (dbgInsts.count(inst) == 0);
      if (succeeding) {
        for (auto predecessor : cfg.getPredecessors(inst)) {
          if ((!succeedingSet.count(predecessor)) &&
              (!dbgInsts.count(predecessor))) {
            succeeding = false;
            break;
          }
        }
      }
      if (succeeding) {
        succeedingSet.insert(inst);
        auto successors = cfg.getSuccessors(inst);
        worklist.insert(successors.begin(), successors.end());
      }
    }
    return new DbgLineIF(succeedingSet, true);
  } break;

  default: {
    // Unknown prefix.
    return NULL;
  } break;
  }
}
}
