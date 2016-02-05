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
    participants = state.senderPath->participants;
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

  participants.emplace_back(new Participant(ParticipantName, uuid));

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

      if (qualifiedName.compare(0, strlen(SPA_INPUT_PREFIX),
                                SPA_INPUT_PREFIX) == 0) {
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
    if (iit.first.compare(0, strlen(SPA_API_INPUT_PREFIX),
                          SPA_API_INPUT_PREFIX) == 0) {
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

Path *Path::buildDerivedPath(Path *basePath, Path *sourcePath,
                             klee::Solver *solver) {
  // Don't augment the root path.
  if (basePath->symbolLog.empty() || sourcePath->symbolLog.empty()) {
    return NULL;
  }
  // Find commonalities in each path's participant and symbol logs.
  // Position of first divergent participant entry.
  unsigned long commonParticipants;
  // Position of first new source symbol log entry.
  auto newSourceLog = sourcePath->symbolLog.begin();
  for (commonParticipants = 0;
       commonParticipants < basePath->getParticipants().size() &&
           commonParticipants < sourcePath->getParticipants().size();
       commonParticipants++) {
    if (basePath->getParticipants()[commonParticipants]->getPathUUID() ==
        sourcePath->getParticipants()[commonParticipants]->getPathUUID()) {
      while (newSourceLog != sourcePath->symbolLog.end() &&
             (*newSourceLog)->getParticipant() ==
                 basePath->getParticipants()[commonParticipants]->getName()) {
        newSourceLog++;
      }
    } else {
      break;
    }
  }
  // Does base have something new that source would be adding to?
  // Otherwise destination becomes the same as source.
  if (commonParticipants == basePath->getParticipants().size()) {
    return NULL;
  }
  // Only consider scenario where source is ahead by a single participant.
  if (sourcePath->getParticipants().size() - commonParticipants != 1) {
    return NULL;
  }
  // Check if the source contributor is already in the new part of base.
  for (auto i = commonParticipants; i < basePath->getParticipants().size();
       i++) {
    if (basePath->getParticipants()[i]->getName() ==
        sourcePath->getParticipants()[commonParticipants]->getName()) {
      return NULL;
    }
  }

  Path *destinationPath = new Path();

  // AND together the path-constraints then get test-case to check if SAT.
  // Unknowns to solve for.
  std::vector<std::string> objectNames;
  std::vector<const klee::Array *> objects;
  // Big AND.
  for (auto cit : basePath->constraints) {
    if (!destinationPath->constraints.addAndCheckConstraint(cit)) {
      delete destinationPath;
      return NULL;
    }
  }
  for (auto cit : sourcePath->constraints) {
    if (!destinationPath->constraints.addAndCheckConstraint(cit)) {
      delete destinationPath;
      return NULL;
    }
  }
  // Connect API inputs.
  for (auto bsit = basePath->symbolLog.begin(),
            ssit = sourcePath->symbolLog.begin();
       ssit != newSourceLog; bsit++, ssit++) {
    if ((*bsit)->isInput() && (*bsit)->isAPI()) {
      assert((*bsit)->getFullName() == (*ssit)->getFullName() &&
             (*bsit)->getInputArray()->size == (*ssit)->getInputArray()->size &&
             "Common part of log is not in sync.");
      objectNames.push_back((*bsit)->getFullName());
      objects.push_back((*bsit)->getInputArray());
      //       *const_cast<std::string *>(&(*ssit)->getInputArray()->name) +=
      // "_dummy";
      for (size_t offset = 0; offset < (*bsit)->getInputArray()->size;
           offset++) {
        klee::UpdateList bul((*bsit)->getInputArray(), 0);
        klee::UpdateList sul((*ssit)->getInputArray(), 0);
        llvm::OwningPtr<klee::ExprBuilder> exprBuilder(
            klee::createDefaultExprBuilder());
        klee::ref<klee::Expr> e = exprBuilder->Eq(
            exprBuilder->Read(bul,
                              exprBuilder->Constant(offset, klee::Expr::Int32)),
            exprBuilder->Read(
                sul, exprBuilder->Constant(offset, klee::Expr::Int32)));
        if (!destinationPath->constraints.addAndCheckConstraint(e)) {
          delete destinationPath;
          return NULL;
        }
      }
    }
  }
  // Solve for API inputs.
  std::vector<std::vector<unsigned char> > result;
  if (solver->getInitialValues(
          klee::Query(destinationPath->constraints,
                      klee::createDefaultExprBuilder()->False()),
          objects, result)) {
    for (size_t i = 0; i < result.size(); i++) {
      destinationPath->testInputs[objectNames[i]] = result[i];
    }
  } else {
    delete destinationPath;
    return NULL;
  }

  // Append new data from source to base.
  // New participant.
  destinationPath->participants = basePath->participants;
  destinationPath->participants.emplace_back(new Participant(
      sourcePath->participants.back()->getName(), destinationPath->uuid));
  // New symbol log entries.
  destinationPath->symbolLog = basePath->symbolLog;
  destinationPath->symbolLog.insert(destinationPath->symbolLog.end(),
                                    newSourceLog, sourcePath->symbolLog.end());
  // New symbols.
  destinationPath->inputSymbols = basePath->inputSymbols;
  destinationPath->outputSymbols = basePath->outputSymbols;
  for (; newSourceLog != sourcePath->symbolLog.end(); newSourceLog++) {
    if ((*newSourceLog)->isInput()) {
      destinationPath->inputSymbols[(*newSourceLog)->getQualifiedName()]
          .push_back(*newSourceLog);
    } else if ((*newSourceLog)->isOutput()) {
      destinationPath->outputSymbols[(*newSourceLog)->getQualifiedName()]
          .push_back(*newSourceLog);
    } else {
      assert(false && "Symbol is neither input nor output.");
    }
  }
  // Tags.
  destinationPath->tags = sourcePath->tags;
  destinationPath->tags.insert(basePath->tags.begin(), basePath->tags.end());
  // Explored line coverage.
  destinationPath->exploredLineCoverage = basePath->exploredLineCoverage;
  for (auto fit : sourcePath->exploredLineCoverage) {
    for (auto lit : fit.second) {
      destinationPath->exploredLineCoverage[fit.first][lit.first] |= lit.second;
    }
  }
  // Explored function coverage.
  destinationPath->exploredFunctionCoverage =
      basePath->exploredFunctionCoverage;
  for (auto fit : sourcePath->exploredFunctionCoverage) {
    destinationPath->exploredFunctionCoverage[fit.first] |= fit.second;
  }
  // Explored path.
  destinationPath->exploredPath = basePath->exploredPath;
  destinationPath
      ->exploredPath[sourcePath->getParticipants().back()->getName()] =
          sourcePath
      ->exploredPath[sourcePath->getParticipants().back()->getName()];
  // Test line coverage.
  destinationPath->testLineCoverage = basePath->testLineCoverage;
  for (auto fit : sourcePath->testLineCoverage) {
    for (auto lit : fit.second) {
      destinationPath->testLineCoverage[fit.first][lit.first] |= lit.second;
    }
  }
  // Test function coverage.
  destinationPath->testFunctionCoverage = basePath->testFunctionCoverage;
  for (auto fit : sourcePath->testFunctionCoverage) {
    destinationPath->testFunctionCoverage[fit.first] |= fit.second;
  }

  return destinationPath;
}

bool Path::isFunctionCovered(std::string fn,
                             std::map<std::string, bool> &coverage) {
  return coverage.count(fn) && coverage[fn];
}

bool
Path::isLineCovered(std::string dbgStr,
                    std::map<std::string, std::map<long, bool> > &coverage) {
  // Interpret as dir/file:line
  std::string dbgPath = dbgStr.substr(0, dbgStr.find(":"));
  long dbgLineNo = strToNum<long>(dbgStr.substr(dbgPath.length() + 1));

  for (auto covPath : coverage) {
    if (pathPrefixMatch(covPath.first, dbgPath)) {
      return covPath.second.count(dbgLineNo) && covPath.second[dbgLineNo];
    }
  }
  return false;
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

std::string Path::getPathSource() const {
  std::string result = SPA_PATH_START "\n";
  result += SPA_PATH_UUID_START "\n";
  result += uuid + "\n";
  result += SPA_PATH_UUID_END "\n";

  result += SPA_PATH_PARTICIPANTS_START "\n";
  for (auto it : participants) {
    result += it->getName() + SPA_PATH_PARTICIPANT_DELIMITER +
              it->getPathUUID() + "\n";
  }
  result += SPA_PATH_PARTICIPANTS_END "\n";

  if (!derivedFromUUID.empty()) {
    result += SPA_PATH_DERIVEDFROMUUID_START "\n";
    result += derivedFromUUID + "\n";
    result += SPA_PATH_DERIVEDFROMUUID_END "\n";
  }

  result += SPA_PATH_SYMBOLLOG_START "\n";
  for (auto it : symbolLog) {
    result += it->getFullName() + "\n";
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
      &evalExprs[0] + evalExprs.size(), NULL, NULL, true);
  kleaverROS.flush();
  result += kleaverROS.str();
  result += SPA_PATH_KQUERY_END "\n";

  if ((!exploredLineCoverage.empty()) || (!exploredFunctionCoverage.empty())) {
    result += SPA_PATH_EXPLOREDCOVERAGE_START "\n";
    for (auto srcFile : exploredLineCoverage) {
      result += srcFile.first;
      for (auto line : srcFile.second) {
        result += (line.second ? " " : " !") + numToStr(line.first);
      }
      result += "\n";
    }
    for (auto fn : exploredFunctionCoverage) {
      result += (fn.second ? "" : "!") + fn.first + "\n";
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
    for (auto srcFile : testLineCoverage) {
      result += srcFile.first;
      for (auto line : srcFile.second) {
        result += (line.second ? " " : " !") + numToStr(line.first);
      }
      result += "\n";
    }
    for (auto fn : testFunctionCoverage) {
      result += (fn.second ? "" : "!") + fn.first + "\n";
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
