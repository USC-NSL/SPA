/*
 * Cloud9 Parallel Symbolic Execution Engine
 *
 * Copyright (c) 2011, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * All contributors are listed in CLOUD9-AUTHORS file.
 *
*/

#include <iostream>
#include <fstream>
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

#define MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION "max_messagehandler"
#define MAX_INTERESTING_ANNOTATION_FUNCTION "max_interesting"

using namespace llvm;
using namespace cloud9::worker;

namespace {
cl::opt<bool>
        StandAlone("stand-alone",
                cl::desc("Enable running a worker in stand alone mode"),
                cl::init(false));

cl::opt<std::string> ReplayPath("c9-replay-path", cl::desc(
		"Instead of executing jobs, just do a replay of a path. No load balancer involved."));

}

static bool Interrupted = false;
extern cl::opt<double> MaxTime;

JobManager *theJobManager = NULL;

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
	cl::ParseCommandLineOptions(argc, argv, "Cloud9 worker");
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

void done() {
	exit(0);
}

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
	std::map<llvm::Instruction *,std::set<llvm::Instruction *> > cfgPredecessors, cfgSuccessors;
	std::map<llvm::Function *,std::set<llvm::Instruction *> > callers;
	// Iterate functions.
	for ( llvm::Module::iterator mit = mainModule->begin(), mie = mainModule->end(); mit != mie; mit++ ) {
		Function &fn = *mit;
		// Iterate basic blocks.
		for ( llvm::Function::iterator fit = fn.begin(), fie = fn.end(); fit != fie; fit++ ) {
			BasicBlock &bb = *fit;
			// Connect across basic blocks.
			// Iterate successors.
			TerminatorInst *ti = bb.getTerminator();
			unsigned int ns = ti->getNumSuccessors();
			for ( unsigned int i = 0; i < ns; i++ ) {
				// Add entry (even if with no successors) for the current predecessor.
				cfgPredecessors[&(bb.front())];
				cfgSuccessors[ti];
				cfgPredecessors[&(ti->getSuccessor( i )->front())].insert( ti );
				cfgSuccessors[ti].insert( &(ti->getSuccessor( i )->front()) );
			}
			// Connect within basic block.
			// Iterate instructions.
			llvm::BasicBlock::iterator bbit = bb.begin(), bbie = bb.end();
			llvm::Instruction *prevInst = &(*(bbit++));
			for ( ; bbit != bbie; bbit++ ) {
				Instruction *inst = &(*bbit);
				cfgPredecessors[inst];
				cfgSuccessors[inst];
				cfgPredecessors[inst].insert( prevInst );
				cfgSuccessors[prevInst].insert( inst );
				// Check for CallInst or InvokeInst.
				if ( llvm::InvokeInst *ii = dyn_cast<llvm::InvokeInst>( inst ) )
					callers[ii->getCalledFunction()].insert( inst );
				if ( llvm::CallInst *ci = dyn_cast<llvm::CallInst>( inst ) )
					callers[ci->getCalledFunction()].insert( inst );
				prevInst = inst;
			}
		}
	}

	// Find message handling functions.
	CLOUD9_DEBUG( "   Searching for message handlers." );
	std::set<llvm::Function *> messageHandlers;
	for ( std::set<llvm::Instruction *>::iterator it = callers[mainModule->getFunction( MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION )].begin(), ie = callers[mainModule->getFunction( MAX_MESSAGE_HANDLER_ANNOTATION_FUNCTION )].end(); it != ie; it++ )
		messageHandlers.insert( (*it)->getParent()->getParent() );
	if ( messageHandlers.empty() )
		CLOUD9_ERROR( "No message handlers found." );

	// Find interesting instructions.
	CLOUD9_DEBUG( "   Searching for interesting instructions." );
	std::set<llvm::Instruction *> interestingInstructions = callers[mainModule->getFunction( MAX_INTERESTING_ANNOTATION_FUNCTION )];
	if ( interestingInstructions.empty() )
		CLOUD9_ERROR( "No interesting statements found." );

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
		std::set<llvm::Instruction *> s = cfgSuccessors[inst];
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
			for ( std::map<llvm::Instruction *,std::set<llvm::Instruction *> >::iterator it2 = cfgSuccessors.begin(), ie2 = cfgSuccessors.end(); it2 != ie2; it2++ )
				if ( (*it2).first->getParent()->getParent() == fn && reachedFromHandler.count( (*it2).first ) == 0 )
					worklist.insert( (*it2).first );
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
		std::set<llvm::Instruction *> p = cfgPredecessors[inst];
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
			for ( std::map<llvm::Instruction *,std::set<llvm::Instruction *> >::iterator it2 = cfgSuccessors.begin(), ie2 = cfgSuccessors.end(); it2 != ie2; it2++ )
				if ( (*it2).first->getParent()->getParent() == fn && reachesInteresting.count( (*it2).first ) == 0 )
					worklist.insert( (*it2).first );
		// Check if entry instruction.
		if ( inst == &(inst->getParent()->getParent()->getEntryBlock().front()) ) {
			p = callers[inst->getParent()->getParent()];
			// Add all useless callers to work list.
			for ( it = p.begin(), ie = p.end(); it != ie; it++ )
				if ( reachesInteresting.count( *it ) == 0 )
					worklist.insert( *it );
		}
	}
	// Determine useless instructions (can't reach interesting statements after starting at message handler).
	CLOUD9_DEBUG( "   Defining useless instructions." );
	std::set<llvm::Instruction *> uselessInstructions;
	for ( std::map<llvm::Instruction *,std::set<llvm::Instruction *> >::iterator it = cfgSuccessors.begin(), ie = cfgSuccessors.end(); it != ie; it++ )
		if ( reachedFromHandler.count( (*it).first ) == 0 || reachesInteresting.count( (*it).first ) == 0 )
			uselessInstructions.insert( (*it).first );

	// Generate CFG DOT file.
	CLOUD9_DEBUG( "   Generating CFG output." );
	{
		FILE *dotFile = fopen( "cfg.dot", "w" );
		fprintf( dotFile, "digraph CFG {\n" );

		// Color message handling function clusters.
		for ( std::set<llvm::Function *>::iterator it = messageHandlers.begin(), ie = messageHandlers.end(); it != ie; it++ )
			fprintf( dotFile, "	subgraph cluster_%s {\n		color = \"red\";\n	}\n", (*it)->getName().str().c_str() );
		// Add all instructions.
		for ( std::map<llvm::Instruction *,std::set<llvm::Instruction *> >::iterator it = cfgSuccessors.begin(), ie = cfgSuccessors.end(); it != ie; it++ ) {
			Instruction *inst = (*it).first;
			const char *shape = "oval";
			const char *style = "";
			if ( inst == &(inst->getParent()->getParent()->getEntryBlock().front()) )
				shape = "box";
			if ( (*it).second.empty() )
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
		for ( std::map<llvm::Instruction *,std::set<llvm::Instruction *> >::iterator it1 = cfgSuccessors.begin(), ie1 = cfgSuccessors.end(); it1 != ie1; it1++ )
			for ( std::set<llvm::Instruction *>::iterator it2 = (*it1).second.begin(), ie2 = (*it1).second.end(); it2 != ie2; it2++ )
				fprintf( dotFile, "	n%p -> n%p;\n", (void*) (*it1).first, (void*) *it2 );
		// Callers.
		fprintf( dotFile, "	edge [color = \"blue\"];\n" );
		for ( std::map<llvm::Function *,std::set<llvm::Instruction *> >::iterator it1 = callers.begin(), ie1 = callers.end(); it1 != ie1; it1++ ) {
			llvm::Function *fn = (*it1).first;
			for ( std::set<llvm::Instruction *>::iterator it2 = (*it1).second.begin(), ie2 = (*it1).second.end(); it2 != ie2; it2++ ) {

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
	done();

	// Create the job manager
	theJobManager = new JobManager(mainModule, "main", pArgc, pArgv, envp);

	if (ReplayPath.size() > 0) {
      CLOUD9_INFO("Running in replay mode. No load balancer involved.");

      std::ifstream is(ReplayPath.c_str());

      if (is.fail()) {
          CLOUD9_EXIT("Could not open the replay file " << ReplayPath);
      }

      cloud9::ExecutionPathSetPin pathSet = cloud9::ExecutionPathSet::parse(is);

      theJobManager->replayJobs(pathSet);
	} else if (StandAlone) {
	  CLOUD9_INFO("Running in stand-alone mode. No load balancer involved.");

	  theJobManager->processJobs(true, (int)MaxTime.getValue());

	  cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::TimeOut, "Timeout");

	  theJobManager->finalize();
	} else {
      CommManager commManager(theJobManager); // Handle outside communication
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
