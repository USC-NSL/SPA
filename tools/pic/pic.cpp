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
#include "spa/CFGBackwardIF.h"
#include "spa/WhitelistIF.h"
#include "spa/DummyIF.h"
#include "spa/NegatedIF.h"
#include "spa/WaypointUtility.h"
#include "spa/AstarUtility.h"
#include "spa/FilteredUtility.h"
#include "spa/PathFilter.h"

namespace {
std::string InputFile;

llvm::cl::opt<std::string>
    DumpCFG("dump-cfg",
            llvm::cl::desc("Dumps the analyzed program's annotated CFG to the "
                           "given file, as a .dot file."));

llvm::cl::opt<std::string> DumpCG(
    "dump-cg",
    llvm::cl::desc(
        "Dumps the analyzed program's CG to the given file, as a .dot file."));

// 	llvm::cl::opt<bool> SaveSeeds( "save-seeds",
// 		llvm::cl::desc( "Generates seed paths from seed inputs." ) );
//
// 	llvm::cl::opt<std::string> SeedFile( "seed-file",
// 		llvm::cl::desc( "Loads previously generated seed paths." ) );

llvm::cl::opt<std::string> InitValueFile(
    "init-values",
    llvm::cl::desc(
        "Loads initial values (typically state) from the specified file."));

llvm::cl::opt<std::string>
    PathFile("path-file", llvm::cl::desc("Sets the output path file."));

llvm::cl::opt<bool>
    Client("client",
           llvm::cl::desc("Explores client paths (API to packet output)."),
           llvm::cl::init(false));

llvm::cl::opt<bool>
    Server("server",
           llvm::cl::desc("Explores server paths (packet input to API)."));

llvm::cl::opt<std::string, true>
    InputFileOpt(llvm::cl::desc("<input bytecode>"), llvm::cl::Positional,
                 llvm::cl::location(InputFile), llvm::cl::init("-"));

llvm::cl::opt<std::string>
    SenderPaths("sender-paths",
                llvm::cl::desc("Specifies a path-file to with sender path to "
                               "seed receiving inputs with."));

llvm::cl::opt<bool> FollowSenderPaths(
    "follow-sender-paths",
    llvm::cl::desc("Tells SPA to follow the specified sender path-file for new "
                   "paths as they are generated (joint symbolic execution)."));
}

class SpaClientPathFilter : public SPA::PathFilter {
public:
  bool checkPath(SPA::Path &path) {
    return path.getTag(SPA_HANDLERTYPE_TAG) == SPA_APIHANDLER_VALUE &&
           path.getTag(SPA_OUTPUT_TAG) == SPA_OUTPUT_VALUE /*&&
      			path.getTag( "ReplayDone" ) == "1"*/;
  }
};

