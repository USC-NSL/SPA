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
llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(
      argc, argv, "Systematic Protocol Analysis - Analyze Coverage");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  SPA::PathLoader pathLoader(inFile);
  llvm::OwningPtr<SPA::Path> path;
  while (path.reset(pathLoader.getPath()), path) {
    klee::klee_message("Processing path.");
  }

  return 0;
}
