#include <set>

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

void addOutput(unsigned long index, std::string text) {
  static std::set<unsigned long> initializedFiles;

  char fileName[MAX_FILE_NAME];
  assert(snprintf(fileName, sizeof(fileName), OutputPattern.getValue().c_str(), index + 1) > 0);

  std::ofstream ofs(fileName,
                    std::ios_base::out
                      | (initializedFiles.count(index) ?
                           std::ios_base::app : std::ios_base::trunc));
  assert(ofs.is_open() && "Unable to open output file." );
  initializedFiles.insert(index);

  ofs << text;
  ofs.close();
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Path-file Splitter" );

  assert((NumOutFiles.getValue() > 0) != (NumPathsPerFile.getValue() > 0) && "Either specify -n or -p.");

  std::ifstream ifs(InputFile);
  assert( ifs.good() && "Unable to open input path-file." );
  SPA::PathLoader input(ifs);

  unsigned long pathCount = 0, numFiles = 0, numPathsPerFile = 0;

  if (NumOutFiles.getValue() > 0) {
    bool done = false;
    do {
      for (unsigned long i = 0; i < NumOutFiles.getValue(); i++) {
        std::string p = input.getPathText();
        if (p.empty()) {
          done = true;
          break;
        }
        addOutput(i, p);
        pathCount++;
      }
    } while (! done);

    numFiles = std::min(pathCount, (unsigned long) NumOutFiles.getValue());
    numPathsPerFile = ceil(((double) pathCount) / numFiles);
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
        addOutput(j, p);
        pathCount++;
      }
      j++;
    } while (! done);

    numPathsPerFile = std::min(pathCount, (unsigned long) NumPathsPerFile.getValue());
    numFiles = ceil(((double) pathCount) / NumPathsPerFile.getValue());
  }

  llvm::outs() << "Created " << numFiles
      << " path-files with up to " << numPathsPerFile << " paths per file.\n";
}
