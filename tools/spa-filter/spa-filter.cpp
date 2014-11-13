#include <fstream>

#include "llvm/Support/CommandLine.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                  llvm::cl::desc("<input path-file>"));

llvm::cl::opt<std::string> OutFileName(llvm::cl::Positional, llvm::cl::Required,
                                   llvm::cl::desc("<output path-file>"));

llvm::cl::opt<std::string> Criteria(llvm::cl::Positional, llvm::cl::Required,
                                    llvm::cl::desc("<matching criteria>"));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Systematic Protocol Analysis - Filter");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  std::ifstream outFile(OutFileName);
  assert(outFile.is_open() && "Unable to open output path-file.");

  return 0;
}
