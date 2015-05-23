#include <fstream>

#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/OwningPtr.h>
#include "../../lib/Core/Common.h"

#include <spa/PathLoader.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<bool>
    Always("always", llvm::cl::init(false),
           llvm::cl::desc("Finds sites that were covered in every path."));

llvm::cl::opt<bool>
    Never("never", llvm::cl::init(false),
          llvm::cl::desc("Finds sites that were not covered in every path."));

llvm::cl::opt<bool>
    DiffStats("diff-stats", llvm::cl::init(false),
              llvm::cl::desc("Finds the covered sites during exploration but "
                             "not testing, and vice-versa."));

llvm::cl::opt<int>
    N("n", llvm::cl::init(0),
      llvm::cl::desc("The number of stats to show when showing top n."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(
      argc, argv, "Systematic Protocol Analysis - Analyze Coverage");

  assert((Always || Never || DiffStats) &&
         "No analysis requested. Use -help for available options.");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  SPA::PathLoader pathLoader(inFile);
  llvm::OwningPtr<SPA::Path> path;
  unsigned long numPaths = 0;

  std::set<std::pair<std::string, long> > alwaysLineCoverage;
  std::set<std::string> alwaysFunctionCoverage;

  std::set<std::pair<std::string, long> > neverLineCoverage;
  std::set<std::string> neverFunctionCoverage;

  std::map<std::pair<std::string, long>, long> exploredNotTestLineCoverage;
  std::map<std::string, long> exploredNotTestFunctionCoverage;
  std::map<std::pair<std::string, long>, long> testNotExploredLineCoverage;
  std::map<std::string, long> testNotExploredFunctionCoverage;

  if (Always || Never) {
    // Add coverage from first path
    path.reset(pathLoader.getPath());
    for (auto exploredFile : path->getExploredLineCoverage()) {
      for (auto exploredLine : exploredFile.second) {
        if (exploredLine.second) {
          alwaysLineCoverage.insert(
              std::make_pair(exploredFile.first, exploredLine.first));
        } else {
          neverLineCoverage.insert(
              std::make_pair(exploredFile.first, exploredLine.first));
        }
      }
    }
    for (auto exploredFn : path->getExploredFunctionCoverage()) {
      if (exploredFn.second) {
        alwaysFunctionCoverage.insert(exploredFn.first);
      } else {
        neverFunctionCoverage.insert(exploredFn.first);
      }
    }
    // Find lines covered during testing, but not during exploration.
    for (auto testFile : path->getTestLineCoverage()) {
      for (auto testLine : testFile.second) {
        if (testLine.second) {
          alwaysLineCoverage.insert(
              std::make_pair(testFile.first, testLine.first));
        } else {
          neverLineCoverage.insert(
              std::make_pair(testFile.first, testLine.first));
        }
      }
    }
    // Find functions covered during testing, but not during exploration.
    for (auto testFn : path->getTestFunctionCoverage()) {
      if (testFn.second) {
        alwaysFunctionCoverage.insert(testFn.first);
      } else {
        neverFunctionCoverage.insert(testFn.first);
      }
    }
  }

  while (path.reset(pathLoader.getPath()), path) {
    klee::klee_message("Processing path %ld.", ++numPaths);

    if (Always) {
      for (auto a : alwaysLineCoverage) {
        if (!(path->getExploredLineCoverage()[a.first][a.second] ||
              path->getTestLineCoverage()[a.first][a.second])) {
          alwaysLineCoverage.erase(a);
        }
      }
      for (auto a : alwaysFunctionCoverage) {
        if (!(path->getExploredFunctionCoverage()[a] ||
              path->getTestFunctionCoverage()[a])) {
          alwaysFunctionCoverage.erase(a);
        }
      }
    }

    if (Never) {
      for (auto a : neverLineCoverage) {
        if (path->getExploredLineCoverage()[a.first][a.second] ||
            path->getTestLineCoverage()[a.first][a.second]) {
          neverLineCoverage.erase(a);
        }
      }
      for (auto a : neverFunctionCoverage) {
        if (path->getExploredFunctionCoverage()[a] ||
            path->getTestFunctionCoverage()[a]) {
          neverFunctionCoverage.erase(a);
        }
      }
    }

    if (DiffStats) {
      // Find lines covered during exploration, but not during testing.
      for (auto exploredFile : path->getExploredLineCoverage()) {
        for (auto exploredLine : exploredFile.second) {
          if (exploredLine.second &&
              !path->getTestLineCoverage()[exploredFile.first][
                  exploredLine.first]) {
            exploredNotTestLineCoverage[
                std::make_pair(exploredFile.first, exploredLine.first)]++;
          }
        }
      }
      // Find functions covered during exploration, but not during testing.
      for (auto exploredFn : path->getExploredFunctionCoverage()) {
        if (exploredFn.second &&
            !path->getTestFunctionCoverage()[exploredFn.first]) {
          exploredNotTestFunctionCoverage[exploredFn.first]++;
        }
      }
      // Find lines covered during testing, but not during exploration.
      for (auto testFile : path->getTestLineCoverage()) {
        for (auto testLine : testFile.second) {
          if (testLine.second &&
              !path
                  ->getExploredLineCoverage()[testFile.first][testLine.first]) {
            testNotExploredLineCoverage[
                std::make_pair(testFile.first, testLine.first)]++;
          }
        }
      }
      // Find functions covered during testing, but not during exploration.
      for (auto testFn : path->getTestFunctionCoverage()) {
        if (testFn.second &&
            !path->getExploredFunctionCoverage()[testFn.first]) {
          testNotExploredFunctionCoverage[testFn.first]++;
        }
      }
    }
  }

  if (Always) {
    klee::klee_message("Lines covered in every path:");
    for (auto a : alwaysLineCoverage) {
      klee::klee_message("  %s:%ld", a.first.c_str(), a.second);
    }

    klee::klee_message("Functions covered in every path:");
    for (auto a : alwaysFunctionCoverage) {
      klee::klee_message("  %s", a.c_str());
    }
  }

  if (Never) {
    klee::klee_message("Lines not covered in every path:");
    for (auto a : neverLineCoverage) {
      klee::klee_message("  %s:%ld", a.first.c_str(), a.second);
    }

    klee::klee_message("Functions not covered in every path:");
    for (auto a : neverFunctionCoverage) {
      klee::klee_message("  %s", a.c_str());
    }
  }

  if (DiffStats) {
    // Sort coverage stats.
    std::set<std::pair<long, std::pair<std::string, long> > >
        sortedExploredNotTestLineCoverage;
    std::set<std::pair<long, std::string> >
        sortedExploredNotTestFunctionCoverage;
    std::set<std::pair<long, std::pair<std::string, long> > >
        sortedTestNotExploredLineCoverage;
    std::set<std::pair<long, std::string> >
        sortedTestNotExploredFunctionCoverage;

    for (auto a : exploredNotTestLineCoverage) {
      sortedExploredNotTestLineCoverage.insert(
          std::make_pair(-a.second, a.first));
    }
    for (auto a : exploredNotTestFunctionCoverage) {
      sortedExploredNotTestFunctionCoverage.insert(
          std::make_pair(-a.second, a.first));
    }
    for (auto a : testNotExploredLineCoverage) {
      sortedTestNotExploredLineCoverage.insert(
          std::make_pair(-a.second, a.first));
    }
    for (auto a : testNotExploredFunctionCoverage) {
      sortedTestNotExploredFunctionCoverage.insert(
          std::make_pair(-a.second, a.first));
    }

    klee::klee_message(
        "Top lines covered during exploration, but not in testing:");
    long count = N;
    for (auto a : sortedExploredNotTestLineCoverage) {
      klee::klee_message("  %s:%ld (%ld%% of paths)", a.second.first.c_str(),
                         a.second.second, -a.first * 100 / numPaths);
      if (--count == 0) {
        break;
      }
    }

    klee::klee_message(
        "Top functions covered during exploration, but not in testing:");
    count = N;
    for (auto a : sortedExploredNotTestFunctionCoverage) {
      klee::klee_message("  %s (%ld%% of paths)", a.second.c_str(),
                         -a.first * 100 / numPaths);
      if (--count == 0) {
        break;
      }
    }

    klee::klee_message(
        "Top lines covered in testing, but not during exploration:");
    count = N;
    for (auto a : sortedTestNotExploredLineCoverage) {
      klee::klee_message("  %s:%ld (%ld%% of paths)", a.second.first.c_str(),
                         a.second.second, -a.first * 100 / numPaths);
      if (--count == 0) {
        break;
      }
    }

    klee::klee_message(
        "Top functions covered in testing, but not during exploration:");
    count = N;
    for (auto a : sortedTestNotExploredFunctionCoverage) {
      klee::klee_message("  %s (%ld%% of paths)", a.second.c_str(),
                         -a.first * 100 / numPaths);
      if (--count == 0) {
        break;
      }
    }
  }

  return 0;
}
