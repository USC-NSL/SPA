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

#include <spa/SPA.h>

#define MAIN_ENTRY_FUNCTION	"__user_main"
#define OLD_ENTRY_FUNCTION	"__spa_old_user_main"
#define SPA_INIT_FUNCTION	"spa_init"
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
		PATH_STARTED,
		SYMBOLS,
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

		int pArgc;
		char **pArgv;
		char **pEnvp;
		klee::readProgramArguments(pArgc, pArgv, pEnvp, NULL);

		// Create the job manager
		NoOutput = true;
		theJobManager = new cloud9::worker::JobManager( module, "main", pArgc, pArgv, NULL );
		(dynamic_cast<cloud9::worker::SymbolicEngine*>(theJobManager->getInterpreter()))
			->registerStateEventHandler( this );
	}

	/**
	 * Generates a new main function that looks like:
	 * 
	 *	int main( int argc, char **argv ) {
	 * 		spa_init();
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
		// spa_init();
		llvm::CallInst *spaInitCall = llvm::CallInst::Create( module->getFunction( SPA_INIT_FUNCTION ), "", entryBB);
		spaInitCall->setCallingConv(llvm::CallingConv::C);
		spaInitCall->setTailCall(false);
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
	}

	void SPA::start() {
		if ( instructionFilter ) {
			UseInstructionFiltering = true;
			klee::FilteringSearcher::setInstructionFilter( instructionFilter );
		}

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

	std::string cleanUpLine( std::string line ) {
		// Remove comments.
		line = line.substr( 0, line.find( SPA_PATH_COMMENT ) );
		// Remove trailing white space.
		line = line.substr( 0, line.find_last_not_of( SPA_PATH_WHITE_SPACE ) + 1 );
		// Remove leading white space.
		if ( line.find_first_not_of( SPA_PATH_WHITE_SPACE ) != line.npos )
			line = line.substr( line.find_first_not_of( SPA_PATH_WHITE_SPACE ), line.npos );
		
		return line;
	}

	#define changeState( from, to ) \
		assert( state == from && "Invalid path file." ); \
		state = to;

	std::map<std::string, std::set< std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> > > SPA::loadPaths( std::istream pathFile ) {
		std::map<std::string, std::set< std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> > > paths;
		int numPaths = 0;

		LoadState_t state = PATH_DONE;
		std::string handlerName = "";
		std::string kQuery = "";
		std::map<std::string, std::string> symbols;
		std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> pathData;
		while ( pathFile.good() ) {
			std::string line;
			getline( pathFile, line );
			line = cleanUpLine( line );
			if ( line.empty() )
				continue;

			if ( line == SPA_PATH_START ) {
				changeState( PATH_DONE, PATH_STARTED );
				handlerName.clear();
			} else if ( line == SPA_PATH_SYMBOLS_START ) {
				changeState( PATH_STARTED, SYMBOLS );
				assert( (! handlerName.empty()) && "No handler name found." );
			} else if ( line == SPA_PATH_SYMBOLS_END ) {
				changeState( SYMBOLS, PATH_STARTED );
			} else if ( line == SPA_PATH_CONSTRAINTS_START ) {
				changeState( PATH_STARTED, CONSTRAINTS );
				kQuery = "";
			} else if ( line == SPA_PATH_CONSTRAINTS_END ) {
				changeState( CONSTRAINTS, PATH_STARTED );

				llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer( kQuery );
				klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
				klee::expr::Parser *P = klee::expr::Parser::Create( "", MB, Builder );
				while ( klee::expr::Decl *D = P->ParseTopLevelDecl() ) {
					assert( ! P->GetNumErrors() && "Error parsing constraints in path file." );
					if ( klee::expr::QueryCommand *QC = dyn_cast<klee::expr::QueryCommand>( D ) ) {
						pathData.second = new klee::ConstraintManager( QC->Constraints );
						delete D;
						break;
					} else if ( klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>( D ) ) {
						pathData.first[symbols[AD->Name->Name]] = AD->Root;
					}
				}
				delete P;
				delete Builder;
				delete MB;
			} else if ( line == SPA_PATH_END ) {
				changeState( PATH_STARTED, PATH_DONE );
				paths[handlerName].insert( pathData );
				numPaths++;
			} else {
				switch ( state ) {
					case PATH_STARTED : {
						assert( handlerName.empty() && "Handler name multiply defined." );
						handlerName = line;
					} break;
					case SYMBOLS : {
						assert( line.find( SPA_PATH_SYMBOLS_DELIMITER ) != line.npos && line.find( SPA_PATH_SYMBOLS_DELIMITER ) + strlen( SPA_PATH_SYMBOLS_DELIMITER ) < line.size() && "Invalid symbol definition." );
						std::string name = line.substr( 0, line.find( SPA_PATH_SYMBOLS_DELIMITER ) );
						std::string symbol = line.substr( line.find( SPA_PATH_SYMBOLS_DELIMITER ) + strlen( SPA_PATH_SYMBOLS_DELIMITER ), line.npos );
						symbols[symbol] = name;
					} break;
					case CONSTRAINTS : {
						kQuery += " " + line;
					} break;
					default : {
						assert( false && "Invalid path file." );
					} break;
				}
			}
		}

		std::cerr << "Loaded " << numPaths << " paths." << std::endl;

		return paths;
	}


	void SPA::onStateDestroy(klee::ExecutionState *kState, bool silenced) {
		assert(kState);

		std::string handlerName;
		if ( ! pathFilter || pathFilter->checkPath( kState ) ) {
			CLOUD9_DEBUG( "Outputting path." );
			output << SPA_PATH_START << std::endl;
			output << handlerName << std::endl;
			output << SPA_PATH_SYMBOLS_START << std::endl;
			for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::iterator it = kState->symbolics.begin(), ie = kState->symbolics.end(); it != ie; it++ )
				output << (*it).first->name << SPA_PATH_SYMBOLS_DELIMITER << (*it).second->name << std::endl;
			output << SPA_PATH_SYMBOLS_END << std::endl;
			output << SPA_PATH_CONSTRAINTS_START << std::endl;
			for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::iterator it = kState->symbolics.begin(), ie = kState->symbolics.end(); it != ie; it++ )
				output << "array " << (*it).second->name << "[" << (*it).second->size << "] : w32 -> w8 = symbolic" << std::endl;
			output << SPA_PATH_CONSTRAINTS_KLEAVER_START << std::endl;
			for ( klee::ConstraintManager::constraint_iterator it = kState->constraints().begin(), ie = kState->constraints().end(); it != ie; it++)
				output << *it << std::endl;
			output << SPA_PATH_CONSTRAINTS_KLEAVER_END << std::endl;
			output << SPA_PATH_CONSTRAINTS_END << std::endl;
			output << SPA_PATH_END << std::endl;
		}
	}
}
