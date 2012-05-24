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
#include "spa/NegatedIF.h"
// #include "spa/IntersectionIF.h"
#include "spa/PathFilter.h"
#include "spa/max.h"

#define MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION	"max_message_handler_entry"
#define MAX_INTERESTING_ANNOTATION_FUNCTION		"max_interesting"
#define MAX_HANDLER_NAME_TAG					"max_HandlerName"

namespace {
	llvm::cl::opt<std::string> DumpCFG("dump-cfg", llvm::cl::desc(
		"Dumps the analyzed program's annotated CFG to the given file, as a .dot file."));
}

class MaxPathFilter : public SPA::PathFilter {
public:
	bool checkPath( SPA::Path &path ) {
		return ! path.getTag( MAX_HANDLER_NAME_TAG ).empty();
	}
};


int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Manipulation Attack eXplorer" );

	llvm::Module *module = klee::loadByteCode();
	module = klee::prepareModule( module );

	std::ofstream pathFile( MAX_PATH_FILE, std::ios::out | std::ios::trunc );
	assert( pathFile.is_open() && "Unable to open path file." );
	SPA::SPA spa = SPA::SPA( module, pathFile );

	// Pre-process the CFG and select useful paths.
	CLOUD9_INFO( "Pruning CFG." );

	// Get full CFG and call-graph.
	CLOUD9_DEBUG( "   Building CFG & CG." );
	SPA::CFG cfg( module );
	SPA::CG cg( cfg );

	CLOUD9_DEBUG( "   Creating CFG filter." );
	// Find message handling function entry points.
	std::set<llvm::Instruction *> messageHandlers;
	std::set<llvm::Instruction *> mhCallers = cg.getCallers( module->getFunction( MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION ) );
	for ( std::set<llvm::Instruction *>::iterator it = mhCallers.begin(), ie = mhCallers.end(); it != ie; it++ ) {
		spa.addEntryFunction( (*it)->getParent()->getParent() );
		messageHandlers.insert( &(*it)->getParent()->getParent()->front().front() );
	}
	assert( ! messageHandlers.empty() && "No message handlers found." );

	// Find interesting instructions.
	std::set<llvm::Instruction *> interestingInstructions = cg.getCallers( module->getFunction( MAX_INTERESTING_ANNOTATION_FUNCTION ) );
	assert( ! interestingInstructions.empty() && "No interesting statements found." );
	for ( std::set<llvm::Instruction *>::iterator it = interestingInstructions.begin(), ie = interestingInstructions.end(); it != ie; it++ )
		spa.addCheckpoint( *it );

	// Create instruction filter.
// 	SPA::IntersectionIF filter = SPA::IntersectionIF();
// 	filter.addIF( new SPA::CFGForwardIF( cfg, cg, messageHandlers ) );
// 	filter.addIF( new SPA::CFGBackwardIF( cfg, cg, interestingInstructions ) );
	SPA::CFGBackwardIF filter = SPA::CFGBackwardIF( cfg, cg, interestingInstructions );
	spa.setInstructionFilter( &filter );

	if ( DumpCFG.size() > 0 ) {
		CLOUD9_DEBUG( "Dumping CFG to: " << DumpCFG.getValue() );
		std::ofstream dotFile( DumpCFG.getValue().c_str() );
		assert( dotFile.is_open() && "Unable to open dump file." );

		std::map<SPA::InstructionFilter *, std::string> annotations;
		annotations[new SPA::WhitelistIF( messageHandlers )] = "style = \"filled\" color = \"green\"";
		annotations[new SPA::WhitelistIF( interestingInstructions )] = "style = \"filled\" color = \"red\"";
		annotations[new SPA::NegatedIF( &filter )] = "style = \"filled\"";

// 		cfg.dump( dotFile, NULL, annotations );
		cfg.dump( dotFile, &filter, annotations );

		dotFile.flush();
		dotFile.close();
	}

	spa.setPathFilter( new MaxPathFilter() );
	spa.setOutputTerminalPaths( false );

	CLOUD9_DEBUG( "Starting SPA." );
	spa.start();

	pathFile.flush();
	pathFile.close();
	CLOUD9_DEBUG( "Done." );

	return 0;
}
