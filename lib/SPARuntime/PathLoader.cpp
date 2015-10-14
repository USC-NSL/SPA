/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <list>
#include <sstream>

#include "llvm/Support/MemoryBuffer.h"

#include <../Core/Common.h>
#include "../Core/Memory.h"
#include "klee/ExprBuilder.h"
#include <expr/Parser.h>

#include <spa/PathLoader.h>
#include <spa/Util.h>

#define changeState(from, to)                                                  \
  if (state != from) {                                                         \
    klee::klee_message("Invalid path file. Error near line %ld.", lineNumber); \
    assert(false && "Invalid path file.");                                     \
  }                                                                            \
  state = to;

namespace {
typedef enum {
  START,
  PATH,
  SYMBOLS,
  OUTPUTS,
  TAGS,
  KQUERY,
  EXPLOREDCOVERAGE,
  PARTICIPANTS,
  EXPLOREDPATH,
  TESTINPUTS,
  TESTCOVERAGE
} LoadState_t;
}

namespace SPA {
std::string cleanUpLine(std::string line) {
  // Remove comments.
  line = line.substr(0, line.find(SPA_PATH_COMMENT));
  // Remove trailing white space.
  line = line.substr(0, line.find_last_not_of(SPA_PATH_WHITE_SPACE) + 1);
  // Remove leading white space.
  if (line.find_first_not_of(SPA_PATH_WHITE_SPACE) != line.npos)
    line = line.substr(line.find_first_not_of(SPA_PATH_WHITE_SPACE), line.npos);

  return line;
}

std::vector<std::string> split(std::string str, std::string delimiter) {
  std::vector<std::string> result;
  size_t p;
  while ((p = str.find(delimiter)) != std::string::npos) {
    result.push_back(str.substr(0, p));
    str = str.substr(p + delimiter.size());
  }
  result.push_back(str);

  return result;
}

Path *PathLoader::getPath() {
  // Save current position in case of failure.
  unsigned long checkpointLN = lineNumber;
  std::streampos checkpointPos = input.tellg();

  LoadState_t state = START;
  Path *path = NULL;
  std::map<std::string, std::string> arrayToName;
  std::vector<std::pair<std::string, size_t> > outputSizes;
  std::string kQuery;
  while (input.good()) {
    std::string line;
    getline(input, line);
    lineNumber++;
    line = cleanUpLine(line);
    if (line.empty())
      continue;

    if (line == SPA_PATH_START) {
      changeState(START, PATH);
      path = new Path();
      arrayToName.clear();
      outputSizes.clear();
      kQuery = "";
    } else if (line == SPA_PATH_SYMBOLS_START) {
      changeState(PATH, SYMBOLS);
    } else if (line == SPA_PATH_SYMBOLS_END) {
      changeState(SYMBOLS, PATH);
    } else if (line == SPA_PATH_OUTPUTS_START) {
      changeState(PATH, OUTPUTS);
    } else if (line == SPA_PATH_OUTPUTS_END) {
      changeState(OUTPUTS, PATH);
    } else if (line == SPA_PATH_TAGS_START) {
      changeState(PATH, TAGS);
    } else if (line == SPA_PATH_TAGS_END) {
      changeState(TAGS, PATH);
    } else if (line == SPA_PATH_KQUERY_START) {
      changeState(PATH, KQUERY);
    } else if (line == SPA_PATH_KQUERY_END) {
      changeState(KQUERY, PATH);

      llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer(kQuery);
      klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
      klee::expr::Parser *P = klee::expr::Parser::Create("", MB, Builder);
      while (klee::expr::Decl *D = P->ParseTopLevelDecl()) {
        assert(!P->GetNumErrors() && "Error parsing kquery in path file.");
        if (klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>(D)) {
          if (arrayToName.count(AD->Root->name))
            path->symbols[arrayToName[AD->Root->name]] = AD->Root;
        } else if (klee::expr::QueryCommand *QC =
                       dyn_cast<klee::expr::QueryCommand>(D)) {
          path->constraints = klee::ConstraintManager(QC->Constraints);

          for (auto vit : QC->Values) {
            assert((!outputSizes.empty()) && outputSizes.front().second > 0 &&
                   "Too many expressions in path file kquery.");
            path->outputValues[outputSizes.front().first].push_back(vit);
            if (--outputSizes.front().second == 0) {
              outputSizes.erase(outputSizes.begin());
            }
          }
          assert(outputSizes.empty() &&
                 "Too few expressions in path file kquery.");

          delete D;
          break;
        }
      }
      delete P;
      delete Builder;
      delete MB;
    } else if (line == SPA_PATH_EXPLOREDCOVERAGE_START) {
      changeState(PATH, EXPLOREDCOVERAGE);
    } else if (line == SPA_PATH_EXPLOREDCOVERAGE_END) {
      changeState(EXPLOREDCOVERAGE, PATH);
    } else if (line == SPA_PATH_PARTICIPANTS_START) {
      changeState(PATH, PARTICIPANTS);
    } else if (line == SPA_PATH_PARTICIPANTS_END) {
      changeState(PARTICIPANTS, PATH);
    } else if (line == SPA_PATH_EXPLOREDPATH_START) {
      changeState(PATH, EXPLOREDPATH);
    } else if (line == SPA_PATH_EXPLOREDPATH_END) {
      changeState(EXPLOREDPATH, PATH);
    } else if (line == SPA_PATH_TESTINPUTS_START) {
      changeState(PATH, TESTINPUTS);
    } else if (line == SPA_PATH_TESTINPUTS_END) {
      changeState(TESTINPUTS, PATH);
    } else if (line == SPA_PATH_TESTCOVERAGE_START) {
      changeState(PATH, TESTCOVERAGE);
    } else if (line == SPA_PATH_TESTCOVERAGE_END) {
      changeState(TESTCOVERAGE, PATH);
    } else if (line == SPA_PATH_END) {
      changeState(PATH, START);
      if (!filter || filter->checkPath(*path))
        return path;
      else
        delete path;
    } else {
      switch (state) {
      case SYMBOLS: {
        std::vector<std::string> s = split(line, SPA_PATH_SYMBOL_DELIMITER);
        assert(s.size() == 2 && "Invalid symbol specification in path file.");
        arrayToName[s[1]] = s[0];
      } break;
      case OUTPUTS: {
        std::vector<std::string> s = split(line, SPA_PATH_OUTPUT_DELIMITER);
        assert(s.size() == 2 && "Invalid output specification in path file.");
        outputSizes.push_back(std::make_pair(s[0], strToNum<int>(s[1])));
      } break;
      case TAGS: {
        std::vector<std::string> s = split(line, SPA_PATH_TAG_DELIMITER);
        assert(s.size() == 2 && "Invalid tag specification in path file.");
        path->tags[s[0]] = s[1];
      } break;
      case KQUERY: {
        kQuery += " " + line;
      } break;
      case EXPLOREDCOVERAGE: {
        auto delim = line.find(" ");
        std::string name = line.substr(0, delim);

        if (delim != std::string::npos) {
          std::stringstream ss(line.substr(name.length()));
          std::map<long, bool> coverage;

          while (ss.good()) {
            while (ss.peek() == ' ') {
              ss.get();
            }
            bool covered = (ss.peek() != '!');
            if (!covered) {
              ss.get();
            }
            long lineNo;
            ss >> lineNo;
            coverage[lineNo] = covered;
          }

          assert(!coverage.empty() && "Invalid line coverage line.");
          path->exploredLineCoverage[name] = coverage;
        } else {
          if (name[0] == '!') {
            name = name.substr(1);
            path->exploredFunctionCoverage[name] = false;
          } else {
            path->exploredFunctionCoverage[name] = true;
          }
        }
      } break;
      case PARTICIPANTS: {
        path->participants.push_back(line);
      } break;
      case EXPLOREDPATH: {
        auto delim = line.find(" ");
        static std::string moduleName = "";
        if (delim == std::string::npos) {
          moduleName = line.substr(delim + 1);
        } else {
          std::string srcLine = line.substr(0, delim);
          bool branch = !(line.substr(delim + 1) == "0");

          assert(!moduleName.empty());
          path->exploredPath[moduleName]
              .push_back(std::make_pair(srcLine, branch));
        }
      } break;
      case TESTINPUTS: {
        std::string name = line.substr(0, line.find(" "));
        std::stringstream ss(line.substr(name.length()));
        std::vector<uint8_t> value;

        while (ss.good()) {
          unsigned int n;
          ss >> std::hex >> n;
          value.push_back(n);
        }

        path->testInputs[name] = value;
      } break;
      case TESTCOVERAGE: {
        std::string name = line.substr(0, line.find(" "));
        std::stringstream ss(line.substr(name.length()));
        std::map<long, bool> coverage;

        while (ss.good()) {
          while (ss.peek() == ' ') {
            ss.get();
          }
          bool covered = (ss.peek() != '!');
          if (!covered) {
            ss.get();
          }
          long line;
          ss >> line;
          coverage[line] = covered;
        }

        if (coverage.empty()) {
          if (name[0] == '!') {
            name = name.substr(1);
            path->testFunctionCoverage[name] = false;
          } else {
            path->testFunctionCoverage[name] = true;
          }
        } else {
          path->testLineCoverage[name] = coverage;
        }
      } break;
      default: {
        assert(false && "Invalid path file.");
      } break;
      }
    }
  }

  // Might have found an incomplete path, restore checkpoint position.
  input.clear();
  input.seekg(checkpointPos, std::ios::beg);
  lineNumber = checkpointLN;

  return NULL;
}

Path *PathLoader::getPath(uint64_t pathID) {
  restart();
  if (skipPaths(pathID)) {
    return getPath();
  } else {
    return NULL;
  }
}

std::string PathLoader::getPathText() {
  // Save current position in case of failure.
  unsigned long checkpointLN = lineNumber;
  std::streampos checkpointPos = input.tellg();

  LoadState_t state = START;
  std::string result;
  while (input.good()) {
    std::string line;
    getline(input, line);
    lineNumber++;
    result += line + '\n';
    line = cleanUpLine(line);
    if (line.empty())
      continue;

    if (line == SPA_PATH_START) {
      changeState(START, PATH);
    } else if (line == SPA_PATH_END) {
      changeState(PATH, START);
      return result;
    } else {
      changeState(PATH, PATH);
    }
  }

  // Might have found an incomplete path, restore checkpoint position.
  input.clear();
  input.seekg(checkpointPos, std::ios::beg);
  lineNumber = checkpointLN;

  return "";
}

std::string PathLoader::getPathText(uint64_t pathID) {
  restart();
  if (skipPaths(pathID)) {
    return getPathText();
  } else {
    return "";
  }
}

bool PathLoader::skipPath() {
  // Save current position in case of failure.
  unsigned long checkpointLN = lineNumber;
  std::streampos checkpointPos = input.tellg();

  LoadState_t state = START;
  while (input.good()) {
    std::string line;
    getline(input, line);
    lineNumber++;
    line = cleanUpLine(line);
    if (line.empty())
      continue;

    if (line == SPA_PATH_START) {
      changeState(START, PATH);
    } else if (line == SPA_PATH_END) {
      changeState(PATH, START);
      return true;
    } else {
      changeState(PATH, PATH);
    }
  }

  // Might have found an incomplete path, restore checkpoint position.
  input.clear();
  input.seekg(checkpointPos, std::ios::beg);
  lineNumber = checkpointLN;

  return false;
}

bool PathLoader::skipPaths(uint64_t numPaths) {
  for (uint64_t i = 0; i < numPaths; i++) {
    if (!skipPath()) {
      return false;
    }
  }
  return true;
}
}
