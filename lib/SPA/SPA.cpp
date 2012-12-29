/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <list>
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
#include "llvm/InstrTypes.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Type.h"
#include "llvm/Instructions.h"

#include "klee/Internal/System/Time.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "klee/Init.h"
#include "klee/Executor.h"
#include "klee/Searcher.h"
#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include <expr/Parser.h>
#include "../../lib/Core/Context.h"
#include "../../lib/Core/TimingSolver.h"

#include "cloud9/Logger.h"
#include "cloud9/ExecutionTree.h"
#include "cloud9/ExecutionPath.h"
#include "cloud9/Protocols.h"
#include "cloud9/worker/TreeNodeInfo.h"
#include "cloud9/worker/JobManager.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/CoreStrategies.h"
#include "cloud9/worker/KleeCommon.h"
#include "cloud9/worker/CommManager.h"
#include "cloud9/Utils.h"
#include "cloud9/instrum/InstrumentationManager.h"

#include <spa/SPA.h>
#include <spa/SpaSearcher.h>
#include <spa/Path.h>

#define MAIN_ENTRY_FUNCTION				"__user_main"
#define ALTERNATIVE_MAIN_ENTRY_FUNCTION	"main"
#define OLD_ENTRY_FUNCTION				"__spa_old_main"
#define KLEE_INT_FUNCTION				"klee_int"
#define MALLOC_FUNCTION					"malloc"
#define INIT_VALUE_ID_VAR_NAME			"spa_internal_initValueID"
#define HANDLER_ID_VAR_NAME				"spa_internal_HanderID"
#define SEED_ID_VAR_NAME				"spa_internal_SeedID"
extern cl::opt<double> MaxTime;
extern cl::opt<bool> NoOutput;

namespace {
	cl::opt<bool> StandAlone("stand-alone",
		cl::desc("Enable running a worker in stand alone mode"),
		cl::init(true));

	typedef enum {
		START,
		PATH,
		SYMBOL_NAME,
		SYMBOL_ARRAY,
		SYMBOL_VALUE,
		SYMBOL_END,
		TAG_KEY,
		TAG_VALUE,
		CONSTRAINTS,
		PATH_DONE
	} LoadState_t;
}

namespace SPA {
	cl::opt<std::string> RecoverState( "recover-state",
		llvm::cl::desc( "Specifies a file with a previously saved processing queue to load." ) );

	static bool Interrupted = false;
	cloud9::worker::JobManager *theJobManager;

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

