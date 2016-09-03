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
extern llvm::cl::opt<std::string> ParticipantName;

Path::Path(klee::ExecutionState *kState, klee::Solver *solver) {
  klee::ExecutionState state(*kState);

  // Load data from sender path.
  if (state.senderPath) {
    symbolLog = state.senderPath->symbolLog;
    inputSymbols = state.senderPath->inputSymbols;
    outputSymbols = state.senderPath->outputSymbols;
    tags = state.senderPath->tags;
    exploredLineCoverage = state.senderPath->exploredLineCoverage;
    exploredFunctionCoverage = state.senderPath->exploredFunctionCoverage;
    exploredPath = state.senderPath->exploredPath;
    testLineCoverage = state.senderPath->testLineCoverage;
    testFunctionCoverage = state.senderPath->testFunctionCoverage;
  }

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

  std::set<std::string> symbolsInSenderLog;
  for (auto it : symbolLog) {
    symbolsInSenderLog.insert(it->getFullName());
  }

  for (auto it : orderedSymbols) {
    std::string fullName = it.second.second->name;
    if (symbolsInSenderLog.count(fullName) == 0) {
      std::string qualifiedName =
          fullName.substr(0, fullName.rfind(SPA_SYMBOL_DELIMITER));
      std::string localName =
          qualifiedName.substr(0, qualifiedName.rfind(SPA_SYMBOL_DELIMITER));

      if (qualifiedName.compare(0, strlen(SPA_INPUT_PREFIX),
                                SPA_INPUT_PREFIX) == 0) {
        std::shared_ptr<Symbol> s(
            new Symbol(uuid, "", fullName, it.second.second));
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
            outputValues.push_back(
                state.constraints.simplifyExpr(os->read8(i)));
          }
          std::shared_ptr<Symbol> s(
              new Symbol(uuid, "", fullName, outputValues));
          symbolLog.push_back(s);
          outputSymbols[qualifiedName].push_back(s);
        }
      }
    }
  }

  constraints = state.constraints;

  for (auto inst : state.instructionCoverage) {
    std::string srcLoc = debugLocation(inst);
    std::string srcFile = srcLoc.substr(0, srcLoc.find(":"));
    long srcLineNum = strToNum<long>(srcLoc.substr(srcLoc.find(":") + 1));
    exploredLineCoverage[ParticipantName][srcFile][srcLineNum] = true;
    exploredFunctionCoverage[ParticipantName][
        inst->getParent()->getParent()->getName().str()] = true;
  }
  for (llvm::Function &fn :
       *state.pc->inst->getParent()->getParent()->getParent()) {
    for (llvm::BasicBlock &bb : fn) {
      for (llvm::Instruction &inst : bb) {
        std::string srcLoc = debugLocation(&inst);
        std::string srcFile = srcLoc.substr(0, srcLoc.find(":"));
        long srcLineNum = strToNum<long>(srcLoc.substr(srcLoc.find(":") + 1));

        if (!exploredLineCoverage[ParticipantName][srcFile].count(srcLineNum)) {
          exploredLineCoverage[ParticipantName][srcFile][srcLineNum] = false;
        }
        if (!exploredFunctionCoverage[ParticipantName]
                .count(inst.getParent()->getParent()->getName().str())) {
          exploredFunctionCoverage[ParticipantName][
              inst.getParent()->getParent()->getName().str()] = false;
        }
      }
    }
  }

  exploredPath[ParticipantName].clear();
  for (auto branchDecision : state.branchDecisions) {
    exploredPath[ParticipantName].push_back(std::make_pair(
        debugLocation(branchDecision.first), branchDecision.second));
  }

  // Unknowns to solve for,
  std::vector<std::string> objectNames;
  std::vector<const klee::Array *> objects;
  // Process inputs.
  for (auto iit : inputSymbols) {
    // Check if API input.
    if (iit.first.compare(0, strlen(SPA_INPUT_PREFIX), SPA_INPUT_PREFIX) == 0) {
      for (auto sit : iit.second) {
        objectNames.push_back(sit->getFullName());
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

bool Path::isFunctionCovered(
    std::string fn,
    std::map<std::string, std::map<std::string, bool> > &coverage,
    std::string participant) {
  if (participant.empty()) {
    for (auto participantCov : coverage) {
      if (participantCov.second.count(fn) && participantCov.second[fn]) {
        return true;
      }
    }
    return false;
  } else {
    return coverage.count(participant) && coverage[participant].count(fn) &&
           coverage[participant][fn];
  }
}

bool Path::isLineCovered(
    std::string dbgStr,
    std::map<std::string, std::map<std::string, std::map<long, bool> > > &
        coverage,
    std::string participant) {
  // Interpret as dir/file:line
  std::string dbgPath = dbgStr.substr(0, dbgStr.find(":"));
  long dbgLineNo = strToNum<long>(dbgStr.substr(dbgPath.length() + 1));

  if (participant.empty()) {
    for (auto participantCov : coverage) {
      for (auto covPath : participantCov.second) {
        if (pathPrefixMatch(covPath.first, dbgPath) &&
            covPath.second.count(dbgLineNo) && covPath.second[dbgLineNo]) {
          return true;
        }
      }
    }
    return false;
  } else {
    if (coverage.count(participant)) {
      for (auto covPath : coverage[participant]) {
        if (pathPrefixMatch(covPath.first, dbgPath)) {
          return covPath.second.count(dbgLineNo) && covPath.second[dbgLineNo];
        }
      }
    }
  }
  return false;
}

bool Path::isCovered(std::string dbgStr) {
  std::string participant;
  auto d = dbgStr.find("@");
  if (d != std::string::npos) {
    participant = dbgStr.substr(0, d);
    dbgStr = dbgStr.substr(d + 1);
  }

  if (dbgStr.find(":") == std::string::npos) {
    // Interpret as function.
    if (!testFunctionCoverage.empty()) {
      return isFunctionCovered(dbgStr, testFunctionCoverage, participant);
    } else if (!exploredFunctionCoverage.empty()) {
      return isFunctionCovered(dbgStr, exploredFunctionCoverage, participant);
    } else {
      assert(false && "No coverage data.");
    }
  } else {
    // Interpret as dir/file:line
    if (!testLineCoverage.empty()) {
      return isLineCovered(dbgStr, testLineCoverage, participant);
    } else if (!exploredLineCoverage.empty()) {
      return isLineCovered(dbgStr, exploredLineCoverage, participant);
    } else {
      assert(false && "No coverage data.");
    }
  }
}

std::string Path::getPathSource() const {
  std::string result = SPA_PATH_START "\n";
  result += SPA_PATH_UUID_START "\n";
  result += uuid + "\n";
  result += SPA_PATH_UUID_END "\n";

  result += SPA_PATH_SYMBOLLOG_START "\n";
  std::vector<const klee::Array *> evalArrays;
  for (auto it : symbolLog) {
    result += it->getPathUUID() + SPA_PATH_SYMBOLLOG_DELIMITER +
              it->getDerivedFromUUID() + SPA_PATH_SYMBOLLOG_DELIMITER +
              it->getFullName() + "\n";

    if (it->isInput()) {
      evalArrays.push_back(it->getInputArray());
    }
  }
  result += SPA_PATH_SYMBOLLOG_END "\n";

  result += SPA_PATH_OUTPUTS_START "\n";
  std::vector<klee::ref<klee::Expr> > evalExprs;
  for (auto oit : outputSymbols) {
    for (auto sit : oit.second) {
      result += sit->getFullName() + SPA_PATH_OUTPUT_DELIMITER +
                numToStr(sit->getOutputValues().size()) + "\n";
      for (auto bit : sit->getOutputValues()) {
        evalExprs.push_back(bit);
      }
    }
  }
  result += SPA_PATH_OUTPUTS_END "\n";

  result += SPA_PATH_TAGS_START "\n";
  for (auto it : tags) {
    result += it.first + SPA_PATH_TAG_DELIMITER + it.second + "\n";
  }
  result += SPA_PATH_TAGS_END "\n";

  result += SPA_PATH_KQUERY_START "\n";
  klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();

  std::string kleaverStr;
  llvm::raw_string_ostream kleaverROS(kleaverStr);
  klee::ExprPPrinter::printQuery(
      kleaverROS, constraints, exprBuilder->False(), &evalExprs[0],
      &evalExprs[0] + evalExprs.size(), &evalArrays[0],
      &evalArrays[0] + evalArrays.size(), true);
  kleaverROS.flush();
  result += kleaverROS.str();
  result += SPA_PATH_KQUERY_END "\n";

  if ((!exploredLineCoverage.empty()) || (!exploredFunctionCoverage.empty())) {
    result += SPA_PATH_EXPLOREDCOVERAGE_START "\n";
    for (auto participant : exploredLineCoverage) {
      for (auto srcFile : participant.second) {
        result += participant.first + " " + srcFile.first;
        for (auto line : srcFile.second) {
          result += (line.second ? " " : " !") + numToStr(line.first);
        }
        result += "\n";
      }
    }
    for (auto participant : exploredFunctionCoverage) {
      for (auto fn : participant.second) {
        result +=
            participant.first + (fn.second ? " " : " !") + fn.first + "\n";
      }
    }
    result += SPA_PATH_EXPLOREDCOVERAGE_END "\n";
  }

  if (!exploredPath.empty()) {
    result += SPA_PATH_EXPLOREDPATH_START "\n";
    for (auto moduleIt : exploredPath) {
      result += moduleIt.first + "\n";
      for (auto pathIt : moduleIt.second) {
        result += pathIt.first + " " + numToStr(pathIt.second) + "\n";
      }
    }
    result += SPA_PATH_EXPLOREDPATH_END "\n";
  }

  result += SPA_PATH_TESTINPUTS_START "\n";
  for (auto input : testInputs) {
    result += input.first;
    for (uint8_t b : input.second) {
      result += " " + numToStrHex(b);
    }
    result += "\n";
  }
  result += SPA_PATH_TESTINPUTS_END "\n";

  if ((!testLineCoverage.empty()) || (!testFunctionCoverage.empty())) {
    result += SPA_PATH_TESTCOVERAGE_START "\n";
    for (auto participant : testLineCoverage) {
      for (auto srcFile : participant.second) {
        result += participant.first + " " + srcFile.first;
        for (auto line : srcFile.second) {
          result += (line.second ? " " : " !") + numToStr(line.first);
        }
        result += "\n";
      }
    }
    for (auto participant : testFunctionCoverage) {
      for (auto fn : participant.second) {
        result +=
            participant.first + (fn.second ? " " : " !") + fn.first + "\n";
      }
    }
    result += SPA_PATH_TESTCOVERAGE_END "\n";
  }

  result += SPA_PATH_END "\n";
  return result;
}

std::ostream &operator<<(std::ostream &stream, const Path &path) {
  stream << path.getPathSource();
  stream.flush();
  return stream;
}

}
