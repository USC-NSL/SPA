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
#include "spa/CFGForwardIF.h"
#include "spa/CFGBackwardIF.h"
#include "spa/WhitelistIF.h"
#include "spa/NegatedIF.h"
#include "spa/IntersectionIF.h"
#include "spa/PathFilter.h"

#define SPA_API_ANNOTATION_FUNCTION				"spa_api_entry"
#define SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION	"spa_message_handler_entry"
#define SPA_OUTPUT_ANNOTATION_FUNCTION			"spa_output"

extern std::string InputFile;

namespace {
	llvm::cl::opt<std::string> DumpCFG("dump-cfg", llvm::cl::desc(
		"Dumps the analyzed program's annotated CFG to the given file, as a .dot file."));

	llvm::cl::opt<std::string> PathFile("path-file", llvm::cl::desc(
		"Sets the output path file."));
}

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
	std::set<llvm::Instruction *> apiCallers = cg.getCallers( module->getFunction( SPA_API_ANNOTATION_FUNCTION ) );
	for ( std::set<llvm::Instruction *>::iterator it = apiCallers.begin(), ie = apiCallers.end(); it != ie; it++ ) {
		spa.addEntryFunction( (*it)->getParent()->getParent() );
		entryPoints.insert( &(*it)->getParent()->getParent()->front().front() );
	}
	std::set<llvm::Instruction *> mhCallers = cg.getCallers( module->getFunction( SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION ) );
	for ( std::set<llvm::Instruction *>::iterator it = mhCallers.begin(), ie = mhCallers.end(); it != ie; it++ ) {
		spa.addEntryFunction( (*it)->getParent()->getParent() );
		entryPoints.insert( &(*it)->getParent()->getParent()->front().front() );
	}
	assert( ! entryPoints.empty() && "No APIs or message handlers found." );

	// Find outputs.
	std::set<llvm::Instruction *> outputPoints = cg.getCallers( module->getFunction( SPA_OUTPUT_ANNOTATION_FUNCTION ) );
	assert( ! outputPoints.empty() && "No message outputs found." );

	// Create instruction filter.
	SPA::IntersectionIF filter = SPA::IntersectionIF();
	filter.addIF( new SPA::CFGForwardIF( cfg, cg, entryPoints ) );
	filter.addIF( new SPA::CFGBackwardIF( cfg, cg, outputPoints ) );
	spa.setInstructionFilter( &filter );

	if ( DumpCFG.size() > 0 ) {
		CLOUD9_DEBUG( "Dumping CFG to: " << DumpCFG.getValue() );
		std::ofstream dotFile( DumpCFG.getValue().c_str() );
		assert( dotFile.is_open() && "Unable to open dump file." );

		std::map<SPA::InstructionFilter *, std::string> annotations;
		annotations[new SPA::WhitelistIF( entryPoints )] = "style = \"filled\" color = \"green\"";
		annotations[new SPA::WhitelistIF( outputPoints )] = "style = \"filled\" color = \"red\"";
		annotations[new SPA::NegatedIF( &filter )] = "style = \"filled\"";

		cfg.dump( dotFile, annotations );

		dotFile.flush();
		dotFile.close();
	}

	CLOUD9_DEBUG( "Starting SPA." );
	spa.start();

	pathFile.flush();
	pathFile.close();
	CLOUD9_DEBUG( "Done." );

	return 0;
}
