#include <fstream>
#include <sstream>
#include <iterator>

#include "llvm/Support/CommandLine.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include <klee/Solver.h>
#include "../../lib/Core/Common.h"
#include "../../lib/Core/Memory.h"

#include "spa/Util.h"
#include "spa/CFG.h"
#include "spa/CG.h"
#include "spa/SPA.h"
#include "spa/Path.h"
#include "spa/CFGBackwardFilter.h"
#include "spa/WhitelistIF.h"
#include "spa/DummyIF.h"
#include "spa/NegatedIF.h"
#include "spa/WaypointUtility.h"
#include "spa/AstarUtility.h"
#include "spa/FilteredUtility.h"
#include "spa/PathFilter.h"

namespace {
std::string InputBCFile;

llvm::cl::opt<std::string> InPaths(
    "in-paths",
    llvm::cl::desc("Specifies the input path file to connect when processing "
                   "inputs (default: leave inputs unconstrained)."));

llvm::cl::list<std::string> Connect(
    "connect",
    llvm::cl::desc("Specifies symbols to connect in a symbol1=symbol2 format "
                   "(default: auto-detect out=in like patterns)."));

llvm::cl::opt<std::string> OutputPaths(
    "out-paths",
    llvm::cl::desc(
        "Sets the output path file (default: <input-bytecode>.paths)."));

llvm::cl::list<std::string> StartFrom(
    "start-from",
    llvm::cl::desc("Functions to start execution from (default: main)."));

llvm::cl::list<std::string>
    StopAt("stop-at",
           llvm::cl::desc("Code-points to stop symbolic execution after."));

llvm::cl::list<std::string>
    Toward("toward",
           llvm::cl::desc("Code-points to direct execution towards."));

llvm::cl::list<std::string>
    AwayFrom("away-from",
             llvm::cl::desc("Code-points to direct execution away from."));

llvm::cl::opt<bool>
    OutputTerminal("output-terminal",
                   llvm::cl::desc("Enable outputting terminal paths."));

llvm::cl::opt<bool>
    OutputEarly("output-early",
                llvm::cl::desc("Enable outputting early terminated paths."));

llvm::cl::list<std::string>
    OutputAt("output-at", llvm::cl::desc("Code-points to output paths at."));

llvm::cl::opt<std::string, true>
    InputBCFileOpt(llvm::cl::desc("<input bytecode>"), llvm::cl::Positional,
                   llvm::cl::location(InputBCFile));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv, "Systematic Protocol Analyzer");

  std::string pathFileName = OutputPaths;
  if (pathFileName == "")
    pathFileName = InputBCFile + ".paths";
  klee::klee_message("Writing output to: %s", pathFileName.c_str());
  std::ofstream pathFile(pathFileName.c_str(), std::ios::out | std::ios::app);
  assert(pathFile.is_open() && "Unable to open path file.");
  SPA::SPA spa(InputBCFile, pathFile);

  llvm::Module *module = spa.getModule();

  // Pre-process the CFG and select useful paths.
  klee::klee_message("Pruning CFG.");

  // Get full CFG and call-graph.
  SPA::CFG cfg(module);
  SPA::CG cg(module);

  spa.addSymbolicInitialValues();

  // Rebuild full CFG and call-graph (changed by SPA after adding init/entry
  // handlers).
  cfg = SPA::CFG(module);
  cg = SPA::CG(module);

  // Create state utility function.
  klee::klee_message("   Creating state utility function.");

  spa.addStateUtilityBack(new SPA::FilteredUtility(), false);
  // All else being the same, go DFS.
  spa.addStateUtilityBack(new SPA::DepthUtility(), false);

  std::ifstream ifs;
  if (!InPaths.empty()) {
    klee::klee_message("   Seeding paths from path-file: %s", InPaths.c_str());
    ifs.open(InPaths);
    assert(ifs.good() && "Unable to open input path file.");
    spa.setSenderPathLoader(new SPA::PathLoader(ifs), false);
  }

  klee::klee_message("Starting SPA.");
  spa.start();

  pathFile.flush();
  pathFile.close();
  klee::klee_message("Done.");

  return 0;
}
