#include <fstream>

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

#include "spa/CFG.h"
#include "spa/CG.h"
#include "spa/SPA.h"
#include "spa/Path.h"
// #include "spa/CFGForwardIF.h"
#include "spa/CFGBackwardIF.h"
#include "spa/WhitelistIF.h"
#include "spa/NegatedIF.h"
// #include "spa/IntersectionIF.h"
#include "spa/PathFilter.h"
#include "spa/max.h"

#define MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION "max_message_handler_entry"
#define MAX_INTERESTING_ANNOTATION_FUNCTION "max_interesting"
#define MAX_HANDLER_NAME_TAG "max_HandlerName"

namespace {
std::string InputFile;

llvm::cl::opt<std::string>
    DumpCFG("dump-cfg",
            llvm::cl::desc("Dumps the analyzed program's annotated CFG to the "
                           "given file, as a .dot file."));

llvm::cl::opt<std::string, true>
    InputFileOpt(llvm::cl::desc("<input bytecode>"), llvm::cl::Positional,
                 llvm::cl::location(InputFile), llvm::cl::init("-"));
}

class MaxPathFilter : public SPA::PathFilter {
public:
  bool checkPath(SPA::Path &path) {
    return !path.getTag(MAX_HANDLER_NAME_TAG).empty();
  }
};

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv, "Manipulation Attack eXplorer");

  std::ofstream pathFile(MAX_PATH_FILE, std::ios::out | std::ios::trunc);
  assert(pathFile.is_open() && "Unable to open path file.");
  SPA::SPA spa = SPA::SPA(InputFile, pathFile);

  llvm::Module *module = spa.getModule();

  // Pre-process the CFG and select useful paths.
  klee::klee_message("Pruning CFG.");

  // Get full CFG and call-graph.
  klee::klee_message("   Building CFG & CG.");
  SPA::CFG cfg(module);
  SPA::CG cg(module);

  klee::klee_message("   Creating CFG filter.");
  // Find message handling function entry points.
  std::set<llvm::Instruction *> messageHandlers;
  std::set<llvm::Instruction *> mhCallers = cg.getDefiniteCallers(
      module->getFunction(MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION));
  for (std::set<llvm::Instruction *>::iterator it = mhCallers.begin(),
                                               ie = mhCallers.end();
       it != ie; it++) {
    spa.addEntryFunction((*it)->getParent()->getParent());
    messageHandlers.insert(
        &(*it)->getParent()->getParent()->getEntryBlock().front());
  }
  assert(!messageHandlers.empty() && "No message handlers found.");

  // Find interesting instructions.
  std::set<llvm::Instruction *> interestingInstructions = cg.getDefiniteCallers(
      module->getFunction(MAX_INTERESTING_ANNOTATION_FUNCTION));
  assert(!interestingInstructions.empty() &&
         "No interesting statements found.");
  for (std::set<llvm::Instruction *>::iterator
           it = interestingInstructions.begin(),
           ie = interestingInstructions.end();
       it != ie; it++)
    spa.addCheckpoint(*it);

  // Create instruction filter.
  // 	SPA::IntersectionIF filter = SPA::IntersectionIF();
  // 	filter.addIF( new SPA::CFGForwardIF( cfg, cg, messageHandlers ) );
  // 	filter.addIF( new SPA::CFGBackwardIF( cfg, cg, interestingInstructions )
  SPA::CFGBackwardIF filter =
      SPA::CFGBackwardIF(cfg, cg, interestingInstructions);
  spa.addStateUtilityBack(&filter, false);

  if (DumpCFG.size() > 0) {
    klee::klee_message("Dumping CFG to: %s", DumpCFG.getValue().c_str());
    std::ofstream dotFile(DumpCFG.getValue().c_str());
    assert(dotFile.is_open() && "Unable to open dump file.");

    std::map<SPA::InstructionFilter *, std::string> annotations;
    annotations[new SPA::WhitelistIF(messageHandlers)] =
        "style = \"filled\" color = \"green\"";
    annotations[new SPA::WhitelistIF(interestingInstructions)] =
        "style = \"filled\" color = \"red\"";
    annotations[new SPA::NegatedIF(&filter)] = "style = \"filled\"";

    // 		cfg.dump( dotFile, NULL, annotations, NULL, FULL );
    cfg.dump(dotFile, cg, &filter, annotations, NULL, FULL);

    dotFile.flush();
    dotFile.close();
  }

  spa.setPathFilter(new MaxPathFilter());
  spa.setOutputTerminalPaths(false);

  klee::klee_message("Starting SPA.");
  spa.start();

  pathFile.flush();
  pathFile.close();
  klee::klee_message("Done.");

  return 0;
}
