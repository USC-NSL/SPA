/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Support/CommandLine.h"
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

#include <spa/Util.h>

#include <spa/Path.h>

namespace SPA {
extern llvm::cl::opt<std::string> Participant;

Path::Path(klee::ExecutionState *kState, klee::Solver *solver) {
  klee::ExecutionState state(*kState);

  // Load data from sender path.
  if (state.senderPath) {
    participants = state.senderPath->participants;
    symbolLog = state.senderPath->symbolLog;
    tags = state.senderPath->tags;
    exploredLineCoverage = state.senderPath->exploredLineCoverage;
    exploredFunctionCoverage = state.senderPath->exploredFunctionCoverage;
    exploredPath = state.senderPath->exploredPath;
    testLineCoverage = state.senderPath->testLineCoverage;
    testFunctionCoverage = state.senderPath->testFunctionCoverage;
  }

  participants.push_back(Participant);

  std::map<uint64_t, std::pair<const klee::MemoryObject *,
                               const klee::Array *> > orderedSymbols;
  for (auto it : state.symbolics) {
    std::string name = it.second->name;
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

      tags[name.substr(strlen(SPA_TAG_PREFIX))] = std::string(buf);
      delete buf;
    } else if (name.compare(0, strlen(SPA_INPUT_PREFIX), SPA_INPUT_PREFIX) ==
               0 || name.compare(0, strlen(SPA_OUTPUT_PREFIX),
                                 SPA_OUTPUT_PREFIX) == 0) {
      uint64_t id =
          strToNum<uint64_t>(name.substr(name.rfind(SPA_SYMBOL_DELIMITER) + 1));
      assert(orderedSymbols.count(id) == 0 &&
             "Repeated symbol sequence number.");
      orderedSymbols[id] = it;
    }
  }

  for (auto it : orderedSymbols) {
    std::string fullName = it.second.second->name;
    std::string qualifiedName =
        fullName.substr(0, fullName.rfind(SPA_SYMBOL_DELIMITER));

    if (qualifiedName.compare(0, strlen(SPA_INPUT_PREFIX), SPA_INPUT_PREFIX) ==
        0) {
      std::shared_ptr<Symbol> s(new Symbol(fullName, it.second.second));
      symbolLog.push_back(s);
      inputSymbols[qualifiedName].push_back(s);
    } else if (qualifiedName.compare(0, strlen(SPA_OUTPUT_PREFIX),
                                     SPA_OUTPUT_PREFIX) == 0 ||
               qualifiedName.compare(0, strlen(SPA_INIT_PREFIX),
                                     SPA_INIT_PREFIX) == 0) {
      if (const klee::ObjectState *os =
              state.addressSpace.findObject(it.second.first)) {
        std::vector<klee::ref<klee::Expr> > outputValues;
        for (unsigned int i = 0; i < os->size; i++) {
          outputValues.push_back(os->read8(i));
        }
        std::shared_ptr<Symbol> s(new Symbol(fullName, outputValues));
        symbolLog.push_back(s);
        outputSymbols[qualifiedName].push_back(s);
      }
    }
  }

  // Add non-colliding symbols from sender path.
  for (auto it : state.senderPath->inputSymbols) {
    if (inputSymbols.count(it.first) == 0) {
      inputSymbols[it.first] = it.second;
    }
  }
  for (auto it : state.senderPath->outputSymbols) {
    if (outputSymbols.count(it.first) == 0) {
      outputSymbols[it.first] = it.second;
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

  exploredPath[Participant].clear();
  for (auto branchDecision : state.branchDecisions) {
    exploredPath[Participant].push_back(std::make_pair(
        debugLocation(branchDecision.first), branchDecision.second));
  }

  // Unknowns to solve for,
  std::vector<std::string> objectNames;
  std::vector<const klee::Array *> objects;
  // Process inputs.
  for (auto iit : inputSymbols) {
    // Check if API input.
    if (iit.first.compare(0, strlen(SPA_API_INPUT_PREFIX),
                          SPA_API_INPUT_PREFIX) == 0) {
      for (auto sit : iit.second) {
        objectNames.push_back(sit->getName());
        objects.push_back(sit->getInputArray());
      }
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
  stream << SPA_PATH_UUID_START << std::endl;
  stream << path.uuid << std::endl;
  stream << SPA_PATH_UUID_END << std::endl;

  stream << SPA_PATH_PARTICIPANTS_START << std::endl;
  for (auto it : path.participants) {
    stream << it << std::endl;
  }
  stream << SPA_PATH_PARTICIPANTS_END << std::endl;

  stream << SPA_PATH_SYMBOLLOG_START << std::endl;
  for (auto it : path.symbolLog) {
    stream << it->getName() << std::endl;
  }
  stream << SPA_PATH_SYMBOLLOG_END << std::endl;

  stream << SPA_PATH_OUTPUTS_START << std::endl;
  std::vector<klee::ref<klee::Expr> > evalExprs;
  for (auto oit : path.outputSymbols) {
    for (auto sit : oit.second) {
      stream << sit->getName() << SPA_PATH_OUTPUT_DELIMITER
             << sit->getOutputValues().size() << std::endl;
      for (auto bit : sit->getOutputValues()) {
        evalExprs.push_back(bit);
      }
    }
  }
  stream << SPA_PATH_OUTPUTS_END << std::endl;

  stream << SPA_PATH_TAGS_START << std::endl;
  for (auto it : path.tags) {
    stream << it.first << SPA_PATH_TAG_DELIMITER << it.second << std::endl;
  }
  stream << SPA_PATH_TAGS_END << std::endl;

  stream << SPA_PATH_KQUERY_START << std::endl;
  klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
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
