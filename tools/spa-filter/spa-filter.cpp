#include <fstream>
#include <unistd.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/OwningPtr.h>
#include "../../lib/Core/Common.h"
#include <spa/FilterExpression.h>

#include <spa/PathLoader.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#define FOLLOW_WAIT 1 // s

namespace {
llvm::cl::opt<bool> EnableDbg("d", llvm::cl::init(false),
                              llvm::cl::desc("Output debug information."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));

llvm::cl::opt<bool> Follow("f", llvm::cl::init(false),
                           llvm::cl::desc("Follow inputs file."));

llvm::cl::opt<std::string> OutFileName(llvm::cl::Positional, llvm::cl::Required,
                                       llvm::cl::desc("<output path-file>"));

llvm::cl::opt<std::string> RejectOutFileName(
    "output-rejects",
    llvm::cl::desc("Specify file to output rejected paths to."));

llvm::cl::opt<std::string> Criteria(llvm::cl::Positional, llvm::cl::Required,
                                    llvm::cl::desc("<matching criteria>"));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Systematic Protocol Analysis - Filter");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  std::ofstream outFile(OutFileName);
  assert(outFile.is_open() && "Unable to open output path-file.");

  std::ofstream rejectOutFile;
  if (!RejectOutFileName.empty()) {
    rejectOutFile.open(RejectOutFileName);
    assert(rejectOutFile.is_open() &&
           "Unable to open reject output path-file.");
  }

  klee::klee_message("Parsing criteria.");
  llvm::OwningPtr<SPA::FilterExpression> expr(
      SPA::FilterExpression::parse(Criteria));
  assert(expr);

  if (EnableDbg) {
    klee::klee_message("Expression parsed as: %s", expr->dbg_str().c_str());
  }

  SPA::PathLoader pathLoader(inFile);
  llvm::OwningPtr<SPA::Path> path;
  while (path.reset(pathLoader.getPath()), path || Follow) {
    if (!path) {
      klee::klee_message("Waiting for new input paths.");
      sleep(FOLLOW_WAIT);
      continue;
    }

    klee::klee_message("Processing path.");

    if (expr->checkPath(*path.get())) {
      klee::klee_message("Path accepted. Outputting.");
      outFile << *path;
    } else {
      if (rejectOutFile.is_open()) {
        klee::klee_message("Path rejected. Outputting to reject file.");
        rejectOutFile << *path;
      } else {
        klee::klee_message("Path rejected. Dropping.");
      }
    }
  }

  return 0;
}
