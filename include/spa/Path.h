/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PATH_H__
#define __PATH_H__

#include <map>

#include <klee/Constraints.h>
#include "klee/ExecutionState.h"
#include <klee/Solver.h>

#define SPA_PATH_START "--- PATH START ---"
#define SPA_PATH_SYMBOLS_START "--- SYMBOLS START ---"
#define SPA_PATH_SYMBOL_DELIMITER "	"
#define SPA_PATH_SYMBOLS_END "--- SYMBOLS END ---"
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
#define SPA_PATH_PARTICIPANTS_START "--- PARTICIPANTS START ---"
#define SPA_PATH_PARTICIPANTS_END "--- PARTICIPANTS END ---"
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
class Path {
  friend void ::loadCoverage(SPA::Path *path);

private:
  std::map<std::string, const klee::Array *> symbols;
  std::map<std::string, std::vector<klee::ref<klee::Expr> > > outputValues;
  std::map<std::string, std::string> tags;
  klee::ConstraintManager constraints;
  std::map<std::string, std::map<long, bool> > exploredLineCoverage;
  std::map<std::string, bool> exploredFunctionCoverage;
  std::vector<std::string> participants;
  std::map<std::string, std::vector<std::pair<std::string, bool> > >
      exploredPath;
  std::map<std::string, std::vector<uint8_t> > testInputs;
  std::map<std::string, std::map<long, bool> > testLineCoverage;
  std::map<std::string, bool> testFunctionCoverage;

  Path();
  static bool isFunctionCovered(std::string fn,
                                std::map<std::string, bool> &coverage);
  static bool
      isLineCovered(std::string dbgStr,
                    std::map<std::string, std::map<long, bool> > &coverage);

public:
  Path(klee::ExecutionState *kState, klee::Solver *solver);

  const klee::Array *getSymbol(std::string name) const {
    return symbols.count(name) ? symbols.find(name)->second : NULL;
  }

  bool hasOutput(std::string name) const {
    return outputValues.count(name) > 0;
  }

  size_t getOutputSize(std::string name) const {
    return outputValues.count(name) ? outputValues.find(name)->second.size()
                                    : 0;
  }

  const klee::ref<klee::Expr> getOutputValue(std::string name,
                                             unsigned int offset) const {
    assert(offset < getOutputSize(name) && "Symbol offset out of bounds.");
    return outputValues.find(name)->second[offset];
  }

  const std::map<std::string, const klee::Array *> getSymbols() const {
    return symbols;
  }

  std::map<std::string, std::vector<klee::ref<klee::Expr> > >::const_iterator
  beginOutputs() {
    return outputValues.begin();
  }

  std::map<std::string, std::vector<klee::ref<klee::Expr> > >::const_iterator
  endOutputs() {
    return outputValues.end();
  }

  std::string getTag(std::string key) const {
    return tags.count(key) ? tags.find(key)->second : std::string();
  }

  const klee::ConstraintManager &getConstraints() const { return constraints; }

  bool isCovered(std::string dbgStr);

  std::vector<std::string> getParticipants() const { return participants; }

  std::map<std::string, std::vector<std::pair<std::string, bool> > >
  getExploredPath() const {
    return exploredPath;
  }

  const std::map<std::string, std::vector<uint8_t> > &getTestInputs() const {
    return testInputs;
  }

  const std::vector<uint8_t> &getTestInput(std::string name) const {
    if (testInputs.count(name)) {
      return testInputs.find(name)->second;
    } else {
      static std::vector<uint8_t> empty;
      return empty;
    }
  }

  std::map<std::string, std::map<long, bool> > getExploredLineCoverage() {
    return exploredLineCoverage;
  }

  std::map<std::string, bool> getExploredFunctionCoverage() {
    return exploredFunctionCoverage;
  }

  std::map<std::string, std::map<long, bool> > getTestLineCoverage() {
    return testLineCoverage;
  }

  std::map<std::string, bool> getTestFunctionCoverage() {
    return testFunctionCoverage;
  }

  friend class PathLoader;
  friend std::ostream &operator<<(std::ostream &stream, const Path &path);
};

std::ostream &operator<<(std::ostream &stream, const Path &path);
}

#endif // #ifndef __PATH_H__
