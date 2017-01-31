/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PATH_H__
#define __PATH_H__

#include <map>

#include <klee/Constraints.h>
#include "klee/ExecutionState.h"
#include <klee/Solver.h>

#include <spa/SPA.h>

#define SPA_PATH_START "--- PATH START ---"
#define SPA_PATH_UUID_START "--- UUID START ---"
#define SPA_PATH_UUID_END "--- UUID END ---"
#define SPA_PATH_SYMBOLLOG_START "--- SYMBOL LOG START ---"
#define SPA_PATH_SYMBOLLOG_DELIMITER "	"
#define SPA_PATH_SYMBOLLOG_END "--- SYMBOL LOG END ---"
#define SPA_PATH_OUTPUTS_START "--- OUTPUTS START ---"
#define SPA_PATH_OUTPUT_DELIMITER "	"
#define SPA_PATH_OUTPUTS_END "--- OUTPUTS END ---"
#define SPA_PATH_TAGS_START "--- TAGS START ---"
#define SPA_PATH_TAG_DELIMITER "	"
#define SPA_PATH_TAGS_END "--- TAGS END ---"
#define SPA_PATH_KQUERY_START "--- KQUERY START ---"
#define SPA_PATH_KQUERY_END "--- KQUERY END ---"
#define SPA_PATH_EXPLOREDCOVERAGE_START "--- EXPLORED COVERAGE START ---"
#define SPA_PATH_EXPLOREDCOVERAGE_END "--- EXPLORED COVERAGE END ---"
#define SPA_PATH_EXPLOREDPATH_START "--- EXPLORED PATH START ---"
#define SPA_PATH_EXPLOREDPATH_END "--- EXPLORED PATH END ---"
#define SPA_PATH_TESTINPUTS_START "--- TEST INPUTS START ---"
#define SPA_PATH_TESTINPUTS_END "--- TEST INPUTS END ---"
#define SPA_PATH_TESTCOVERAGE_START "--- TEST COVERAGE START ---"
#define SPA_PATH_TESTCOVERAGE_END "--- TEST COVERAGE END ---"
#define SPA_PATH_END "--- PATH END ---"
#define SPA_PATH_COMMENT "#"
#define SPA_PATH_WHITE_SPACE " 	\r\n"

void loadCoverage(SPA::Path *path);

namespace SPA {
std::string strSplitJoin(std::string input, std::string delimiter, int lidx,
                         unsigned int rcut);
std::string generateUUID();

class Symbol {
private:
  std::string pathUUID;
  std::string derivedFromUUID;
  std::string fullName;
  const klee::Array *array = NULL;
  std::vector<klee::ref<klee::Expr> > outputValues;

public:
  Symbol(std::string pathUUID, std::string derivedFromUUID,
         std::string fullName, const klee::Array *array)
      : pathUUID(pathUUID), derivedFromUUID(derivedFromUUID),
        fullName(fullName), array(array) {}

  Symbol(std::string pathUUID, std::string derivedFromUUID,
         std::string fullName,
         std::vector<klee::ref<klee::Expr> > outputValues =
             std::vector<klee::ref<klee::Expr> >())
      : pathUUID(pathUUID), derivedFromUUID(derivedFromUUID),
        fullName(fullName), outputValues(outputValues) {}

