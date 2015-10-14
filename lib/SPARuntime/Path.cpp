/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <llvm/Support/raw_ostream.h>
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/MemoryBuffer.h"

#include "../Core/Common.h"
#include "../../lib/Core/Memory.h"
#include "klee/ExecutionState.h"
#include "klee/ExprBuilder.h"
#include "klee/util/ExprPPrinter.h"
#include <expr/Parser.h>
#include <klee/Solver.h>

#include <spa/SPA.h>
#include <spa/Util.h>

#include <spa/Path.h>

namespace SPA {
Path::Path() {}

Path::Path(klee::ExecutionState *kState, klee::Solver *solver) {
  klee::ExecutionState state(*kState);

  // Load data from sender path.
  if (state.senderPath) {
    tags = state.senderPath->tags;
    exploredLineCoverage = state.senderPath->exploredLineCoverage;
    exploredFunctionCoverage = state.senderPath->exploredFunctionCoverage;
    participants = state.senderPath->getParticipants();
    exploredPath = state.senderPath->exploredPath;
    testLineCoverage = state.senderPath->testLineCoverage;
    testFunctionCoverage = state.senderPath->testFunctionCoverage;

    // Rename colliding inputs.
    for (auto it : state.senderPath->symbols) {
      if (state.arrayNames.count(it.first)) {
        std::string newName;
        int s = 2;
        while (
            state.arrayNames.count((newName = it.first + numToStr<int>(s))) ||
            state.senderPath->getSymbol((newName)) ||
            state.senderPath->hasOutput(newName)) {
          s++;
        }
        klee::klee_message(
            "Renaming colliding symbol from input path: %s -> %s",
            it.first.c_str(), newName.c_str());
        *const_cast<std::string *>(&it.second->name) = newName;
      }
    }

    // Merge outputs.
    for (auto it = state.senderPath->beginOutputs(),
              ie = state.senderPath->endOutputs();
         it != ie; it++) {
      if (!state.arrayNames.count(it->first)) {
        outputValues[it->first] = it->second;
      } else {
        std::string newName;
        int s = 2;
        while (
            state.arrayNames.count((newName = it->first + numToStr<int>(s))) ||
            state.senderPath->getSymbol((newName)) ||
            state.senderPath->hasOutput(newName)) {
          s++;
        }
        klee::klee_message(
            "Renaming colliding output from input path: %s -> %s",
            it->first.c_str(), newName.c_str());
        outputValues[newName] = it->second;
      }
    }
  }

  for (auto it : state.symbolics) {
    std::string name = it.first->name;

    if (name.compare(0, strlen(SPA_TAG_PREFIX), SPA_TAG_PREFIX) == 0) {
      const klee::ObjectState *addrOS = state.addressSpace.findObject(it.first);
      assert(addrOS && "Tag not set.");

      klee::ref<klee::Expr> addrExpr =
          addrOS->read(0, klee::Context::get().getPointerWidth());
      assert(isa<klee::ConstantExpr>(addrExpr) && "Tag address is symbolic.");
      klee::ref<klee::ConstantExpr> address =
          cast<klee::ConstantExpr>(addrExpr);
      klee::ObjectPair op;
      assert(state.addressSpace.resolveOne(address, op) &&
             "Tag address is not uniquely defined.");
      const klee::MemoryObject *mo = op.first;
      const klee::ObjectState *os = op.second;

      char *buf = new char[mo->size];
      unsigned ioffset = 0;
      klee::ref<klee::Expr> offset_expr =
          klee::SubExpr::create(address, op.first->getBaseExpr());
      assert(isa<klee::ConstantExpr>(offset_expr) &&
             "Tag is an invalid string.");
      klee::ref<klee::ConstantExpr> value =
          cast<klee::ConstantExpr>(offset_expr.get());
      ioffset = value.get()->getZExtValue();
      assert(ioffset < mo->size);

      unsigned i;
      for (i = 0; i < mo->size - ioffset - 1; i++) {
        klee::ref<klee::Expr> cur = os->read8(i + ioffset);
        assert(isa<klee::ConstantExpr>(cur) &&
               "Symbolic character in tag value.");
        buf[i] = cast<klee::ConstantExpr>(cur)->getZExtValue(8);
      }
      buf[i] = 0;

      name = name.substr(strlen(SPA_TAG_PREFIX));
      tags[name] = std::string(buf);
      // 				klee_message( "	Tag: " << name << " = " << buf );
      delete buf;
    } else {
      symbols[name] = it.second;

      // Symbolic value.
      if (name.compare(0, strlen(SPA_OUTPUT_PREFIX), SPA_OUTPUT_PREFIX) == 0 ||
          name.compare(0, strlen(SPA_STATE_PREFIX), SPA_STATE_PREFIX) == 0 ||
          name.compare(0, strlen(SPA_INIT_PREFIX), SPA_INIT_PREFIX) == 0)
        if (const klee::ObjectState *os =
                state.addressSpace.findObject(it.first))
          for (unsigned int i = 0; i < os->size; i++)
            outputValues[name].push_back(os->read8(i));
    }
  }

  llvm::raw_null_ostream rnos;
  klee::PPrinter p(rnos);
  for (auto it : state.constraints) {
    p.scan(it);
  }
  for (const klee::Array *a : p.usedArrays) {
    if (symbols.count(a->name) == 0) {
      symbols[a->name] = a;
    }
  }

  constraints = state.constraints;

  for (auto inst : state.instructionCoverage) {
    std::string srcLoc = debugLocation(inst);
    std::string srcFile = srcLoc.substr(0, srcLoc.find(":"));
    long srcLineNum = strToNum<long>(srcLoc.substr(srcLoc.find(":") + 1));
    exploredLineCoverage[srcFile][srcLineNum] = true;
    exploredFunctionCoverage[inst->getParent()->getParent()->getName().str()] =
        true;
  }
  for (llvm::Function &fn :
       *state.pc->inst->getParent()->getParent()->getParent()) {
    for (llvm::BasicBlock &bb : fn) {
      for (llvm::Instruction &inst : bb) {
        std::string srcLoc = debugLocation(&inst);
        std::string srcFile = srcLoc.substr(0, srcLoc.find(":"));
        long srcLineNum = strToNum<long>(srcLoc.substr(srcLoc.find(":") + 1));

        if (!exploredLineCoverage[srcFile].count(srcLineNum)) {
          exploredLineCoverage[srcFile][srcLineNum] = false;
        }
        if (!exploredFunctionCoverage.count(
                inst.getParent()->getParent()->getName().str())) {
          exploredFunctionCoverage[
              inst.getParent()->getParent()->getName().str()] = false;
        }
      }
    }
  }

  std::string moduleName = state.pc->inst->getParent()->getParent()->getParent()
      ->getModuleIdentifier();
  participants.push_back(moduleName);

  exploredPath[moduleName].clear();
  for (auto branchDecision : state.branchDecisions) {
    exploredPath[moduleName].push_back(std::make_pair(
        debugLocation(branchDecision.first), branchDecision.second));
  }

  // Unknowns to solve for,
  std::vector<std::string> objectNames;
  std::vector<const klee::Array *> objects;
  // Process inputs.
  for (auto it : symbols) {
    // Check if API input.
    if (it.first.compare(0, strlen(SPA_INPUT_PREFIX), SPA_INPUT_PREFIX) == 0) {
      objectNames.push_back(it.first);
      objects.push_back(it.second);
    }
  }

  std::vector<std::vector<unsigned char> > result;
  if (solver->getInitialValues(
          klee::Query(constraints, klee::createDefaultExprBuilder()->False()),
          objects, result)) {
    for (size_t i = 0; i < result.size(); i++) {
      testInputs[objectNames[i]] = result[i];
    }
  }
}

bool Path::isFunctionCovered(std::string fn,
                             std::map<std::string, bool> &coverage) {
  assert(coverage.count(fn) && "No coverage data for specified function.");
  return coverage[fn];
}

bool
Path::isLineCovered(std::string dbgStr,
                    std::map<std::string, std::map<long, bool> > &coverage) {
  // Interpret as dir/file:line
  std::string dbgPath = dbgStr.substr(0, dbgStr.find(":"));
  long dbgLineNo = strToNum<long>(dbgStr.substr(dbgPath.length() + 1));

  for (auto covPath : coverage) {
    if (pathPrefixMatch(covPath.first, dbgPath)) {
      assert(covPath.second.count(dbgLineNo) &&
             "No coverage data for specified source line.");
      return covPath.second[dbgLineNo];
    }
  }
  assert(false && "No coverage data for specified file.");
}

bool Path::isCovered(std::string dbgStr) {
  if (dbgStr.find(":") == std::string::npos) {
    // Interpret as function.
    if (!testFunctionCoverage.empty()) {
      return isFunctionCovered(dbgStr, testFunctionCoverage);
    } else if (!exploredFunctionCoverage.empty()) {
      return isFunctionCovered(dbgStr, exploredFunctionCoverage);
    } else {
      assert(false && "No coverage data.");
    }
  } else {
    // Interpret as dir/file:line
    if (!testLineCoverage.empty()) {
      return isLineCovered(dbgStr, testLineCoverage);
    } else if (!exploredLineCoverage.empty()) {
      return isLineCovered(dbgStr, exploredLineCoverage);
    } else {
      assert(false && "No coverage data.");
    }
  }
}

std::ostream &operator<<(std::ostream &stream, const Path &path) {
  stream << SPA_PATH_START << std::endl;
  stream << SPA_PATH_OUTPUTS_START << std::endl;
  for (auto it : path.outputValues) {
    stream << it.first << SPA_PATH_OUTPUT_DELIMITER << it.second.size()
           << std::endl;
  }
  stream << SPA_PATH_OUTPUTS_END << std::endl;

  stream << SPA_PATH_TAGS_START << std::endl;
  for (auto it : path.tags) {
    stream << it.first << SPA_PATH_TAG_DELIMITER << it.second << std::endl;
  }
  stream << SPA_PATH_TAGS_END << std::endl;

  stream << SPA_PATH_KQUERY_START << std::endl;
  klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
  std::vector<klee::ref<klee::Expr> > evalExprs;
  for (auto it : path.outputValues) {
    for (auto it2 : it.second) {
      evalExprs.push_back(it2);
    }
  }
  llvm::raw_os_ostream ros(stream);
  klee::ExprPPrinter::printQuery(
      ros, path.getConstraints(), exprBuilder->False(), &evalExprs[0],
      &evalExprs[0] + evalExprs.size(), NULL, NULL, true);
  ros.flush();
  stream << SPA_PATH_KQUERY_END << std::endl;

  if ((!path.exploredLineCoverage.empty()) ||
      (!path.exploredFunctionCoverage.empty())) {
    stream << SPA_PATH_EXPLOREDCOVERAGE_START << std::endl;
    for (auto srcFile : path.exploredLineCoverage) {
      stream << srcFile.first;
      for (auto line : srcFile.second) {
        stream << " " << (line.second ? "" : "!") << line.first;
      }
      stream << std::endl;
    }
    for (auto fn : path.exploredFunctionCoverage) {
      stream << (fn.second ? "" : "!") << fn.first << std::endl;
    }
    stream << SPA_PATH_EXPLOREDCOVERAGE_END << std::endl;
  }

  if (!path.getParticipants().empty()) {
    stream << SPA_PATH_PARTICIPANTS_START << std::endl;
    for (auto it : path.getParticipants()) {
      stream << it << std::endl;
    }
    stream << SPA_PATH_PARTICIPANTS_END << std::endl;
  }

  if (!path.getExploredPath().empty()) {
    stream << SPA_PATH_EXPLOREDPATH_START << std::endl;
    for (auto moduleIt : path.getExploredPath()) {
      stream << moduleIt.first << std::endl;
      for (auto pathIt : moduleIt.second) {
        stream << pathIt.first << " " << pathIt.second << std::endl;
      }
    }
    stream << SPA_PATH_EXPLOREDPATH_END << std::endl;
  }

  stream << SPA_PATH_TESTINPUTS_START << std::endl;
  for (auto input : path.getTestInputs()) {
    stream << input.first << std::hex;
    for (uint8_t b : input.second) {
      stream << " " << (int) b;
    }
    stream << std::dec << std::endl;
  }
  stream << SPA_PATH_TESTINPUTS_END << std::endl;

  if ((!path.testLineCoverage.empty()) ||
      (!path.testFunctionCoverage.empty())) {
    stream << SPA_PATH_TESTCOVERAGE_START << std::endl;
    for (auto srcFile : path.testLineCoverage) {
      stream << srcFile.first;
      for (auto line : srcFile.second) {
        stream << " " << (line.second ? "" : "!") << line.first;
      }
      stream << std::endl;
    }
    for (auto fn : path.testFunctionCoverage) {
      stream << (fn.second ? "" : "!") << fn.first << std::endl;
    }
    stream << SPA_PATH_TESTCOVERAGE_END << std::endl;
  }

  return stream << SPA_PATH_END << std::endl;
}
}
