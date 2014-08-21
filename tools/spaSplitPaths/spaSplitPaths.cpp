#include <vector>

#include "llvm/Support/CommandLine.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "spa/PathLoader.h"


#define MAX_FILE_NAME 100

namespace {
  llvm::cl::opt<std::string> InputFile("i", llvm::cl::desc(
    "Input path-file."), llvm::cl::Required);

  llvm::cl::opt<std::string> OutputPattern("o", llvm::cl::desc(
    "Output file name pattern."), llvm::cl::Required);

  llvm::cl::opt<unsigned> OutputQuantity("n", llvm::cl::desc(
    "Number of output files."), llvm::cl::Required);
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Path-file Splitter" );

  std::ifstream ifs(InputFile);
  assert( ifs.good() && "Unable to open input path-file." );
  SPA::PathLoader input(ifs);

  std::vector<std::ofstream *> output;
  assert(OutputQuantity.getValue() > 0);
  for (unsigned long i = 0; i < OutputQuantity.getValue(); i++) {
    char fileName[MAX_FILE_NAME];
    assert(snprintf(fileName, sizeof(fileName), OutputPattern.getValue().c_str(), i + 1) > 0);
    std::ofstream *ofs = new std::ofstream(fileName, std::ios_base::out);
    assert(ofs->is_open() && "Unable to open output file." );
    output.push_back(ofs);
  }

  bool done = false;
  do {
    for (unsigned long i = 0; i < OutputQuantity.getValue(); i++) {
      std::string p = input.getPathText();
      if (p.empty()) {
        done = true;
        break;
      }
      *output[i] << p;
    }
  } while (! done);

  for (std::ofstream *ofs : output) {
    ofs->close();
  }
}
