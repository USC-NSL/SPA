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
#include "spa/CFGForwardIF.h"
#include "spa/CFGBackwardIF.h"
#include "spa/WhitelistIF.h"
#include "spa/NegatedIF.h"
#include "spa/IntersectionIF.h"
#include "spa/PathFilter.h"
#include "spa/maxRuntime.h"

#define MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION	"max_message_handler_entry"
#define MAX_INTERESTING_ANNOTATION_FUNCTION		"max_interesting"
#define MAX_HANDLER_NAME_VAR_NAME				"max_internal_HandlerName"
#define MAX_NUM_INTERESTING_VAR_NAME			"max_internal_NumInteresting"

namespace {
	llvm::cl::opt<std::string> DumpCFG("dump-cfg", llvm::cl::desc(
		"Dumps the analyzed program's annotated CFG to the given file, as a .dot file."));
}

class MaxPathFilter : public SPA::PathFilter {
private:
	klee::Solver *solver;

public:
	MaxPathFilter() {
		solver = new klee::STPSolver( false, true );
		solver = klee::createCexCachingSolver(solver);
		solver = klee::createCachingSolver(solver);
		solver = klee::createIndependentSolver(solver);
	}

	bool checkPath( klee::ExecutionState *kState ) {
		// Find special max variables.
		const klee::ObjectState *numInterestingObjState = NULL;
		const klee::ObjectState *handlerNameObjState = NULL;
		for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::iterator it = kState->symbolics.begin(), ie = kState->symbolics.end(); it != ie; it++ ) {
			if ( (*it).first->name == MAX_HANDLER_NAME_VAR_NAME )
				handlerNameObjState = kState->addressSpace().findObject( (*it).first );
			if ( (*it).first->name == MAX_NUM_INTERESTING_VAR_NAME )
				numInterestingObjState = kState->addressSpace().findObject( (*it).first );
		}
		
		if ( numInterestingObjState && handlerNameObjState ) {
			klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
			klee::ref<klee::Expr> checkInteresting = exprBuilder->Not(
				exprBuilder->Eq(
					numInterestingObjState->read( 0, 32 ),
								exprBuilder->Constant( APInt( 32, 0 ) ) ) );
			
			bool result = false;
			assert( solver->mustBeTrue( klee::Query( kState->constraints(), checkInteresting), result ) && "Solver failure." );
			
// 			if ( result ) {
// 				klee::ref<klee::Expr> addressExpr = handlerNameObjState->read( 0, klee::Context::get().getPointerWidth() );
// 				assert( isa<klee::ConstantExpr>(addressExpr) && "Handler Name is symbolic." );
// 				klee::ref<klee::ConstantExpr> address = cast<klee::ConstantExpr>(addressExpr);
// 				klee::ObjectPair op;
// 				assert( kState->addressSpace().resolveOne(address, op) && "Handler Name is not uniquely defined." );
// 				const klee::MemoryObject *mo = op.first;
// 				const klee::ObjectState *os = op.second;
// 				
// 				char *buf = new char[mo->size];
// 				unsigned ioffset = 0;
// 				klee::ref<klee::Expr> offset_expr = klee::SubExpr::create(address, op.first->getBaseExpr());
// 				assert( isa<klee::ConstantExpr>(offset_expr) && "Handler Name is an invalid string." );
// 				klee::ref<klee::ConstantExpr> value = cast<klee::ConstantExpr>(offset_expr.get());
// 				ioffset = value.get()->getZExtValue();
// 				assert(ioffset < mo->size);
// 				
// 				unsigned i;
// 				for ( i = 0; i < mo->size - ioffset - 1; i++ ) {
// 					klee::ref<klee::Expr> cur = os->read8( i + ioffset );
// 					assert( isa<klee::ConstantExpr>(cur) && "Symbolic character in Handler Name." );
// 					buf[i] = cast<klee::ConstantExpr>(cur)->getZExtValue(8);
// 				}
// 				buf[i] = 0;
// 				
// 				handlerName = buf;
// 				delete[] buf;
// 			}
			
			return result;
		} else {
			return false;
		}
	}
};


int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Manipulation Attack eXplorer" );

	llvm::Module *module = klee::loadByteCode();
	module = klee::prepareModule( module );

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
	for ( std::set<llvm::Instruction *>::iterator it = mhCallers.begin(), ie = mhCallers.end(); it != ie; it++ )
		messageHandlers.insert( &(*it)->getParent()->getParent()->front().front() );
	assert( ! messageHandlers.empty() && "No message handlers found." );

	// Find interesting instructions.
	std::set<llvm::Instruction *> interestingInstructions = cg.getCallers( module->getFunction( MAX_INTERESTING_ANNOTATION_FUNCTION ) );
	assert( ! interestingInstructions.empty() && "No interesting statements found." );

	// Create instruction filter.
	SPA::IntersectionIF filter = SPA::IntersectionIF();
	filter.addIF( new SPA::CFGForwardIF( cfg, cg, messageHandlers ) );
	filter.addIF( new SPA::CFGBackwardIF( cfg, cg, interestingInstructions ) );

	if ( DumpCFG.size() > 0 ) {
		CLOUD9_DEBUG( "Dumping CFG to: " << DumpCFG.getValue() );
		std::ofstream dotFile( DumpCFG.getValue().c_str() );
		assert( dotFile.is_open() && "Unable to open dump file." );

		std::map<SPA::InstructionFilter *, std::string> annotations;
		annotations[new SPA::WhitelistIF( messageHandlers )] = "style = \"filled\" color = \"green\"";
		annotations[new SPA::WhitelistIF( interestingInstructions )] = "style = \"filled\" color = \"red\"";
		annotations[new SPA::NegatedIF( &filter )] = "style = \"filled\"";

		cfg.dump( dotFile, annotations );

		dotFile.flush();
		dotFile.close();
	}

	std::ofstream pathFile( MAX_PATH_FILE, std::ios::out | std::ios::trunc );
	assert( pathFile.is_open() && "Unable to open path file." );

	SPA::SPA spa = SPA::SPA( module, pathFile );

	spa.start();

	return 0;
}
