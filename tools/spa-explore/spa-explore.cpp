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

#include "../../lib/Core/Common.h"

#include "spa/SPA.h"
#include "spa/PathLoader.h"
#include "spa/CFGBackwardIF.h"
#include "spa/DbgLineIF.h"
#include "spa/UnionIF.h"
#include "spa/NegatedIF.h"
#include "spa/AstarUtility.h"
#include "spa/FilteredUtility.h"
#include "spa/JSEUtility.h"
#include "spa/FilterExpression.h"

namespace {
std::string InputBCFile;

llvm::cl::opt<std::string> InPaths(
    "in-paths",
    llvm::cl::desc("Specifies the input path file to connect when processing "
                   "inputs (default: leave inputs unconstrained)."));

llvm::cl::opt<bool> FollowInPaths(
    "follow-in-paths",
    llvm::cl::desc(
        "Enables folowing the input path file as new data as added."));

llvm::cl::list<std::string>
    Connect("connect", llvm::cl::desc("Specifies symbols to connect in a "
                                      "receiverSymbol1=senderSymbol2 format."));

llvm::cl::opt<bool>
    ConnectInOut("connect-in-out", llvm::cl::init(false),
                 llvm::cl::desc("Automatically connect inputs to outputs."));

llvm::cl::opt<bool>
    ConnectInIn("connect-in-in", llvm::cl::init(false),
                llvm::cl::desc("Automatically connect common inputs."));

llvm::cl::opt<bool> ConnectSockets(
    "connect-sockets", llvm::cl::init(false),
    llvm::cl::desc("Automatically connect socket inputs to outputs."));

llvm::cl::opt<std::string> OutputPaths(
    "out-paths",
    llvm::cl::desc(
        "Sets the output path file (default: <input-bytecode>.paths)."));

llvm::cl::opt<bool>
    OutputPathsAppend("out-paths-append",
                      llvm::cl::desc("Enable appending paths to output file."));

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
    OutputTerminal("output-terminal", llvm::cl::init(false),
                   llvm::cl::desc("Enable outputting terminal paths."));

llvm::cl::opt<bool>
    OutputDone("output-done", llvm::cl::init(false),
                   llvm::cl::desc("Output paths when the program finishes."));

llvm::cl::opt<bool> OutputLogExhausted(
    "output-when-log-exhausted", llvm::cl::init(false),
    llvm::cl::desc("Output paths when they exhaust the symbolic log (receive "
                   "the first input after replay)."));

llvm::cl::list<std::string>
    OutputAt("output-at", llvm::cl::desc("Code-points to output paths at."));

llvm::cl::opt<std::string> OutputFilter(
    "filter-output",
    llvm::cl::desc("Conditions that paths must meet to be outputted. "
                   "Equivalent to using spa-filter on output."));

llvm::cl::opt<std::string>
    DumpCFG("dump-cfg",
            llvm::cl::desc("Dumps the analyzed program's annotated CFG to the "
                           "given directory."));

llvm::cl::opt<std::string>
    DumpCFGFunction("dump-cfg-fn",
                    llvm::cl::desc("Only dump the specified function."));

llvm::cl::opt<std::string, true>
    InputBCFileOpt(llvm::cl::Positional, llvm::cl::Required,
                   llvm::cl::location(InputBCFile),
                   llvm::cl::desc("<input bytecode>"));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Systematic Protocol Analysis - Explore");

  std::string pathFileName = OutputPaths;
  if (pathFileName == "")
    pathFileName = InputBCFile + ".paths";
  klee::klee_message("Writing output to: %s", pathFileName.c_str());
  std::ofstream pathFile(
      pathFileName.c_str(),
      std::ios_base::out | OutputPathsAppend ? std::ios_base::app
                                             : std::ios_base::trunc);
  assert(pathFile.is_open() && "Unable to open path file.");
  SPA::SPA spa(InputBCFile, pathFile);

  llvm::Module *module = spa.getModule();

  if (StartFrom.size() > 0) {
    for (auto entryFnName : StartFrom) {
      klee::klee_message("Starting symbolic execution at: %s",
                         entryFnName.c_str());
      llvm::Function *fn = module->getFunction(entryFnName);
      assert(fn && "Start point function not found.");
      spa.addEntryFunction(fn);
    }
  } else {
    klee::klee_message("Starting symbolic execution at: main");
    llvm::Function *fn = module->getFunction("main");
    assert(fn && "Start point function not found.");
    spa.addEntryFunction(fn);
  }

  spa.addSymbolicInitialValues();

  std::ifstream ifs;
  if (!InPaths.empty()) {
    klee::klee_message("   Seeding paths from path-file: %s", InPaths.c_str());
    ifs.open(InPaths);
    assert(ifs.good() && "Unable to open input path file.");
    spa.setSenderPathLoader(new SPA::PathLoader(ifs, true), FollowInPaths);
  }