  decltype(pathUUID) getPathUUID() const { return pathUUID; }
  decltype(derivedFromUUID) getDerivedFromUUID() const {
    return derivedFromUUID;
  }
  decltype(fullName) getFullName() const { return fullName; }
  std::string getQualifiedName() const {
    return strSplitJoin(fullName, SPA_SYMBOL_DELIMITER, 0, 1);
  }
  std::string getLocalName() const {
    return strSplitJoin(fullName, SPA_SYMBOL_DELIMITER, 0, 2);
  }
  std::string getParticipant() const {
    return strSplitJoin(fullName, SPA_SYMBOL_DELIMITER, -2, 1);
  }
  std::string getPrefix() const {
    return strSplitJoin(fullName, SPA_SYMBOL_DELIMITER, 2, 3);
  }
  std::string getMessage5Tuple() const {
    assert(isMessage());
    return strSplitJoin(fullName, SPA_SYMBOL_DELIMITER, -3, 2);
  }
  std::string getMessageSourceIP() const {
    return strSplitJoin(getMessage5Tuple(), ".", 0, 7);
  }
  std::string getMessageSourcePort() const {
    return strSplitJoin(getMessage5Tuple(), ".", 4, 6);
  }
  std::string getMessageProtocol() const {
    return strSplitJoin(getMessage5Tuple(), ".", 5, 5);
  }
  std::string getMessageDestinationIP() const {
    return strSplitJoin(getMessage5Tuple(), ".", -5, 1);
  }
  std::string getMessageDestinationPort() const {
    return strSplitJoin(getMessage5Tuple(), ".", -1, 0);
  }

  bool isInput() const {
    return fullName.compare(0, strlen(SPA_INPUT_PREFIX), SPA_INPUT_PREFIX) ==
               0 && array;
  }

  bool isOutput() const {
    return fullName.compare(0, strlen(SPA_OUTPUT_PREFIX), SPA_OUTPUT_PREFIX) ==
               0 && outputValues.size() > 0;
  }

  bool isInit() const {
    return fullName.compare(0, strlen(SPA_INIT_PREFIX), SPA_INIT_PREFIX) == 0 &&
           outputValues.size() > 0;
  }

  bool isAPI() const {
    if (isInput()) {
      return fullName.compare(0, strlen(SPA_API_INPUT_PREFIX),
                              SPA_API_INPUT_PREFIX) == 0;
    } else if (isOutput()) {
      return fullName.compare(0, strlen(SPA_API_OUTPUT_PREFIX),
                              SPA_API_OUTPUT_PREFIX) == 0;
    } else {
      assert(false && "Symbol is neither input nor output.");
    }
  }

  bool isModel() const {
    if (isInput()) {
      return fullName.compare(0, strlen(SPA_MODEL_INPUT_PREFIX),
                              SPA_MODEL_INPUT_PREFIX) == 0;
    } else if (isOutput()) {
      return fullName.compare(0, strlen(SPA_MODEL_OUTPUT_PREFIX),
                              SPA_MODEL_OUTPUT_PREFIX) == 0;
    } else {
      assert(false && "Symbol is neither input nor output.");
    }
  }

  bool isMessage() const {
    if (isInput()) {
      return fullName.compare(0, strlen(SPA_MESSAGE_INPUT_PREFIX),
                              SPA_MESSAGE_INPUT_PREFIX) == 0;
    } else if (isOutput()) {
      return fullName.compare(0, strlen(SPA_MESSAGE_OUTPUT_PREFIX),
                              SPA_MESSAGE_OUTPUT_PREFIX) == 0;
    } else {
      assert(false && "Symbol is neither input nor output.");
    }
  }

  bool isDropped() const {
    return isMessage() && isOutput() &&
           strSplitJoin(fullName, SPA_SYMBOL_DELIMITER, -4, 3) ==
               SPA_DROPPED_PREFIX;
  }

  const klee::Array *getInputArray() const { return array; }

  const std::vector<klee::ref<klee::Expr> > &getOutputValues() const {
    return outputValues;
  }

