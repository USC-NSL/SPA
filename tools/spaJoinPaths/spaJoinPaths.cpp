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

int main(int argc, char **argv, char **envp) {
  assert(argc > 1 && "Insufficient arguments.");
  bool followInputs = (strcmp(argv[1], "-f") == 0);
  assert(argc > (followInputs ? 3 : 2) && "Insufficient arguments.");

  std::set<SPA::PathLoader *> inputs;
  for (int i = (followInputs ? 2 : 1); i < argc - 1; i++) {
    std::ifstream *ifs = new std::ifstream(argv[i]);
    assert(ifs->good() && "Unable to open input path-file.");
    inputs.insert(new SPA::PathLoader(*ifs));
  }

  std::ofstream output(argv[argc - 1], std::ofstream::out | std::ofstream::app);
  assert(output.good() && "Unable to open output path-file.");

  unsigned long pathCount = 0;
  bool foundPath;
  do {
    foundPath = false;
    for (auto iit : inputs) {
      std::string p = iit->getPathText();
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
