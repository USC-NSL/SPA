#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cassert>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/Instruction.h"
#include "llvm/Type.h"
#include "llvm/InstrTypes.h"
#include "llvm/Instructions.h"

#include "llvm/Bitcode/ReaderWriter.h"

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/IRBuilder.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

// All the KLEE includes below

#include "klee/Internal/System/Time.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "klee/Init.h"
#include "klee/Executor.h"
#include "klee/Searcher.h"
#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include "../../lib/Core/Memory.h"
#include "../../lib/Core/TimingSolver.h"

#include "cloud9/Logger.h"
#include "cloud9/ExecutionTree.h"
#include "cloud9/ExecutionPath.h"
#include "cloud9/Protocols.h"
#include "cloud9/worker/TreeNodeInfo.h"
#include "cloud9/worker/JobManager.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/KleeCommon.h"
#include "cloud9/worker/CommManager.h"
#include "cloud9/Utils.h"
#include "cloud9/instrum/InstrumentationManager.h"

#include "spa/CFG.h"
#include "spa/CG.h"

#define MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION	"max_message_handler_entry"
#define MAX_INTERESTING_ANNOTATION_FUNCTION		"max_interesting"
#define MAX_NUM_INTERESTING_VAR_NAME			"max_num_interesting"
#define MAX_ENTRY_FUNCTION						"__user_main"
#define MAX_OLD_ENTRY_FUNCTION					"__max_old_user_main"
#define MAX_HANDLER_ID_VAR_NAME					"maxHanderID"

using namespace SPA;

namespace {
cl::opt<bool>
        StandAlone("stand-alone",
                cl::desc("Enable running a worker in stand alone mode"),
                cl::init(false));

cl::opt<std::string> DumpCFG("dump-cfg", cl::desc(
		"Dumps the analyzed program's annotated CFG to the given file, as a .dot file."));
}

static bool Interrupted = false;
extern cl::opt<double> MaxTime;
extern cl::opt<bool> UseInstructionFiltering;

cloud9::worker::JobManager *theJobManager = NULL;

// This is a temporary hack. If the running process has access to
// externals then it can disable interrupts, which screws up the
// normal "nice" watchdog termination process. We try to request the
// interpreter to halt using this mechanism as a last resort to save
// the state data before going ahead and killing it.
/*
 * This function invokes haltExecution() via gdb.
 */
static void haltViaGDB(int pid) {
	char buffer[256];
	sprintf(buffer, "gdb --batch --eval-command=\"p haltExecution()\" "
		"--eval-command=detach --pid=%d &> /dev/null", pid);

	if (system(buffer) == -1)
		perror("system");
}

/*
 * This function gets executed by the watchdog, via gdb.
 */
// Pulled out so it can be easily called from a debugger.
extern "C" void haltExecution() {
	theJobManager->requestTermination();
}

/*
 *
 */
static void parseArguments(int argc, char **argv) {
	// TODO: Implement some filtering, or reading from a settings file, or
	// from stdin
	cl::ParseCommandLineOptions(argc, argv, "Manipulation Attack eXplorer");
}

/*
 *
 */
static void interrupt_handle() {
	if (!Interrupted && theJobManager) {
		CLOUD9_INFO("Ctrl-C detected, requesting interpreter to halt.");
		haltExecution();
		sys::SetInterruptFunction(interrupt_handle);
	} else {
		CLOUD9_INFO("Ctrl+C detected, exiting.");
		exit(1); // XXX Replace this with pthread_exit() or with longjmp
	}
	Interrupted = true;
}

