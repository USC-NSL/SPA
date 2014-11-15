#include <fstream>

// #include <llvm/ADT/OwningPtr.h>
// #include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include "../../lib/Core/Common.h"

// #include <spa/Path.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<bool> EnableDbg("d", llvm::cl::init(false),
                              llvm::cl::desc("Output debug information."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));

llvm::cl::opt<std::string> OutFileName(llvm::cl::Positional, llvm::cl::Required,
                                       llvm::cl::desc("<output path-file>"));

llvm::cl::opt<std::string> Commands(llvm::cl::Positional, llvm::cl::Required,
                                    llvm::cl::desc("<validation commands>"));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Systematic Protocol Analysis - Validate");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  std::ifstream outFile(OutFileName);
  assert(outFile.is_open() && "Unable to open output path-file.");

  for (size_t pos = 0; pos != std::string::npos;
       pos = Commands.find(";", pos) + 1) {
    std::string cmd = Commands.substr(pos, Commands.find(";", pos));
    // Trim
    cmd.erase(0, cmd.find_first_not_of(" \n\r\t"));
    cmd.erase(cmd.find_last_not_of(" \n\r\t") + 1);

    if (EnableDbg) {
      klee::klee_message("Command: %s", cmd.c_str());
    }
  }

  return 0;
}
