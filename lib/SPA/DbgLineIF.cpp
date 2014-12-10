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
  KEYWORD_MAX
} prefix_t;

std::string prefixes[] = { "BEFORE ", "AFTER ", "REACHING ", "NOT REACHING " };

namespace SPA {
DbgLineIF *DbgLineIF::parse(llvm::Module *module, std::string dbgPoint) {
  // Parse from "prefix {dir/file:line | function}"
  prefix_t prefix = BEFORE;
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

    // Must either specify file name and line number or a function.
    if ((b = dbgPoint.find(":")) == std::string::npos)
      return NULL;
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
                // Check if multiple distinct file locations match the specified
                // criteria (files with same name, different directory).
                if (foundDbgLoc != SPA::debugLocation(&inst))
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

  std::set<std::pair<llvm::Instruction *, llvm::Instruction *> > whitelist;

  switch (prefix) {
  case BEFORE: {
    CFG cfg(module);
    // Whitelist all edges with just a tail in the specification.
    for (auto postInstruction : cfg) {
      // Check control flow edges.
      for (auto preInstruction : cfg.getPredecessors(postInstruction)) {
        if ((!dbgInsts.count(preInstruction)) &&
            dbgInsts.count(postInstruction)) {
          whitelist.insert(std::make_pair(preInstruction, postInstruction));
        }
      }
      // Check call edges.
      if (postInstruction ==
          &postInstruction->getParent()->getParent()->getEntryBlock().front()) {
        whitelist.insert(
            std::make_pair((llvm::Instruction *)NULL, postInstruction));
      }
    }
  } break;

  case AFTER: {
    CFG cfg(module);
    // Whitelist all edges with only the tail in the specification.
    for (auto preInstruction : cfg) {
      // Check control flow edges.
      for (auto postInstruction : cfg.getSuccessors(preInstruction)) {
        if (dbgInsts.count(preInstruction) &&
            (!dbgInsts.count(postInstruction))) {
          whitelist.insert(std::make_pair(preInstruction, postInstruction));
        }
      }
      // Check call edges.
      if (cfg.getSuccessors(preInstruction).empty()) {
        whitelist.insert(
            std::make_pair(preInstruction, (llvm::Instruction *)NULL));
      }
    }
  } break;

  case REACHING: {
    CFG cfg(module);
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

      // Check if instruction is fully succeeded either by whitelist or
      // specification.
      bool reaching = true;
      for (auto successor : cfg.getSuccessors(inst)) {
        if (dbgInsts.count(successor) == 0 &&
            whitelist.count(
                std::make_pair((llvm::Instruction *)NULL, successor)) == 0) {
          reaching = false;
          break;
        }
      }
      if (reaching) {
        whitelist.insert(std::make_pair((llvm::Instruction *)NULL, inst));
        auto predecessors = cfg.getPredecessors(inst);
        worklist.insert(predecessors.begin(), predecessors.end());
      }
    }
  } break;

  case NOT_REACHING: {
    CFG cfg(module);
    CG cg(module);
    CFGBackwardIF reachable(cfg, cg, dbgInsts);
    NegatedIF nonreaching(&reachable);
    for (auto inst : cfg) {
      if (nonreaching.checkInstruction(inst)) {
        whitelist.insert(std::make_pair((llvm::Instruction *)NULL, inst));
      }
    }
  } break;

  default: {
    // Unknown prefix.
    return NULL;
  } break;
  }

  return new DbgLineIF(whitelist);
}
}