static int watchdog(int pid) {
  CLOUD9_INFO("Watchdog: Watching " << pid);
  
  double nextStep = klee::util::getWallTime() + MaxTime * 1.1;
  int level = 0;
  
  // Simple stupid code...
  while (1) {
    sleep(1);
    
    int status, res = waitpid(pid, &status, WNOHANG);

    if (res < 0) {
      if (errno == ECHILD) { // No child, no need to watch but
	// return error since we didn't catch
	// the exit.
	perror("waitpid:");
	CLOUD9_INFO("Watchdog exiting (no child) @ " << klee::util::getWallTime());
	return 1;
      } else if (errno != EINTR) {
	perror("Watchdog waitpid");
	exit(1);
      }
    } else if (res == pid && WIFEXITED(status)) {
      return WEXITSTATUS(status);
    } else if (res == pid && WIFSIGNALED(status)) {
      CLOUD9_INFO("killed by signal " <<  WTERMSIG(status));
    } else if (res == pid && WIFSTOPPED(status)) {
      CLOUD9_INFO("stopped by signal " <<  WSTOPSIG(status));
    } else if ( res == pid && WIFCONTINUED(status)) {
      CLOUD9_INFO("continued\n");
    } else {
      double time = klee::util::getWallTime();

      if (time > nextStep) {
	++level;

	if (level == 1) {
	  CLOUD9_INFO("Watchdog: time expired, attempting halt via INT");
	  kill(pid, SIGINT);
	} else if (level == 2) {
	  CLOUD9_INFO("Watchdog: time expired, attempting halt via gdb");
	  haltViaGDB(pid);
	} else {
	  CLOUD9_INFO("Watchdog: kill(9)ing child (I tried to be nice)");
	  kill(pid, SIGKILL);
	  return 1; // what more can we do
	}

	// Ideally this triggers a dump, which may take a while,
	// so try and give the process extra time to clean up.
	nextStep = klee::util::getWallTime() + std::max(15., MaxTime
							* .1);
      }
    }
  }

  return 0;
}

void dumpCFG(
	CFG &cfg, CG &cg,
	std::set<llvm::Function *> messageHandlers,
	std::set<llvm::Instruction *> interestingInstructions,
	std::set<llvm::Instruction *> uselessInstructions ) {
	// Generate CFG DOT file.
	CLOUD9_DEBUG( "   Generating CFG output: " + DumpCFG );
	FILE *dotFile = fopen( DumpCFG.getValue().c_str(), "w" );
	fprintf( dotFile, "digraph CFG {\n" );

	// Color message handling function clusters.
	for ( std::set<llvm::Function *>::iterator it = messageHandlers.begin(), ie = messageHandlers.end(); it != ie; it++ )
		fprintf( dotFile, "	subgraph cluster_%s {\n		color = \"red\";\n	}\n", (*it)->getName().str().c_str() );
	// Add all instructions.
	for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
		Instruction *inst = *it;
		const char *shape = "oval";
		const char *style = "";
		if ( inst == &(inst->getParent()->getParent()->getEntryBlock().front()) )
			shape = "box";
		if ( cfg.getSuccessors( inst ).empty() )
			shape = "doublecircle";
		if ( uselessInstructions.count( inst ) > 0 )
			style = " style = \"filled\"";
		if ( interestingInstructions.count( inst ) > 0 )
			style = " style = \"filled\" color = \"red\"";
		fprintf( dotFile, "	subgraph cluster_%s {\n		label = \"%s\";\n		n%p [label = \"%d\" shape = \"%s\"%s];\n	}\n",
			inst->getParent()->getParent()->getName().str().c_str(),
			inst->getParent()->getParent()->getName().str().c_str(),
			(void*) inst, inst->getDebugLoc().getLine(),
			shape, style );
	}
	// Add edges.
	// Successors.
	fprintf( dotFile, "	edge [color = \"green\"];\n" );
	for ( CFG::iterator it1 = cfg.begin(), ie1 = cfg.end(); it1 != ie1; it1++ )
		for ( std::set<llvm::Instruction *>::iterator it2 = cfg.getSuccessors( *it1 ).begin(), ie2 = cfg.getSuccessors( *it1 ).end(); it2 != ie2; it2++ )
			fprintf( dotFile, "	n%p -> n%p;\n", (void*) *it1, (void*) *it2 );
	// Callers.
	fprintf( dotFile, "	edge [color = \"blue\"];\n" );
	for ( CG::iterator it1 = cg.begin(), ie1 = cg.end(); it1 != ie1; it1++ ) {
		llvm::Function *fn = *it1;
		for ( std::set<llvm::Instruction *>::iterator it2 = cg.getCallers( fn ).begin(), ie2 = cg.getCallers( fn ).end(); it2 != ie2; it2++ ) {
			if ( fn == NULL )
				fprintf( dotFile, "	IndirectFunction [label = \"*\" shape = \"box\"]\n	n%p -> IndirectFunction;\n", (void*) *it2 );
			else if ( ! fn->empty() )
				fprintf( dotFile, "	n%p -> n%p;\n", (void*) *it2, (void*) &(fn->getEntryBlock().front()) );
			else
				fprintf( dotFile, "	n%p [label = \"%s\" shape = \"box\"]\n	n%p -> n%p;\n", (void*) fn, fn->getName().str().c_str(), (void*) *it2, (void*) fn );
		}
	}

	fprintf( dotFile, "}\n" );
	fclose( dotFile );
}