  for (auto connection : Connect) {
    auto delim = connection.find('=');
    std::string rValue = connection.substr(0, delim);
    std::string sValue = connection.substr(delim + 1);
    klee::klee_message("   Seeding symbol %s with values from %s",
                       rValue.c_str(), sValue.c_str());
    spa.addValueMapping(sValue, rValue);
  }

  if (ConnectInOut) {
    klee::klee_message("   Connecting inputs to outputs.");
    spa.mapInputsToOutputs();
  }

  if (ConnectInIn) {
    klee::klee_message("   Connecting common inputs.");
    spa.mapCommonInputs();
  }

  if (ConnectSockets) {
    klee::klee_message("   Connecting sockets inputs to outputs.");
    spa.mapSockets();
  }

  // Get full CFG and call-graph.
  SPA::CFG cfg(module);
  SPA::CG cg(module);

  for (auto stopPoint : StopAt) {
    klee::klee_message("   Stopping at: %s", stopPoint.c_str());
    SPA::DbgLineIF *dbgInsts = SPA::DbgLineIF::parse(module, stopPoint);
    assert(dbgInsts && "Error parsing stop point.");
    spa.addStopPoint(dbgInsts);
  }

  // Create state utility function.
  klee::klee_message("   Creating state utility function.");

  SPA::UnionIF directingTargets;
  for (auto towardsPoint : Toward) {
    klee::klee_message("      Directing towards: %s", towardsPoint.c_str());
    SPA::DbgLineIF *dbgInsts = SPA::DbgLineIF::parse(module, towardsPoint);
    assert(dbgInsts && "Error parsing towards point.");
    directingTargets.addIF(dbgInsts);
  }

  for (auto awayFromPoint : AwayFrom) {
    klee::klee_message("      Directing away from: %s", awayFromPoint.c_str());
    SPA::DbgLineIF *dbgInsts = SPA::DbgLineIF::parse(module, awayFromPoint);
    assert(dbgInsts && "Error parsing away from point.");
    directingTargets.addIF(
        new SPA::NegatedIF(new SPA::CFGBackwardIF(cfg, cg, dbgInsts)));
  }

  spa.addStateUtilityBack(new SPA::FilteredUtility(), false);
  spa.addStateUtilityBack(new SPA::JSEUtility(), false);

  if (DumpCFG.size() > 0) {
    klee::klee_message("Dumping CFG to: %s", DumpCFG.getValue().c_str());

    std::map<SPA::InstructionFilter *, std::string> annotations;
    annotations[&directingTargets] = "style = \"filled\" fillcolor = \"red\"";

    if (DumpCFGFunction.empty()) {
      cfg.dumpDir(DumpCFG.getValue(), cg, annotations,
                  new SPA::TargetDistanceUtility(
                      module, cfg, cg, new SPA::NegatedIF(&directingTargets)));
    } else {
      llvm::Function *fn = module->getFunction(DumpCFGFunction);
      assert(fn && "Cannot dump invalid function.");

      std::ofstream dotFile(DumpCFG + "/" + fn->getName().str() + ".dot");
      assert(dotFile.good());

      cfg.dump(dotFile, fn, cg, annotations,
               new SPA::TargetDistanceUtility(
                   module, cfg, cg, new SPA::NegatedIF(&directingTargets)));
    }
    return 0;
  }

  if (!directingTargets.getSubFilters().empty()) {
    spa.addStateUtilityBack(
        new SPA::AstarUtility(module, cfg, cg,
                              new SPA::NegatedIF(&directingTargets)),
        false);
  }
  // All else being the same, go DFS.
  spa.addStateUtilityBack(new SPA::DepthUtility(), false);

  for (auto outputPoint : OutputAt) {
    klee::klee_message("Adding a checkpoint at: %s", outputPoint.c_str());
    SPA::DbgLineIF *dbgInsts = SPA::DbgLineIF::parse(module, outputPoint);
    assert(dbgInsts && "Error parsing output point.");
    spa.addCheckpoint(dbgInsts);
  }

  spa.setOutputTerminalPaths(OutputTerminal);
  spa.setOutputDone(OutputDone);
  spa.setOutputLogExhausted(OutputLogExhausted);

  if (!OutputFilter.empty()) {
    SPA::FilterExpression *filter = SPA::FilterExpression::parse(OutputFilter);
    assert(filter && "Invalid filter expression.");

    spa.setPathFilter(filter);
  }

  klee::klee_message("Starting SPA.");
  spa.start();

  pathFile.flush();
  pathFile.close();
  klee::klee_message("Done.");

  return 0;
}