	SPA::SPA( llvm::Module *_module, std::ostream &_output ) :
		module( _module ), output( _output ),
		pathFilter( NULL ), outputTerminalPaths( true ),
		checkpointsFound( 0 ), filteredPathsFound( 0 ), terminalPathsFound( 0 ), outputtedPaths( 0 ) {

		// Make sure to clean up properly before any exit point in the program
		atexit(llvm::llvm_shutdown);

		GOOGLE_PROTOBUF_VERIFY_VERSION;

		cloud9::Logger::getLogger().setLogPrefix("Worker<   >: ");

		// JIT initialization
		llvm::InitializeNativeTarget();

		sys::PrintStackTraceOnErrorSignal();

		cloud9::initBreakSignal();

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
				exit( returnCode );
			}
		}

		// At this point, if the watchdog is enabled, we are in the child process of
		// the fork().

		// Take care of Ctrl+C requests
		sys::SetInterruptFunction(interrupt_handle);

		generateMain();
	}

	/**
	 * Generates a new main function that looks like:
	 * 
	 *	int main( int argc, char **argv ) {
	 * 		init1();
	 * 		init2();
	 * 		(...)
	 *		int initValueID = klee_int( "initValueID" );
	 *		switch ( initValueID ) {
	 *			case 1:
	 * 				var = malloc( value.length );
	 * 				var[0] = value[0]; (...);
	 * 			break;
	 *			(...)
	 * 			default: return 0;
	 *		}
	 *		int handlerID = klee_int( "handlerID" );
	 *		switch ( handlerID ) {
	 *			case 1: handler1(); break;
	 *			case 2: spa_internal_SeedID = seedID; handler2(); break;
	 *			(...)
	 *		}
	 * 		(...)
	 *		return 0;
	 *	}
	 */
	void SPA::generateMain() {
		// Rename old main function
		llvm::Function *oldEntryFunction = module->getFunction( MAIN_ENTRY_FUNCTION );
		if ( ! oldEntryFunction )
			oldEntryFunction = module->getFunction( ALTERNATIVE_MAIN_ENTRY_FUNCTION );
		assert( oldEntryFunction && "No main function found to replace." );
		std::string entryFunctionName = oldEntryFunction->getName().str();
		oldEntryFunction->setName( OLD_ENTRY_FUNCTION );
		// Create new one.
		entryFunction = llvm::Function::Create(
			oldEntryFunction->getFunctionType(),
			llvm::GlobalValue::ExternalLinkage, entryFunctionName, module );
		entryFunction->setCallingConv( llvm::CallingConv::C );
		// Replace the old with the new.
		oldEntryFunction->replaceAllUsesWith( entryFunction );

		// Declare arguments.
		llvm::Function::arg_iterator args = entryFunction->arg_begin();
		llvm::Value *argcVar = args++;
		argcVar->setName( "argc" );
		llvm::Value *argvVar = args++;
		argvVar->setName( "argv" );

		// Create the entry, current and next handler, and return basic blocks.
		llvm::BasicBlock* initBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0);
		firstHandlerBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0);
		nextHandlerBB = firstHandlerBB;
		returnBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0);

		// Allocate arguments.
		new llvm::StoreInst( argcVar, new llvm::AllocaInst( llvm::IntegerType::get( module->getContext(), 32 ), "", initBB ), false, initBB );
		new llvm::StoreInst( argvVar, new llvm::AllocaInst( llvm::PointerType::get( llvm::PointerType::get( llvm::IntegerType::get( module->getContext(), 8 ), 0 ), 0 ), "", initBB ), false, initBB );

		// Get klee_int function.
		llvm::Function *kleeIntFunction = module->getFunction( KLEE_INT_FUNCTION );
		if ( ! kleeIntFunction ) {
			kleeIntFunction = llvm::Function::Create(
				llvm::FunctionType::get(
					llvm::IntegerType::get( module->getContext(), 32 ),
					llvm::ArrayRef<const llvm::Type *>( llvm::PointerType::get( llvm::IntegerType::get( module->getContext(), 8 ), 0 ) ).vec(),
					false ),
				llvm::GlobalValue::ExternalLinkage, KLEE_INT_FUNCTION, module );
			kleeIntFunction->setCallingConv( llvm::CallingConv::C );
		}
		// Create initValueID variable.
		llvm::GlobalVariable *initValueIDVarName = new llvm::GlobalVariable(
			*module,
			llvm::ArrayType::get( llvm::IntegerType::get( module->getContext(), 8 ), strlen( INIT_VALUE_ID_VAR_NAME ) + 1 ),
			true,
			llvm::GlobalValue::PrivateLinkage,
			NULL,
			INIT_VALUE_ID_VAR_NAME );
		initValueIDVarName->setInitializer( llvm::ConstantArray::get( module->getContext(), INIT_VALUE_ID_VAR_NAME, true ) );
		// initValueID = klee_int();
		llvm::Constant *idxs[] = {llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) ), llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) )};
		llvm::CallInst *kleeIntCall = llvm::CallInst::Create(
			kleeIntFunction,
			llvm::ConstantExpr::getGetElementPtr( initValueIDVarName, idxs, 2, false ),
			"", initBB );
		kleeIntCall->setCallingConv(llvm::CallingConv::C);
		kleeIntCall->setTailCall(false);
		// Init handlers will be later added before this point.
		initHandlerPlaceHolder = kleeIntCall;
		// switch ( handlerID )
		initValueSwitchInst = llvm::SwitchInst::Create( kleeIntCall, returnBB, 1, initBB );
		// Entry handlers will be later added starting with id = 1.
		initValueID = 1;

		// Create handlerID variable.
		handlerIDVarName = new llvm::GlobalVariable(
			*module,
			llvm::ArrayType::get( llvm::IntegerType::get( module->getContext(), 8 ), strlen( HANDLER_ID_VAR_NAME ) + 1 ),
			true,
			llvm::GlobalValue::PrivateLinkage,
			NULL,
			HANDLER_ID_VAR_NAME );
		handlerIDVarName->setInitializer( llvm::ConstantArray::get( module->getContext(), HANDLER_ID_VAR_NAME, true ) );

		// Set up basic blocks to add entry handlers.
		llvm::BranchInst::Create( returnBB, nextHandlerBB );
		newEntryLevel();

		// return 0;
		llvm::ReturnInst::Create( module->getContext(), llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) ), returnBB );