void done() {
	exit(0);
}

class MaxStateEventHandler: public cloud9::worker::StateEventHandler {
private:
	klee::Solver *solver;
	int testCaseID;

	bool isStateInteresting( klee::ExecutionState *kState, int handlerID ) {
		// Find special max variables.
		const klee::Array *handlerIDArray = NULL;
		const klee::ObjectState *numInterestingObjState = NULL;
		for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::iterator it = kState->symbolics.begin(), ie = kState->symbolics.end(); it != ie; it++ ) {
			if ( (*it).first->name == MAX_HANDLER_ID_VAR_NAME )
				handlerIDArray = (*it).second;
			if ( (*it).first->name == MAX_NUM_INTERESTING_VAR_NAME )
				numInterestingObjState = kState->addressSpace().findObject( (*it).first );
		}
		
		if ( handlerIDArray && numInterestingObjState ) {
			klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
			klee::ref<klee::Expr> checkInteresting = exprBuilder->And(
				exprBuilder->Eq(
					klee::Expr::createTempRead( handlerIDArray, klee::Expr::Int32 ),
					exprBuilder->Constant( APInt( 32, handlerID ) ) ),
				exprBuilder->Not(
					exprBuilder->Eq(
						numInterestingObjState->read( 0, 32 ),
						exprBuilder->Constant( APInt( 32, 0 ) ) ) ) );

			bool result = false;
			assert( solver->mustBeTrue( klee::Query( kState->constraints(), checkInteresting), result ) && "Solver failure." );
			return result;
		} else {
			return false;
		}
	}

public:
	MaxStateEventHandler() : testCaseID( 0 ) {
		solver = new klee::STPSolver( false, true );
		solver = klee::createCexCachingSolver(solver);
		solver = klee::createCachingSolver(solver);
		solver = klee::createIndependentSolver(solver);
	}
	~MaxStateEventHandler() {
	}
	
	bool onStateBranching(klee::ExecutionState *state, klee::ForkTag forkTag) { return true; }
	void onStateBranched(klee::ExecutionState *kState, klee::ExecutionState *parent, int index, klee::ForkTag forkTag) {}
	void onOutOfResources(klee::ExecutionState *destroyedState) {}
	void onEvent(klee::ExecutionState *kState, unsigned int type, long int value) {}
	void onControlFlowEvent(klee::ExecutionState *kState, cloud9::worker::ControlFlowEvent event) {}
	void onDebugInfo(klee::ExecutionState *kState, const std::string &message) {}

	void onStateDestroy(klee::ExecutionState *kState, bool silenced) {
		assert(kState);

		if ( isStateInteresting( kState, 1 ) ) {
			CLOUD9_DEBUG( "--- Found interesting state. ---" );
			CLOUD9_DEBUG( "Symbolic Variables:" );
			for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::iterator it = kState->symbolics.begin(), ie = kState->symbolics.end(); it != ie; it++ ) {
				CLOUD9_DEBUG( "	" << (*it).first->name << " = " << (*it).second->name );
				const klee::ObjectState *os = kState->addressSpace().findObject( (*it).first );
				if ( os )
					CLOUD9_DEBUG( "		" << os->read( 0, 32 ) );
			}
			std::ostream &out = cloud9::Logger::getLogger().getDebugStream();
			out << "Path Constraints:" << std::endl;
			for ( klee::ConstraintManager::constraint_iterator it = kState->constraints().begin(), ie = kState->constraints().end(); it != ie; it++)
				out << *it << std::endl;
			CLOUD9_DEBUG( "---  ---" );
		}
	}
};

