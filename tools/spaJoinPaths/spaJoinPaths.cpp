#include <unistd.h>
#include <set>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "spa/PathLoader.h"

#define FOLLOW_WAIT_S 1

std::string getPathFromFile(std::string fileName, SPA::PathLoaderPosition &p) {
  std::ifstream ifs(fileName);
  assert(ifs.good() && "Unable to open input path-file.");
  std::unique_ptr<SPA::PathLoader> pathLoader(new SPA::PathLoader(ifs));
  if (p.filePosition) {
    pathLoader->load(p);
  }
  std::string path = pathLoader->getPathText();
  p = pathLoader->save();
  ifs.close();
  return path;
}

int main(int argc, char **argv, char **envp) {
  assert(argc > 1 && "Insufficient arguments.");
  bool followInputs = (strcmp(argv[1], "-f") == 0);
  assert(argc > (followInputs ? 3 : 2) && "Insufficient arguments.");

  // {<file name, position>}
  std::set<std::pair<std::string, SPA::PathLoaderPosition *> > inputs;
  for (int i = (followInputs ? 2 : 1); i < argc - 1; i++) {
    inputs.insert(std::make_pair(argv[i], new SPA::PathLoaderPosition));
  }

  std::ofstream output(argv[argc - 1], std::ofstream::out | std::ofstream::app);
  assert(output.good() && "Unable to open output path-file.");

  unsigned long pathCount = 0;
  bool foundPath;
  do {
    foundPath = false;
    for (auto iit : inputs) {
      std::string p = getPathFromFile(iit.first, *iit.second);
      if (p.empty()) {
        continue;
      } else {
        foundPath = true;
        output << p;
        pathCount++;
      }
    }
    if (followInputs && !foundPath) {
      sleep(FOLLOW_WAIT_S);
    }
  } while (foundPath || followInputs);

  llvm::outs() << "Joined " << pathCount << " paths.\n";
}
