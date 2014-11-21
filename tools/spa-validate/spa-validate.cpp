#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cctype>
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

  std::ofstream outFile(OutFileName);
  assert(outFile.is_open() && "Unable to open output path-file.");

  size_t pos = -1;
  do {
    pos++;
    std::string cmd = Commands.substr(pos, Commands.find(";", pos));
    // Trim
    cmd.erase(0, cmd.find_first_not_of(" \n\r\t"));
    cmd.erase(cmd.find_last_not_of(" \n\r\t") + 1);

    if (cmd.empty())
      continue;

    if (EnableDbg) {
      klee::klee_message("Parsing command: %s", cmd.c_str());
    }

    std::istringstream iss(cmd);
    std::vector<std::string> args;
    copy(std::istream_iterator<std::string>(iss),
         std::istream_iterator<std::string>(), back_inserter(args));

    if (args[0] == "RUN") {
    } else if (args[0] == "WAIT") {
      if (args[1] == "LISTEN") {
        int port = atoi(args[3].c_str());

        if (args[2] == "TCP") {
        } else if (args[2] == "UDP") {
        } else {
          assert(false && "Unknown protocol to listen for.");
        }
      } else if (std::all_of(args[1].begin(), args[1].end(), isdigit)) {
      } else {
        assert(false && "Invalid WAIT command.");
      }
    } else if (args[0] == "CHECK") {
    } else if (args[0] == "TIMEOUT") {
    } else {
      assert(false && "Unknown command.");
    }
  } while ((pos = Commands.find(";", pos)) != std::string::npos);

  return 0;
}