int main(int argc, char **argv, char **envp) {
	// Make sure to clean up properly before any exit point in the program
	atexit(llvm::llvm_shutdown);

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	cloud9::Logger::getLogger().setLogPrefix("Worker<   >: ");

	// JIT initialization
	llvm::InitializeNativeTarget();

	sys::PrintStackTraceOnErrorSignal();

	cloud9::initBreakSignal();

	// Fill up every global cl::opt object declared in the program
	parseArguments(argc, argv);

	// Setup the watchdog process
	if (MaxTime == 0) {
		CLOUD9_INFO("No max time specified; running without watchdog");
	} else {
		int pid = fork();
		if (pid < 0) {
			CLOUD9_EXIT("Unable to fork watchdog");
		} else if (pid) {
			int returnCode = watchdog(pid);
			CLOUD9_INFO("Watchdog child exited with ret = " <<  returnCode);
			return returnCode;
			//return watchdog(pid);
		}
	}

	// At this point, if the watchdog is enabled, we are in the child process of
	// the fork().

	// Take care of Ctrl+C requests
	sys::SetInterruptFunction(interrupt_handle);

	Module *mainModule = klee::loadByteCode();
	mainModule = klee::prepareModule(mainModule);

	int pArgc;
	char **pArgv;
	char **pEnvp;
	klee::readProgramArguments(pArgc, pArgv, pEnvp, envp);

	// Pre-process the CFG and select useful paths.
	CLOUD9_INFO( "Pruning CFG." );

	// Get full CFG and call-graph.
	CLOUD9_DEBUG( "   Building CFG & CG." );
	CFG cfg( mainModule );
	CG cg( cfg );

	// Find message handling functions.
	CLOUD9_DEBUG( "   Searching for message handlers." );
	std::set<llvm::Function *> messageHandlers;
	std::set<llvm::Instruction *> mhCallers = cg.getCallers( mainModule->getFunction( MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION ) );
	for ( std::set<llvm::Instruction *>::iterator it = mhCallers.begin(), ie = mhCallers.end(); it != ie; it++ )
		messageHandlers.insert( (*it)->getParent()->getParent() );
	if ( messageHandlers.empty() ) {
		CLOUD9_ERROR( "No message handlers found." );
		done();
	}

	// Find interesting instructions.
	CLOUD9_DEBUG( "   Searching for interesting instructions." );
	std::set<llvm::Instruction *> interestingInstructions = cg.getCallers( mainModule->getFunction( MAX_INTERESTING_ANNOTATION_FUNCTION ) );
	if ( interestingInstructions.empty() ) {
		CLOUD9_ERROR( "No interesting statements found." );
		done();
	}

	// Find all instructions that are reachable from message handlers.
	CLOUD9_DEBUG( "   Searching forward from handlers." );
	std::set<llvm::Instruction *> reachedFromHandler;
	// Use a work list to add all successors of message handler entry instructions.
	std::set<llvm::Instruction *> worklist;
	for ( std::set<llvm::Function *>::iterator it = messageHandlers.begin(), ie = messageHandlers.end(); it != ie; it++ )
		worklist.insert( &((*it)->getEntryBlock().front()) );
	while ( ! worklist.empty() ) {
		std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
		Instruction *inst = *it;
		worklist.erase( it );
		
		// Mark instruction as reachable.
		reachedFromHandler.insert( inst );
		std::set<llvm::Instruction *> s = cfg.getSuccessors( inst );
		// Add all not reached successors to work list.
		for ( it = s.begin(), ie = s.end(); it != ie; it++ )
			if ( reachedFromHandler.count( *it ) == 0 )
				worklist.insert( *it );
		// Check for CallInst or InvokeInst and add entire function to work list.
		llvm::Function *fn = NULL;
		if ( llvm::InvokeInst *ii = dyn_cast<llvm::InvokeInst>( inst ) )
			fn = ii->getCalledFunction();
		if ( llvm::CallInst *ci = dyn_cast<llvm::CallInst>( inst ) )
			fn = ci->getCalledFunction();
		if ( fn != NULL )
			for ( CFG::iterator it2 = cfg.begin(), ie2 = cfg.end(); it2 != ie2; it2++ )
				if ( (*it2)->getParent()->getParent() == fn && reachedFromHandler.count( *it2 ) == 0 )
					worklist.insert( *it2 );
	}
	// Find all instructions that can reach interesting statements.
	CLOUD9_DEBUG( "   Searching backward from interesting instructions." );
	std::set<llvm::Instruction *> reachesInteresting;
	// Use a worklist to add all predecessors of interesting instructions.
	worklist = interestingInstructions;
	while ( ! worklist.empty() ) {
		std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
		Instruction *inst = *it;
		worklist.erase( it );

		// Mark instruction as reaching.
		reachesInteresting.insert( inst );
		std::set<llvm::Instruction *> p = cfg.getPredecessors( inst );
		// Add all non-reaching predecessors to work list.
		for ( it = p.begin(), ie = p.end(); it != ie; it++ )
			if ( reachesInteresting.count( *it ) == 0 )
				worklist.insert( *it );
		// Check for CallInst or InvokeInst and add entire function to work list.
		llvm::Function *fn = NULL;
		if ( llvm::InvokeInst *ii = dyn_cast<llvm::InvokeInst>( inst ) )
			fn = ii->getCalledFunction();
		if ( llvm::CallInst *ci = dyn_cast<llvm::CallInst>( inst ) )
			fn = ci->getCalledFunction();
		if ( fn != NULL )
			for ( CFG::iterator it2 = cfg.begin(), ie2 = cfg.end(); it2 != ie2; it2++ )
				if ( (*it2)->getParent()->getParent() == fn && reachesInteresting.count( *it2 ) == 0 )
					worklist.insert( *it2 );
		// Check if entry instruction.
		if ( inst == &(inst->getParent()->getParent()->getEntryBlock().front()) ) {
			p = cg.getCallers( inst->getParent()->getParent() );
			// Add all useless callers to work list.
			for ( it = p.begin(), ie = p.end(); it != ie; it++ )
				if ( reachesInteresting.count( *it ) == 0 )
					worklist.insert( *it );
		}
	}
	// Determine useless instructions (can't reach interesting statements after starting at message handler).
	CLOUD9_DEBUG( "   Defining useless instructions." );
	std::set<llvm::Instruction *> uselessInstructions;
	for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
		if ( reachedFromHandler.count( *it ) == 0 || reachesInteresting.count( *it ) == 0 )
			uselessInstructions.insert( *it );

	if ( DumpCFG.size() > 0 )
		dumpCFG( cfg, cg, messageHandlers, interestingInstructions, uselessInstructions );

	// Add entry function.
	CLOUD9_INFO( "Generating entry function." );
	// Rename old main function
	Function *oldEntryFunction = mainModule->getFunction( MAX_ENTRY_FUNCTION );
	oldEntryFunction->setName( MAX_OLD_ENTRY_FUNCTION );

	Function *entryFunction = Function::Create(
		oldEntryFunction->getFunctionType(),
		GlobalValue::ExternalLinkage,
		MAX_ENTRY_FUNCTION,
		mainModule );
	entryFunction->setCallingConv( CallingConv::C );
	oldEntryFunction->replaceAllUsesWith( entryFunction );

	Function *kleeIntFunction = mainModule->getFunction( "klee_int" );
	if ( ! kleeIntFunction ) {
		kleeIntFunction = Function::Create(
			FunctionType::get(
				IntegerType::get( mainModule->getContext(), 32 ),
				ArrayRef<const Type *>( PointerType::get( IntegerType::get(mainModule->getContext(), 8 ), 0 ) ).vec(),
				false ),
			GlobalValue::ExternalLinkage,
			"klee_int",
			mainModule );
		
			kleeIntFunction->setCallingConv( CallingConv::C );
	}

	GlobalVariable *handlerIDVarName = new GlobalVariable(
		*mainModule,
		ArrayType::get( IntegerType::get( mainModule->getContext(), 8 ), strlen( MAX_HANDLER_ID_VAR_NAME ) + 1 ),
		true,
		GlobalValue::PrivateLinkage,
		NULL,
		MAX_HANDLER_ID_VAR_NAME );
	handlerIDVarName->setInitializer( ConstantArray::get(mainModule->getContext(), MAX_HANDLER_ID_VAR_NAME, true ) );

	Function::arg_iterator args = entryFunction->arg_begin();
	Value *argcVar = args++;
	argcVar->setName( "argc" );
	Value *argvVar = args++;
	argvVar->setName( "argv" );

	BasicBlock* entryBB = BasicBlock::Create( mainModule->getContext(), "", entryFunction, 0);
	BasicBlock* returnBB = BasicBlock::Create( mainModule->getContext(), "", entryFunction, 0);

	new StoreInst( argcVar, new AllocaInst( IntegerType::get( mainModule->getContext(), 32 ), "", entryBB ), false, entryBB );
	new StoreInst( argvVar, new AllocaInst( PointerType::get( PointerType::get( IntegerType::get( mainModule->getContext(), 8 ), 0 ), 0 ), "", entryBB ), false, entryBB );
	CallInst *maxInitCall = CallInst::Create( mainModule->getFunction( "max_init" ), "", entryBB);
	maxInitCall->setCallingConv(CallingConv::C);
	maxInitCall->setTailCall(false);
	Constant *idxs[] = {ConstantInt::get( mainModule->getContext(), APInt( 32, 0, true ) ), ConstantInt::get( mainModule->getContext(), APInt( 32, 0, true ) )};
	CallInst *kleeIntCall = CallInst::Create(
		kleeIntFunction,
		ConstantExpr::getGetElementPtr( handlerIDVarName, idxs, 2, false ),
		"", entryBB);
	kleeIntCall->setCallingConv(CallingConv::C);
	kleeIntCall->setTailCall(false);

	SwitchInst* switchInst = SwitchInst::Create( kleeIntCall, returnBB, messageHandlers.size() + 1, entryBB );

	uint32_t handlerID = 1;
	for ( std::set<llvm::Function *>::iterator hit = messageHandlers.begin(), hie = messageHandlers.end(); hit != hie; hit++ ) {
		BasicBlock *swBB = BasicBlock::Create( mainModule->getContext(), "", entryFunction, 0 );
		switchInst->addCase( ConstantInt::get( mainModule->getContext(), APInt( 32, handlerID++, true ) ), swBB );

		CallInst *handlerCallInst = CallInst::Create( *hit, "", swBB );
		handlerCallInst->setCallingConv( CallingConv::C );
		handlerCallInst->setTailCall( false );

		BranchInst::Create(returnBB, swBB);
	}

	ReturnInst::Create( mainModule->getContext(), ConstantInt::get( mainModule->getContext(), APInt( 32, 0, true ) ), returnBB );

	// Create the job manager
	UseInstructionFiltering = true;
	theJobManager = new cloud9::worker::JobManager(mainModule, "main", pArgc, pArgv, envp);
	klee::FilteringSearcher::setInstructionFilter( uselessInstructions );
	(dynamic_cast<cloud9::worker::SymbolicEngine*>(theJobManager->getInterpreter()))
		->registerStateEventHandler( new MaxStateEventHandler() );

	if (StandAlone) {
	  CLOUD9_INFO("Running in stand-alone mode. No load balancer involved.");

	  theJobManager->processJobs(true, (int)MaxTime.getValue());

	  cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::TimeOut, "Timeout");

	  theJobManager->finalize();
	} else {
      cloud9::worker::CommManager commManager(theJobManager); // Handle outside communication
      commManager.setup();

      theJobManager->processJobs(false, (int)MaxTime.getValue()); // Blocking when no jobs are on the queue

      cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::TimeOut, "Timeout");

      // The order here matters, in order to avoid memory corruption
      commManager.finalize();
      theJobManager->finalize();
	}

	delete theJobManager;
	theJobManager = NULL;

	return 0;
}