// 		module->dump();
// 		entryFunction->dump();
	}

	void SPA::addInitFunction( llvm::Function *fn ) {
		llvm::CallInst *initCall = llvm::CallInst::Create( fn, "", initHandlerPlaceHolder );
		initCall->setCallingConv(llvm::CallingConv::C);
		initCall->setTailCall(false);
	}

	void SPA::addEntryFunction( llvm::Function *fn ) {
		// case x:
		// Create basic block for this case.
		llvm::BasicBlock *swBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0 );
		entryHandlerSwitchInst->addCase( llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, entryHandlerID++, true ) ), swBB );
		// handlerx();
		llvm::CallInst *handlerCallInst = llvm::CallInst::Create( fn, "", swBB );
		handlerCallInst->setCallingConv( llvm::CallingConv::C );
		handlerCallInst->setTailCall( false );
		// break;
		llvm::BranchInst::Create(nextHandlerBB, swBB);
	}

	void SPA::addSeedEntryFunction( unsigned int seedID, llvm::Function *fn ) {
		// case x:
		// Create basic block for this case.
		llvm::BasicBlock *swBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0 );
		entryHandlerSwitchInst->addCase( llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, entryHandlerID++, true ) ), swBB );

		// spa_internal_SeedID = seedID;
		llvm::GlobalVariable *seedIDVar = module->getNamedGlobal ( SEED_ID_VAR_NAME );
		assert( seedIDVar && "SeedID variable not declared in module." );
		new StoreInst( ConstantInt::get( module->getContext(), APInt( 32, (uint64_t) seedID, false ) ), seedIDVar );

		// handlerx();
		llvm::CallInst *handlerCallInst = llvm::CallInst::Create( fn, "", swBB );
		handlerCallInst->setCallingConv( llvm::CallingConv::C );
		handlerCallInst->setTailCall( false );
		// break;
		llvm::BranchInst::Create(nextHandlerBB, swBB);
	}

	void SPA::newEntryLevel() {
		// Get klee_int function.
		llvm::Function *kleeIntFunction = module->getFunction( KLEE_INT_FUNCTION );
		if ( ! kleeIntFunction ) {
			kleeIntFunction = llvm::Function::Create(
				llvm::FunctionType::get(
					llvm::IntegerType::get( module->getContext(), 32 ),
					llvm::ArrayRef<const llvm::Type *>( llvm::PointerType::get( llvm::IntegerType::get( module->getContext(), 8 ), 0 ) ).vec(),
					false ),
				llvm::GlobalValue::ExternalLinkage, KLEE_INT_FUNCTION, module );
			kleeIntFunction->setCallingConv( llvm::CallingConv::C );
		}

		// Remove the temporary branch instruction.
		nextHandlerBB->begin()->eraseFromParent();
		// handlerID = klee_int();
		llvm::Constant *idxs[] = {llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) ), llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) )};
		llvm::CallInst *kleeIntCall = llvm::CallInst::Create(
			kleeIntFunction,
			llvm::ConstantExpr::getGetElementPtr( handlerIDVarName, idxs, 2, false ),
			"", nextHandlerBB );
		kleeIntCall->setCallingConv(llvm::CallingConv::C);
		kleeIntCall->setTailCall(false);
		// switch ( handlerID )
		entryHandlerSwitchInst = llvm::SwitchInst::Create( kleeIntCall, returnBB, 1, nextHandlerBB );
		// Entry handlers will be later added starting with id = 1.
		entryHandlerID = 1;

		nextHandlerBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0);
		llvm::BranchInst::Create( returnBB, nextHandlerBB );
	}

	void SPA::addInitialValues( std::map<llvm::Value *, std::vector<uint8_t> > values ) {
		// Get malloc function.
		llvm::Function *mallocFunction = module->getFunction( MALLOC_FUNCTION );
		if ( ! mallocFunction ) {
			mallocFunction = llvm::Function::Create(
				FunctionType::get(
					PointerType::get( IntegerType::get( module->getContext(), 8 ), 0 ),
								  llvm::ArrayRef<const llvm::Type *>( IntegerType::get( module->getContext(), 64 ) ).vec(),
								  false ),
								  GlobalValue::ExternalLinkage, "malloc", module );
			mallocFunction->setCallingConv( CallingConv::C );
		}

		// case x:
		// Create basic block for this case.
		llvm::BasicBlock *swBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0 );
		initValueSwitchInst->addCase( llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, initValueID++, true ) ), swBB );

		// Iterate over values to initialize.
		for ( std::map<llvm::Value *, std::vector<uint8_t> >::iterator it = values.begin(), ie = values.end(); it != ie; it++ ) {
			// %1 = malloc( value.length );
			CallInst* mallocCallInst = CallInst::Create( mallocFunction, llvm::ConstantInt::get( module->getContext(), llvm::APInt( 64, it->second.size(), true ) ), "", swBB );
			mallocCallInst->setCallingConv( CallingConv::C );
			mallocCallInst->setTailCall( true );
			// var = %1
			new StoreInst( mallocCallInst, it->first, false, swBB );

			// Iterator over initial value bytes.
			for ( unsigned int i = 0; i < it->second.size(); i++ ) {
				// var[i] = value[i];
				new StoreInst(
					llvm::ConstantInt::get( module->getContext(), llvm::APInt( 8, it->second[i], true ) ),
					GetElementPtrInst::Create( mallocCallInst, llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, i, true ) ), "", swBB), false, swBB );
			}
		}

		// break;
		llvm::BranchInst::Create( firstHandlerBB, swBB );
	}

	void SPA::addSymbolicInitialValues() {
		addInitialValues( std::map<llvm::Value *, std::vector<uint8_t> >() );
	}

	void SPA::start() {
// 		entryFunction->dump();

		bool outputFP = false;
		for ( std::deque<bool>::iterator it = outputFilteredPaths.begin(), ie = outputFilteredPaths.end(); it != ie; it++ ) {
			if ( *it ) {
				outputFP = true;
				break;
			}
		}
		assert( (outputFP || outputTerminalPaths || ! checkpoints.empty()) && "No points to output data from." );

		int pArgc;
		char **pArgv;
		char **pEnvp;
		char *envp[] = { NULL };
		klee::readProgramArguments(pArgc, pArgv, pEnvp, envp );

		// Create the job manager
		NoOutput = true;
		theJobManager = new cloud9::worker::JobManager( module, "main", pArgc, pArgv, pEnvp );

		if ( ! stateUtilities.empty() ) {
			CLOUD9_INFO( "Replacing strategy stack with SPA utility framework." );
			SpaSearcher *spaSearcher = new SpaSearcher( theJobManager, stateUtilities );
			spaSearcher->addFilteringEventHandler( this );
			theJobManager->setStrategy(
				new cloud9::worker::RandomJobFromStateStrategy(
					theJobManager->getTree(),
					new cloud9::worker::KleeStrategy(
						theJobManager->getTree(),
						spaSearcher ),
					theJobManager ) );
		}

		(dynamic_cast<cloud9::worker::SymbolicEngine*>(theJobManager->getInterpreter()))
			->registerStateEventHandler( this );

		if (StandAlone) {
			CLOUD9_INFO("Running in stand-alone mode. No load balancer involved.");

			if ( RecoverState.size() > 0 ) {
				CLOUD9_DEBUG( "Recovering state from: " << RecoverState.getValue() );
				std::ifstream stateFile( RecoverState.getValue().c_str() );
				assert( stateFile.is_open() && "Unable to open state file." );
				cloud9::ExecutionPathSetPin epsp = cloud9::ExecutionPathSet::parse( stateFile );
				stateFile.close();
				std::vector<long> replayInstrs;
				theJobManager->importJobs(epsp, replayInstrs);
				theJobManager->processLoop(true, false, (int)MaxTime.getValue());
			} else {
				theJobManager->processJobs(true, (int)MaxTime.getValue());
			}

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
	}

	void SPA::showStats() {
		CLOUD9_DEBUG( "Checkpoints: " << checkpointsFound
			<< "; TerminalPaths: " << terminalPathsFound
			<< "; Outputted: " << outputtedPaths
			<< "; Filtered: " << filteredPathsFound );
	}

	void SPA::processPath( klee::ExecutionState *state ) {
		Path path( state );

		if ( ! pathFilter || pathFilter->checkPath( path ) ) {
			CLOUD9_DEBUG( "Outputting path." );
			output << path;
			outputtedPaths++;
		}
	}

	void SPA::onControlFlowEvent( klee::ExecutionState *kState, cloud9::worker::ControlFlowEvent event ) {
// 		if ( event == cloud9::worker::STEP ) {
// 			CLOUD9_DEBUG( "Current state at:" );
// 			klee::c9::printStateStack( std::cerr, *kState ) << std::endl;
// 		}

		if ( event == cloud9::worker::STEP && checkpoints.count( kState->pc()->inst ) ) {
			CLOUD9_DEBUG( "Processing checkpoint path." );
			checkpointsFound++;
			processPath( kState );
			showStats();
		}
	}

	void SPA::onStateDestroy(klee::ExecutionState *kState, bool silenced) {
// 		CLOUD9_DEBUG( "Destroying path at:" );
// 		klee::c9::printStateStack( std::cerr, *kState ) << std::endl;

		if ( ! kState->filtered ) {
			terminalPathsFound++;

			if ( outputTerminalPaths ) {
				assert( kState );

				CLOUD9_DEBUG( "Processing terminal path." );

				processPath( kState );
			}
		} else {
			CLOUD9_DEBUG( "Filtered path destroyed." );
		}
		showStats();
	}

	void SPA::onStateFiltered( klee::ExecutionState *state, unsigned int id ) {
		filteredPathsFound++;

		if ( outputFilteredPaths[id] ) {
			assert( state );

			CLOUD9_DEBUG( "Processing filtered path." );

			processPath( state );
		}
		showStats();
	}
}