class SpaServerPathFilter : public SPA::PathFilter {
public:
  bool checkPath(SPA::Path &path) {
    return path.getTag(SPA_HANDLERTYPE_TAG) == SPA_MESSAGEHANDLER_VALUE &&
           path.getTag(SPA_MSGRECEIVED_TAG) == SPA_MSGRECEIVED_VALUE &&
           path.getTag(SPA_VALIDPATH_TAG) != SPA_VALIDPATH_VALUE;
  }
};

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv, "Systematic Protocol Analyzer");

  assert(((!Client) != (!Server)) &&
         "Must specify either --client or --server.");

  std::string pathFileName = PathFile;
  if (pathFileName == "")
    pathFileName = InputFile + (Client ? ".client" : ".server") + ".paths";
  klee::klee_message("Writing output to: %s", pathFileName.c_str());
  std::ofstream pathFile(pathFileName.c_str(), std::ios::out | std::ios::app);
  assert(pathFile.is_open() && "Unable to open path file.");
  SPA::SPA spa(InputFile, pathFile);

  llvm::Module *module = spa.getModule();

  // Pre-process the CFG and select useful paths.
  klee::klee_message("Pruning CFG.");

  // Get full CFG and call-graph.
  SPA::CFG cfg(module);
  SPA::CG cg(module);

  if (InitValueFile.size() > 0) {
    klee::klee_message("   Setting up initial values.");
    llvm::Function *fn = module->getFunction(SPA_INPUT_ANNOTATION_FUNCTION);
    assert(fn);
    std::map<std::string, std::pair<llvm::Value *, size_t> > initValueVars;
    for (auto it : cg.getDefiniteCallers(fn)) {
      const llvm::CallInst *callInst;
      assert(callInst = dyn_cast<llvm::CallInst>(it));
      assert(callInst->getNumArgOperands() == 5);
      const llvm::ConstantInt *ci;
      assert(ci = dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(1)));
      uint64_t size = ci->getValue().getLimitedValue();
      llvm::User *u;
      assert(u = dyn_cast<llvm::User>(callInst->getArgOperand(2)));
      llvm::GlobalVariable *gv;
      assert(gv = dyn_cast<llvm::GlobalVariable>(u->getOperand(0)));
      llvm::ConstantDataArray *cda;
      assert(cda = dyn_cast<llvm::ConstantDataArray>(gv->getInitializer()));
      // string reconversion to fix LLVM bug (includes null in std::string).
      std::string name = cda->getAsString();
      llvm::Value *var = callInst->getArgOperand(3);

      klee::klee_message("      Found input %s[%ld].", name.c_str(), size);
      assert(initValueVars.count(name) == 0 && "Input multiply declared.");
      initValueVars[name] = std::pair<llvm::Value *, size_t>(var, size);
    }

    std::ifstream initValueFile(InitValueFile.c_str());
    assert(initValueFile.is_open());
    std::map<llvm::Value *,
             std::vector<std::vector<std::pair<bool, uint8_t> > > > initValues;
    while (initValueFile.good()) {
      std::string line;
      getline(initValueFile, line);
      if (!line.empty()) {
        std::string name;
        std::vector<std::pair<bool, uint8_t> > value;
        std::stringstream ss(line);
        ss >> name;
        ss << std::hex;
        while (ss.good()) {
          int v;
          ss >> v;
          if (!ss.fail()) {
            value.push_back(std::pair<bool, uint8_t>(true, v));
          } else {
            value.push_back(std::pair<bool, uint8_t>(false, 0));
            ss.clear();
            std::string dummy;
            ss >> dummy;
          }
        }

        klee::klee_message("      Found initial value for %s[%ld].",
                           name.c_str(), value.size());
        assert(initValueVars.count(name) > 0 &&
               "Initial value defined but not used.");
        assert(initValueVars[name].second >= value.size() &&
               "Initial value doesn't fit in variable.");

        // If last specified by is concrete pad value with concrete 0s to fill
        // variable, otherwise pad with symbols.
        for (size_t i = value.size(); i < initValueVars[name].second; i++)
          value.push_back(std::pair<bool, uint8_t>(value.back().first, 0));

        initValues[initValueVars[name].first].push_back(value);
      } else {
        if (!initValues.empty()) {
          klee::klee_message("      Adding set of %ld initial values.",
                             initValues.size());
          spa.addInitialValues(initValues);
          initValues.clear();
        }
      }
    }
    if (!initValues.empty()) {
      klee::klee_message("      Adding initial value set.");
      spa.addInitialValues(initValues);
    }
  } else {
    klee::klee_message(
        "      No initial input values given, leaving symbolic.");
    spa.addSymbolicInitialValues();
  }

  // 	// Find seed IDs.
  // 	std::set<unsigned int> seedIDs;
  // 	if ( SaveSeeds ) {
  // 		klee::klee_message( "   Setting up path seed generation." );
  // 		Function *fn = module->getFunction( SPA_SEED_ANNOTATION_FUNCTION );
  // 		assert( fn );
  // 		for ( std::set<llvm::Instruction *>::iterator it = cg.getDefiniteCallers(
  // fn ).begin(), ie = cg.getDefiniteCallers( fn ).end(); it != ie; it++ ) {
  // 			const CallInst *callInst;
  // 			assert( callInst = dyn_cast<CallInst>( *it ) );
  // 			assert( callInst->getNumArgOperands() == 4 );
  // 			const llvm::ConstantInt *constInt;
  // 			assert( constInt = dyn_cast<llvm::ConstantInt>( callInst->getArgOperand(
  // 0 ) ) );
  // 			uint64_t id = constInt->getValue().getLimitedValue();
  // 			if ( ! seedIDs.count( id ) )
  // 				klee::klee_message( "      Found seed id: " << id );
  // 			seedIDs.insert( id );
  // 		}
  // 		assert( ! seedIDs.empty() );
  // 	}

  // Find entry handlers.
  std::set<llvm::Instruction *> entryPoints;
  if (Client) {
    llvm::Function *fn = module->getFunction(SPA_API_ANNOTATION_FUNCTION);
    if (fn) {
      for (auto cit : cg.getDefiniteCallers(fn)) {
        klee::klee_message(
            "   Found API entry function: %s",
            cit->getParent()->getParent()->getName().str().c_str());
        // 				if ( seedIDs.empty() )
        spa.addEntryFunction(cit->getParent()->getParent());
        // 				else
        // 					for ( std::set<unsigned int>::iterator sit = seedIDs.begin(),
        // sie = seedIDs.end(); sit != sie; sit++ )
        // 						spa.addSeedEntryFunction( *sit,
        // (*cit)->getParent()->getParent() );
        entryPoints.insert(cit);
      }
    } else {
      klee::klee_message("   API annotation function not present in module.");
    }
  } else if (Server) {
    llvm::Function *fn =
        module->getFunction(SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION);
    if (fn) {
      for (auto cit : cg.getDefiniteCallers(fn)) {
        klee::klee_message(
            "   Found message handler entry function: %s",
            cit->getParent()->getParent()->getName().str().c_str());
        // 				if ( seedIDs.empty() )
        spa.addEntryFunction(cit->getParent()->getParent());
        // 				else
        // 					for ( std::set<unsigned int>::iterator sit = seedIDs.begin(),
        // sie = seedIDs.end(); sit != sie; sit++ )
        // 						spa.addSeedEntryFunction( *sit,
        // (*cit)->getParent()->getParent() );
        entryPoints.insert(cit);
      }
    } else {
      klee::klee_message(
          "   Message handler annotation function not present in module.");
    }
  }
  assert((!entryPoints.empty()) && "No APIs or message handlers found.");

  // Rebuild full CFG and call-graph (changed by SPA after adding init/entry
  // handlers).
  cfg = SPA::CFG(module);
  cg = SPA::CG(module);

  if (DumpCG.size() > 0) {
    klee::klee_message("Dumping CG to: %s", DumpCG.getValue().c_str());
    std::ofstream dotFile(DumpCG.getValue().c_str());
    assert(dotFile.is_open() && "Unable to open dump file.");

    cg.dump(dotFile);

    dotFile.flush();
    dotFile.close();
    return 0;
  }

  // Find checkpoints.
  klee::klee_message("   Setting up path checkpoints.");
  llvm::Function *fn = module->getFunction(SPA_CHECKPOINT_ANNOTATION_FUNCTION);
  std::set<llvm::Instruction *> checkpoints;
  if (fn)
    checkpoints = cg.getDefiniteCallers(fn);
  else
    klee::klee_message(
        "   Checkpoint annotation function not present in module.");
  assert(!checkpoints.empty() && "No checkpoints found.");

  klee::klee_message("   Setting up path waypoints.");
  fn = module->getFunction(SPA_WAYPOINT_ANNOTATION_FUNCTION);
  std::map<unsigned int, std::set<llvm::Instruction *> > waypoints;
  if (fn) {
    for (auto it : cg.getDefiniteCallers(fn)) {
      const llvm::CallInst *callInst;
      assert(callInst = dyn_cast<llvm::CallInst>(it));
      if (callInst->getNumArgOperands() != 1) {
        klee::klee_message("      Found waypoint at %s",
                           SPA::debugLocation(it).c_str());
        klee::klee_message("Arguments: %d", callInst->getNumArgOperands());
        callInst->dump();
        assert(false &&
               "Waypoint annotation function has wrong number of arguments.");
      }
      const llvm::ConstantInt *constInt;
      assert(constInt =
                 dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(0)));
      uint64_t id = constInt->getValue().getLimitedValue();
      klee::klee_message("      Found waypoint with id %ld at %s", id,
                         SPA::debugLocation(it).c_str());
      waypoints[id].insert(it);
    }
  } else {
    klee::klee_message(
        "   Waypoint annotation function not present in module.");
  }

  for (auto it : checkpoints) {
    spa.addCheckpoint(NULL, it);
  }

  // Create instruction filter.
  klee::klee_message("   Creating CFG filter.");
  SPA::CFGBackwardIF *filter = new SPA::CFGBackwardIF(cfg, cg, checkpoints);
  for (auto it : entryPoints) {
    if (!filter->checkInstruction(it)) {
      klee::klee_message(
          "Entry point at function %s is not included in filter. Disabling "
          "filter.",
          it->getParent()->getParent()->getName().str().c_str());
      filter = NULL;
    }
  }
  // 	if ( filter ) {
  // 		if ( Client )
  // 			spa.addStateUtilityBack( filter, false );
  // 		else if ( Server )
  // 			spa.addStateUtilityBack( filter, true );
  // 	}

  // Create waypoint utility.
  // 	SPA::WaypointUtility *waypointUtility = NULL;
  // 	if ( ! waypoints.empty() ) {
  // 		klee::klee_message( "   Creating waypoint utility." );
  // 		waypointUtility = new SPA::WaypointUtility( cfg, cg, waypoints, true );
  // 		spa.addStateUtilityBack( waypointUtility, false );
  // 	}

  // Create state utility function.
  klee::klee_message("   Creating state utility function.");

  spa.addStateUtilityBack(new SPA::FilteredUtility(), false);
  if (Client) {
//     spa.addStateUtilityBack(new SPA::AstarUtility(module, cfg, cg, checkpoints),
//                             false);
    spa.addStateUtilityBack(
        new SPA::TargetDistanceUtility(module, cfg, cg, checkpoints), false);
  } else if (Server && filter) {
//     spa.addStateUtilityBack(new SPA::AstarUtility(module, cfg, cg, *filter),
//                             false);
    spa.addStateUtilityBack(
        new SPA::TargetDistanceUtility(module, cfg, cg, *filter), false);
  }
  // All else being the same, go DFS.
  spa.addStateUtilityBack(new SPA::DepthUtility(), false);

  if (DumpCFG.size() > 0) {
    klee::klee_message("Dumping CFG to: %s", DumpCFG.getValue().c_str());
    // 		std::ofstream dotFile( DumpCFG.getValue().c_str() );
    // 		assert( dotFile.is_open() && "Unable to open dump file." );

    std::map<SPA::InstructionFilter *, std::string> annotations;
    annotations[new SPA::WhitelistIF(checkpoints)] =
        "style = \"filled\" fillcolor = \"red\"";
    // 		if ( filter )
    // 			annotations[new SPA::NegatedIF( filter )] = "style = \"filled\"
    // fillcolor = \"grey\"";

    // 		cfg.dump( dotFile, cg, /*filter*/ NULL, annotations, /*utility*/
    // /*waypointUtility*/ /*filter*/ /*NULL*/ new SPA::TargetDistanceUtility(
    // module, cfg, cg, checkpoints ), /*FULL*/ /*BASICBLOCK*/ FUNCTION );
    // 		cg.dump( dotFile );
    cfg.dumpDir(DumpCFG.getValue(), cg, annotations,
                /*utility*/ /*waypointUtility*/ /*filter*/ /*NULL*/ new SPA::
                    TargetDistanceUtility(module, cfg, cg, checkpoints));

    // 		dotFile.flush();
    // 		dotFile.close();
    return 0;
  }

  if (Client) {
    spa.setOutputTerminalPaths(false);
    spa.setPathFilter(new SpaClientPathFilter());
  } else if (Server) {
    spa.setOutputTerminalPaths(true);
    spa.setPathFilter(new SpaServerPathFilter());
  }

  std::ifstream ifs;
  if (!SenderPaths.empty()) {
    klee::klee_message("   Seeding paths from sender path-file: %s",
                       SenderPaths.c_str());
    ifs.open(SenderPaths);
    assert(ifs.good() && "Unable to open sender path file.");
    spa.setSenderPathLoader(new SPA::PathLoader(ifs), FollowSenderPaths);
    spa.addDefaultValueMappings();
  }

  klee::klee_message("Starting SPA.");
  spa.start();

  pathFile.flush();
  pathFile.close();
  klee::klee_message("Done.");

  return 0;
}
