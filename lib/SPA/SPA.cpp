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
#include <spa/Path.h>

#define MAIN_ENTRY_FUNCTION	"__user_main"
#define OLD_ENTRY_FUNCTION	"__spa_old_user_main"
#define KLEE_INT_FUNCTION	"klee_int"
#define HANDLER_ID_VAR_NAME	"spa_internal_HanderID"
extern cl::opt<double> MaxTime;
extern cl::opt<bool> NoOutput;
extern cl::opt<bool> UseInstructionFiltering;

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
		module( _module ),
		outputTerminalPaths( true ),
		instructionFilter( NULL ),
		pathFilter( NULL ),
		output( _output ) {

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
	}

	/**
	 * Generates a new main function that looks like:
	 * 
	 *	int main( int argc, char **argv ) {
	 * 		init1();
	 * 		init2();
	 * 		(...)
	 *		int handlerID = klee_int( "handlerID" );
	 *		switch ( handlerID ) {
	 *			case 1: handler1(); break;
	 *			case 2: handler1(); break;
	 *			(...)
	 *		}
	 *		return 0;
	 *	}
	 */
	void SPA::generateMain() {
		// Rename old main function
		llvm::Function *oldEntryFunction = module->getFunction( MAIN_ENTRY_FUNCTION );
		oldEntryFunction->setName( OLD_ENTRY_FUNCTION );
		// Create new one.
		llvm::Function *entryFunction = llvm::Function::Create(
			oldEntryFunction->getFunctionType(),
			llvm::GlobalValue::ExternalLinkage, MAIN_ENTRY_FUNCTION, module );
		entryFunction->setCallingConv( llvm::CallingConv::C );
		// Replace the old with the new.
		oldEntryFunction->replaceAllUsesWith( entryFunction );

		// Declare arguments.
		llvm::Function::arg_iterator args = entryFunction->arg_begin();
		llvm::Value *argcVar = args++;
		argcVar->setName( "argc" );
		llvm::Value *argvVar = args++;
		argvVar->setName( "argv" );

		// Create the entry and return basic blocks.
		llvm::BasicBlock* entryBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0);
		llvm::BasicBlock* returnBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0);

		// Allocate arguments.
		new llvm::StoreInst( argcVar, new llvm::AllocaInst( llvm::IntegerType::get( module->getContext(), 32 ), "", entryBB ), false, entryBB );
		new llvm::StoreInst( argvVar, new llvm::AllocaInst( llvm::PointerType::get( llvm::PointerType::get( llvm::IntegerType::get( module->getContext(), 8 ), 0 ), 0 ), "", entryBB ), false, entryBB );
		// Init functions;
		for ( std::list<llvm::Function *>::iterator it = initFunctions.begin(), ie = initFunctions.end(); it != ie; it++ ) {
			llvm::CallInst *initCall = llvm::CallInst::Create( *it, "", entryBB);
			initCall->setCallingConv(llvm::CallingConv::C);
			initCall->setTailCall(false);
		}
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
		// Create handlerID variable.
		llvm::GlobalVariable *handlerIDVarName = new llvm::GlobalVariable(
			*module,
			llvm::ArrayType::get( llvm::IntegerType::get( module->getContext(), 8 ), strlen( HANDLER_ID_VAR_NAME ) + 1 ),
																		  true,
																	llvm::GlobalValue::PrivateLinkage,
																	NULL,
																	HANDLER_ID_VAR_NAME );
		handlerIDVarName->setInitializer( llvm::ConstantArray::get( module->getContext(), HANDLER_ID_VAR_NAME, true ) );
		// handlerID = klee_int();
		llvm::Constant *idxs[] = {llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) ), llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) )};
		llvm::CallInst *kleeIntCall = llvm::CallInst::Create(
			kleeIntFunction,
			llvm::ConstantExpr::getGetElementPtr( handlerIDVarName, idxs, 2, false ),
			"", entryBB);
		kleeIntCall->setCallingConv(llvm::CallingConv::C);
		kleeIntCall->setTailCall(false);
		// switch ( handlerID )
		llvm::SwitchInst* switchInst = llvm::SwitchInst::Create( kleeIntCall, returnBB, entryFunctions.size() + 1, entryBB );
		// case x:
		uint32_t handlerID = 1;
		for ( std::list<llvm::Function *>::iterator it = entryFunctions.begin(), ie = entryFunctions.end(); it != ie; it++ ) {
			// Create basic block for this case.
			llvm::BasicBlock *swBB = llvm::BasicBlock::Create( module->getContext(), "", entryFunction, 0 );
			switchInst->addCase( llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, handlerID++, true ) ), swBB );
			// handlerx();
			llvm::CallInst *handlerCallInst = llvm::CallInst::Create( *it, "", swBB );
			handlerCallInst->setCallingConv( llvm::CallingConv::C );
			handlerCallInst->setTailCall( false );
			// break;
			llvm::BranchInst::Create(returnBB, swBB);
		}
		// return 0;
		llvm::ReturnInst::Create( module->getContext(), llvm::ConstantInt::get( module->getContext(), llvm::APInt( 32, 0, true ) ), returnBB );

// 		module->dump();
		// 		entryFunction->dump();
	}

	void SPA::start() {
		assert( (outputTerminalPaths || ! checkpoints.empty()) && "No points to output data from." );

		generateMain();

		int pArgc;
		char **pArgv;
		char **pEnvp;
		char *envp[] = { NULL };
		klee::readProgramArguments(pArgc, pArgv, pEnvp, envp );

		// Create the job manager
		NoOutput = true;
		theJobManager = new cloud9::worker::JobManager( module, "main", pArgc, pArgv, pEnvp );

		if ( instructionFilter ) {
			CLOUD9_INFO( "Replacing strategy stack with SPA filtering." );
			theJobManager->setStrategy(
				new cloud9::worker::RandomJobFromStateStrategy(
					theJobManager->getTree(),
					new cloud9::worker::KleeStrategy(
						theJobManager->getTree(),
						new klee::FilteringSearcher( instructionFilter ) ),
					theJobManager ) );
		}

		(dynamic_cast<cloud9::worker::SymbolicEngine*>(theJobManager->getInterpreter()))
			->registerStateEventHandler( this );

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
	}

	void SPA::onControlFlowEvent( klee::ExecutionState *kState, cloud9::worker::ControlFlowEvent event ) {
		assert( kState );

		if ( event == cloud9::worker::STEP && checkpoints.count( kState->pc()->inst ) ) {
			CLOUD9_DEBUG( "Processing checkpoint path." );

			Path path( kState );

			if ( ! pathFilter || pathFilter->checkPath( path ) ) {
				CLOUD9_DEBUG( "Outputting path." );
				output << path;
			}
		}
	}

	void SPA::onStateDestroy(klee::ExecutionState *kState, bool silenced) {
		if ( outputTerminalPaths ) {
			assert( kState );

			CLOUD9_DEBUG( "Processing terminal path." );

			Path path( kState );

			if ( ! pathFilter || pathFilter->checkPath( path ) ) {
				CLOUD9_DEBUG( "Outputting path." );
				output << path;
			}
		}
	}
}
