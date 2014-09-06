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

  llvm::cl::opt<unsigned> NumOutFiles("n", llvm::cl::desc(
    "Number of output files."));

  llvm::cl::opt<unsigned> NumPathsPerFile("p", llvm::cl::desc(
    "Number of paths per output file."));
}

std::vector<std::ofstream *> output;

std::ofstream *getOutput(unsigned long index) {
  if (index >= output.size()) {
    for (unsigned long i = output.size(); i <= index; i++ ) {
      char fileName[MAX_FILE_NAME];
      assert(snprintf(fileName, sizeof(fileName), OutputPattern.getValue().c_str(), i + 1) > 0);
      llvm::outs() << "Creating " << fileName << ".\n";
      std::ofstream *ofs = new std::ofstream(fileName, std::ios_base::out);
      assert(ofs->is_open() && "Unable to open output file." );
      output.push_back(ofs);
    }
  }
  assert(index < output.size());

  return output[index];
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Path-file Splitter" );

  assert((NumOutFiles.getValue() > 0) != (NumPathsPerFile.getValue() > 0) && "Either specify -n or -p.");

  std::ifstream ifs(InputFile);
  assert( ifs.good() && "Unable to open input path-file." );
  SPA::PathLoader input(ifs);

  if (NumOutFiles.getValue() > 0) {
    bool done = false;
    do {
      for (unsigned long i = 0; i < NumOutFiles.getValue(); i++) {
        std::string p = input.getPathText();
        if (p.empty()) {
          done = true;
          break;
        }
        *getOutput(i) << p;
      }
    } while (! done);
  } else {
    assert(NumPathsPerFile.getValue() > 0);
    unsigned long j = 0;
    bool done = false;
    do {
      for (unsigned long i = 0; i < NumPathsPerFile.getValue(); i++) {
        std::string p = input.getPathText();
        if (p.empty()) {
          done = true;
          break;
        }
        *getOutput(j) << p;
      }
      j++;
    } while (! done);
  }

  for (std::ofstream *ofs : output) {
    ofs->close();
  }
}