  friend class PathLoader;
  friend Path *buildDerivedPath(Path *basePath, Path *sourcePath);
};

class Path {
  friend void ::loadCoverage(Path *path);

private:
  std::string uuid = generateUUID();
  std::vector<std::shared_ptr<Symbol> > symbolLog;
  // qualified name -> [symbols]
  std::map<std::string, std::vector<std::shared_ptr<Symbol> > > inputSymbols;
  // qualified name -> [symbols]
  std::map<std::string, std::vector<std::shared_ptr<Symbol> > > outputSymbols;
  std::map<std::string, std::string> tags;
  klee::ConstraintManager constraints;
  // participant -> (file -> (line -> covered))
  std::map<std::string, std::map<std::string, std::map<long, bool> > >
      exploredLineCoverage;
  // participant -> (function -> covered)
  std::map<std::string, std::map<std::string, bool> > exploredFunctionCoverage;
  // participant -> [location, branch]
  std::map<std::string, std::vector<std::pair<std::string, bool> > >
      exploredPath;
  // full name -> [bytes]
  std::map<std::string, std::vector<uint8_t> > testInputs;
  // participant -> (file -> (line -> covered))
  std::map<std::string, std::map<std::string, std::map<long, bool> > >
      testLineCoverage;
  // participant -> (function -> covered)
  std::map<std::string, std::map<std::string, bool> > testFunctionCoverage;

  static bool isFunctionCovered(
      std::string fn,
      std::map<std::string, std::map<std::string, bool> > &coverage,
      std::string participant);
  static bool isLineCovered(
      std::string dbgStr,
      std::map<std::string, std::map<std::string, std::map<long, bool> > > &
          coverage,
      std::string participant);

public:
  Path() {}
  Path(klee::ExecutionState *kState, klee::Solver *solver);

  const decltype(uuid) getUUID() const { return uuid; }

  const std::string getDerivedFromUUID() const {
    if (symbolLog.empty()) {
      return "";
    } else {
      return symbolLog.back()->getDerivedFromUUID();
    }
  }

  const std::string getParentUUID() const {
    if (symbolLog.empty()) {
      return "";
    }
    for (auto it = symbolLog.rbegin(), ie = symbolLog.rend(); it != ie; it++) {
      if ((*it)->getPathUUID() != symbolLog.back()->getPathUUID()) {
        return (*it)->getPathUUID();
      }
    }
    return "";
  }

  const std::string getParticipantParentUUID() const {
    if (symbolLog.empty()) {
      return "";
    }
    for (auto it = symbolLog.rbegin(), ie = symbolLog.rend(); it != ie; it++) {
      if ((*it)->getParticipant() != symbolLog.back()->getParticipant()) {
        return (*it)->getPathUUID();
      }
    }
    return "";
  }

  const decltype(symbolLog) & getSymbolLog() const { return symbolLog; }

  const decltype(inputSymbols) & getInputSymbols() const {
    return inputSymbols;
  }

  const decltype(outputSymbols) & getOutputSymbols() const {
    return outputSymbols;
  }

  decltype(tags) getTags() const { return tags; }

  const klee::ConstraintManager &getConstraints() const { return constraints; }

  bool isCovered(std::string dbgStr);

  decltype(exploredPath) getExploredPath() const { return exploredPath; }

  const decltype(testInputs) & getTestInputs() const { return testInputs; }

  const std::vector<uint8_t> &getTestInput(std::string name) const {
    if (testInputs.count(name)) {
      return testInputs.find(name)->second;
    } else {
      static std::vector<uint8_t> empty;
      return empty;
    }
  }

  decltype(exploredLineCoverage) getExploredLineCoverage() {
    return exploredLineCoverage;
  }

  decltype(exploredFunctionCoverage) getExploredFunctionCoverage() {
    return exploredFunctionCoverage;
  }

  decltype(testLineCoverage) getTestLineCoverage() { return testLineCoverage; }

  decltype(testFunctionCoverage) getTestFunctionCoverage() {
    return testFunctionCoverage;
  }

  std::string getPathSource() const;

  friend class PathLoader;
  friend std::ostream &operator<<(std::ostream &stream, const Path &path);
  friend Path *buildDerivedPath(Path *basePath, Path *sourcePath);
};

std::ostream &operator<<(std::ostream &stream, const Path &path);
}

#endif // #ifndef __PATH_H__
