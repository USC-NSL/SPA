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
// #include "spa/CFGForwardIF.h"
#include "spa/CFGBackwardIF.h"
#include "spa/WhitelistIF.h"
#include "spa/DummyIF.h"
#include "spa/NegatedIF.h"
// #include "spa/IntersectionIF.h"
#include "spa/PathFilter.h"

extern std::string InputFile;

namespace {
	llvm::cl::opt<std::string> DumpCFG("dump-cfg", llvm::cl::desc(
		"Dumps the analyzed program's annotated CFG to the given file, as a .dot file."));

	llvm::cl::opt<std::string> PathFile("path-file", llvm::cl::desc(
		"Sets the output path file."));
}

class SpaPathFilter : public SPA::PathFilter {
public:
	bool checkPath( SPA::Path &path ) {
		return ! path.getTag( SPA_OUTPUT_TAG ).empty();
	}
};

int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer" );

	llvm::Module *module = klee::loadByteCode();
	module = klee::prepareModule( module );

	std::string pathFileName = PathFile;
	if ( pathFileName == "" )
		pathFileName = InputFile + ".paths";
	CLOUD9_INFO( "Writing output to: " << pathFileName );
	std::ofstream pathFile( pathFileName.c_str(), std::ios::out | std::ios::trunc );
	assert( pathFile.is_open() && "Unable to open path file." );
	SPA::SPA spa = SPA::SPA( module, pathFile );

	// Pre-process the CFG and select useful paths.
	CLOUD9_INFO( "Pruning CFG." );

	// Get full CFG and call-graph.
	CLOUD9_DEBUG( "   Building CFG & CG." );
	SPA::CFG cfg( module );
	SPA::CG cg( cfg );

	CLOUD9_DEBUG( "   Creating CFG filter." );
	// Find entry points.
	std::set<llvm::Instruction *> entryPoints;
	Function *fn = module->getFunction( SPA_API_ANNOTATION_FUNCTION );
	if ( fn ) {
		std::set<llvm::Instruction *> apiCallers = cg.getDefiniteCallers( fn );
		for ( std::set<llvm::Instruction *>::iterator it = apiCallers.begin(), ie = apiCallers.end(); it != ie; it++ ) {
			CLOUD9_DEBUG( "      Found API entry function: " << (*it)->getParent()->getParent()->getName().str() );
			spa.addEntryFunction( (*it)->getParent()->getParent() );
			entryPoints.insert( &(*it)->getParent()->getParent()->front().front() );
		}
	} else {
		CLOUD9_INFO( "      API annotation function not present in module." );
	}
	fn = module->getFunction( SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION );
	if ( fn ) {
		std::set<llvm::Instruction *> mhCallers = cg.getDefiniteCallers( fn );
		for ( std::set<llvm::Instruction *>::iterator it = mhCallers.begin(), ie = mhCallers.end(); it != ie; it++ ) {
			CLOUD9_DEBUG( "      Found message handler entry function: " << (*it)->getParent()->getParent()->getName().str() );
			spa.addEntryFunction( (*it)->getParent()->getParent() );
			entryPoints.insert( &(*it)->getParent()->getParent()->front().front() );
		}
	} else {
		CLOUD9_INFO( "      Message handler annotation function not present in module." );
	}
	assert( ! entryPoints.empty() && "No APIs or message handlers found." );

	// Find outputs.
	fn = module->getFunction( SPA_CHECKPOINT_ANNOTATION_FUNCTION );
	std::set<llvm::Instruction *> checkpoints;
	if ( fn )
		checkpoints = cg.getDefiniteCallers( fn );
	assert( ! checkpoints.empty() && "No message outputs found." );

	// Create instruction filter.
// 	SPA::IntersectionIF filter = SPA::IntersectionIF();
// 	filter.addIF( new SPA::CFGForwardIF( cfg, cg, entryPoints ) );
// 	filter.addIF( new SPA::CFGBackwardIF( cfg, cg, checkpoints ) );
	SPA::CFGBackwardIF filter = SPA::CFGBackwardIF( cfg, cg, checkpoints );
// 	SPA::DummyIF filter = SPA::DummyIF();
	spa.setInstructionFilter( &filter );

	CLOUD9_DEBUG( "   Setting up path checkpoints." );
	for ( std::set<llvm::Instruction *>::iterator it = checkpoints.begin(), ie = checkpoints.end(); it != ie; it++ )
		spa.addCheckpoint( *it );

	if ( DumpCFG.size() > 0 ) {
		CLOUD9_DEBUG( "Dumping CFG to: " << DumpCFG.getValue() );
		std::ofstream dotFile( DumpCFG.getValue().c_str() );
		assert( dotFile.is_open() && "Unable to open dump file." );

		std::map<SPA::InstructionFilter *, std::string> annotations;
		annotations[new SPA::WhitelistIF( entryPoints )] = "style = \"filled\" color = \"green\"";
		annotations[new SPA::WhitelistIF( checkpoints )] = "style = \"filled\" color = \"red\"";
		annotations[new SPA::NegatedIF( &filter )] = "style = \"filled\"";

		cfg.dump( dotFile, NULL, annotations );
// 		cfg.dump( dotFile, &filter, annotations );

		dotFile.flush();
		dotFile.close();
	}

	spa.setPathFilter( new SpaPathFilter() );
	spa.setOutputTerminalPaths( true );

	CLOUD9_DEBUG( "Starting SPA." );
	spa.start();

	pathFile.flush();
	pathFile.close();
	CLOUD9_DEBUG( "Done." );

	return 0;
}
