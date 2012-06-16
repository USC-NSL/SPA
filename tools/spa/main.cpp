#include <fstream>

#include "llvm/Support/CommandLine.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "klee/Init.h"
#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include <klee/Solver.h>
#include "../../lib/Core/Memory.h"

#include "spa/CFG.h"
#include "spa/CG.h"
#include "spa/SPA.h"
#include "spa/Path.h"
#include "spa/CFGBackwardIF.h"
#include "spa/WhitelistIF.h"
#include "spa/DummyIF.h"
#include "spa/NegatedIF.h"
#include "spa/AstarUtility.h"
#include "spa/PathFilter.h"

extern std::string InputFile;

namespace {
	llvm::cl::opt<std::string> DumpCFG( "dump-cfg",
		llvm::cl::desc( "Dumps the analyzed program's annotated CFG to the given file, as a .dot file." ) );

	llvm::cl::opt<std::string> PathFile( "path-file",
		llvm::cl::desc( "Sets the output path file." ) );

	llvm::cl::opt<bool> Client( "client",
		llvm::cl::desc( "Explores client paths (API to packet output)." ),
		cl::init( false ) );

	llvm::cl::opt<bool> Server( "server",
		llvm::cl::desc( "Explores server paths (packet input to API)." ) );
}

class SpaClientPathFilter : public SPA::PathFilter {
public:
	bool checkPath( SPA::Path &path ) {
		return path.getTag( SPA_HANDLERTYPE_TAG ) == SPA_APIHANDLER_VALUE &&
			! path.getTag( SPA_OUTPUT_TAG ).empty();
	}
};

class SpaServerPathFilter : public SPA::PathFilter {
public:
	bool checkPath( SPA::Path &path ) {
		return path.getTag( SPA_HANDLERTYPE_TAG ) == SPA_MESSAGEHANDLER_VALUE &&
			path.getTag( SPA_VALIDPATH_TAG ) != SPA_VALIDPATH_VALUE;
	}
};

