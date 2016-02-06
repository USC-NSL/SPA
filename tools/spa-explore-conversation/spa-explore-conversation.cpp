#include <unistd.h>

#include <fstream>

#include <llvm/Support/CommandLine.h>
#include "../../lib/Core/Common.h"

#include <spa/PathLoader.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<std::string> InPaths(llvm::cl::Positional, llvm::cl::Required,
                                   llvm::cl::desc("<input path-file>"));

llvm::cl::opt<bool> FollowInPaths(
    "follow-in-paths",
    llvm::cl::desc(
        "Enables following the input path file as new data as added."));

llvm::cl::opt<std::string> OutputPaths(
    "out-paths",
    llvm::cl::desc("Sets the output path file (default: append to input."));

llvm::cl::opt<bool>
    OutputPathsAppend("out-paths-append",
                      llvm::cl::desc("Enable appending paths to output file."));

llvm::cl::opt<bool> Debug("d", llvm::cl::desc("Show debug information."));
}

std::vector<SPA::Path *> paths;

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(
      argc, argv, "Systematic Protocol Analysis - Explore Conversation");

  klee::klee_message("Reading input from: %s", InPaths.c_str());
  std::ifstream ifs(InPaths);
  assert(ifs.good() && "Unable to open input path file.");
  std::unique_ptr<SPA::PathLoader> pathLoader(new SPA::PathLoader(ifs, true));

  if (OutputPaths.empty()) {
    OutputPaths = InPaths;
    OutputPathsAppend = true;
  }
  klee::klee_message("Writing output to: %s", OutputPaths.c_str());
  std::ofstream outFile(
      OutputPaths.c_str(),
      std::ios_base::out |
          (OutputPathsAppend ? std::ios_base::app : std::ios_base::trunc));
  assert(outFile.is_open() && "Unable to open output path-file.");

  klee::Solver *solver =
  klee::createIndependentSolver(klee::createCachingSolver(
    klee::createCexCachingSolver(new klee::STPSolver(false, true))));

  SPA::Path *newPath;
  for (unsigned long pathID = 0;; pathID++) {
    while ((!(newPath = pathLoader->getPath())) && FollowInPaths) {
      sleep(1);
      klee::klee_message("Waiting for path %ld.", pathID);
    }
    if (!newPath) {
      break;
    }
    klee::klee_message("Loading path %ld.", pathID);

    for (auto pairPath : paths) {
      std::unique_ptr<SPA::Path> derivedPath(
          SPA::Path::buildDerivedPath(newPath, pairPath,solver));
      if (derivedPath) {
        klee::klee_message("Augmented path %s with %s to produce %s.",
                           newPath->getUUID().c_str(),
                           pairPath->getUUID().c_str(),
                           derivedPath->getUUID().c_str());
        outFile << *derivedPath;
      }

      derivedPath.reset(SPA::Path::buildDerivedPath(pairPath, newPath,solver));
      if (derivedPath) {
        klee::klee_message("Augmented path %s with %s to produce %s.",
                           pairPath->getUUID().c_str(),
                           newPath->getUUID().c_str(),
                           derivedPath->getUUID().c_str());
        outFile << *derivedPath;
      }
    }

    paths.push_back(newPath);
  }

  outFile.flush();
  klee::klee_message("Done.");

  return 0;
}