int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer" );

	assert( ((! Client) != (! Server)) && "Must specify either --client or --server." );

	llvm::Module *module = klee::loadByteCode();
	module = klee::prepareModule( module );

	std::string pathFileName = PathFile;
	if ( pathFileName == "" )
		pathFileName = InputFile + (Client ? ".client" : ".server") + ".paths";
	CLOUD9_INFO( "Writing output to: " << pathFileName );
	std::ofstream pathFile( pathFileName.c_str(), std::ios::out | std::ios::trunc );
	assert( pathFile.is_open() && "Unable to open path file." );
	SPA::SPA spa = SPA::SPA( module, pathFile );

	// Pre-process the CFG and select useful paths.
	CLOUD9_INFO( "Pruning CFG." );

	// Get full CFG and call-graph.
	SPA::CFG cfg( module );
	SPA::CG cg( cfg );

	// Find entry handlers.
	std::set<llvm::Instruction *> entryPoints;
	if ( Client ) {
		Function *fn = module->getFunction( SPA_API_ANNOTATION_FUNCTION );
		if ( fn ) {
			std::set<llvm::Instruction *> apiCallers = cg.getDefiniteCallers( fn );
			for ( std::set<llvm::Instruction *>::iterator it = apiCallers.begin(), ie = apiCallers.end(); it != ie; it++ ) {
				CLOUD9_DEBUG( "   Found API entry function: " << (*it)->getParent()->getParent()->getName().str() );
				spa.addEntryFunction( (*it)->getParent()->getParent() );
				entryPoints.insert( *it );
			}
		} else {
			CLOUD9_INFO( "   API annotation function not present in module." );
		}
	} else if ( Server ) {
		Function *fn = module->getFunction( SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION );
		if ( fn ) {
			std::set<llvm::Instruction *> mhCallers = cg.getDefiniteCallers( fn );
			for ( std::set<llvm::Instruction *>::iterator it = mhCallers.begin(), ie = mhCallers.end(); it != ie; it++ ) {
				CLOUD9_DEBUG( "   Found message handler entry function: " << (*it)->getParent()->getParent()->getName().str() );
				spa.addEntryFunction( (*it)->getParent()->getParent() );
				entryPoints.insert( *it );
			}
		} else {
			CLOUD9_INFO( "   Message handler annotation function not present in module." );
		}
	}
	assert( (! entryPoints.empty()) && "No APIs or message handlers found." );

	// Rebuild full CFG and call-graph (changed by SPA after adding init/entry handlers).
	cfg = SPA::CFG( module );
	cg = SPA::CG( cfg );

	// Find checkpoints.
	CLOUD9_DEBUG( "   Setting up path checkpoints." );
	Function *fn = module->getFunction( SPA_CHECKPOINT_ANNOTATION_FUNCTION );
	std::set<llvm::Instruction *> checkpoints;
	if ( fn )
		checkpoints = cg.getDefiniteCallers( fn );
	else
		CLOUD9_INFO( "   Checkpoint annotation function not present in module." );
	assert( ! checkpoints.empty() && "No checkpoints found." );

	for ( std::set<llvm::Instruction *>::iterator it = checkpoints.begin(), ie = checkpoints.end(); it != ie; it++ )
		spa.addCheckpoint( *it );

	// Create instruction filter.
	CLOUD9_DEBUG( "   Creating CFG filter." );
	SPA::InstructionFilter *filter = new SPA::CFGBackwardIF( cfg, cg, checkpoints );
	spa.setInstructionFilter( filter );
	if ( Client )
		spa.setOutputFilteredPaths( false );
	else if ( Server )
		spa.setOutputFilteredPaths( true );
	for ( std::set<llvm::Instruction *>::iterator it = entryPoints.begin(), ie = entryPoints.end(); it != ie; it++ ) {
		if ( ! filter->checkInstruction( *it ) ) {
			CLOUD9_DEBUG( "Entry point at function " << (*it)->getParent()->getParent()->getName().str() << " is not included in filter." );
			assert( false && "Entry point is filtered out." );
		}
	}

	// Create state utility function.
	CLOUD9_DEBUG( "   Creating state utility function." );
	SPA::StateUtility *utility = NULL;
	if ( Client )
		utility = new SPA::AstarUtility( cfg, cg, checkpoints );
// 		utility = new SPA::TargetDistanceUtility( cfg, cg, checkpoints );
	else if ( Server && filter )
		utility = new SPA::AstarUtility( cfg, cg, *filter );
// 		utility = new SPA::TargetDistanceUtility( cfg, cg, *filter );
	spa.setStateUtility( utility );

	if ( DumpCFG.size() > 0 ) {
		CLOUD9_DEBUG( "Dumping CFG to: " << DumpCFG.getValue() );
		std::ofstream dotFile( DumpCFG.getValue().c_str() );
		assert( dotFile.is_open() && "Unable to open dump file." );

		std::map<SPA::InstructionFilter *, std::string> annotations;
		annotations[new SPA::WhitelistIF( checkpoints )] = "style = \"filled\" fillcolor = \"red\"";
		if ( filter )
			annotations[new SPA::NegatedIF( filter )] = "style = \"filled\" fillcolor = \"grey\"";

		cfg.dump( dotFile, /*filter*/ NULL, annotations, utility /*NULL*/ );

		dotFile.flush();
		dotFile.close();
		return 0;
	}

	if ( Client )
		spa.setPathFilter( new SpaClientPathFilter() );
	else if ( Server )
		spa.setPathFilter( new SpaServerPathFilter() );

	spa.setOutputTerminalPaths( false );

	CLOUD9_DEBUG( "Starting SPA." );
	spa.start();

	pathFile.flush();
	pathFile.close();
	CLOUD9_DEBUG( "Done." );

	return 0;
}
