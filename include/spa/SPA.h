/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __SPA_H__
#define __SPA_H__

#include <iostream>
#include <list>
#include <map>

#include <llvm/Module.h>

#include <cloud9/worker/SymbolicEngine.h>

#include <spa/InstructionFilter.h>
#include <spa/PathFilter.h>

namespace SPA {
	class SPA : public cloud9::worker::StateEventHandler {
	private:
		llvm::Module *module;
		std::list<llvm::Function *> initFunctions;
		std::list<llvm::Function *> entryFunctions;
		std::set<llvm::Instruction *> checkpoints;
		InstructionFilter *instructionFilter;
		PathFilter *pathFilter;
		std::ostream &output;

		void generateMain();

	public:
		SPA( llvm::Module *_module, std::ostream &_output );
		void addInitFunction( llvm::Function *fn ) { initFunctions.push_back( fn ); }
		void addEntryFunction( llvm::Function *fn ) { entryFunctions.push_back( fn ); }
		void setInstructionFilter( InstructionFilter *_instructionFilter ) { instructionFilter = _instructionFilter; }
		void setPathFilter( PathFilter *_pathFilter ) { pathFilter = _pathFilter; }
		void addCheckpoint( llvm::Instruction *instruction ) { checkpoints.insert( instruction ); }
		void start();

		bool onStateBranching(klee::ExecutionState *state, klee::ForkTag forkTag) { return true; }
		void onStateBranched(klee::ExecutionState *kState, klee::ExecutionState *parent, int index, klee::ForkTag forkTag) {}
		void onOutOfResources(klee::ExecutionState *destroyedState) {}
		void onEvent(klee::ExecutionState *kState, unsigned int type, long int value) {}
		void onDebugInfo(klee::ExecutionState *kState, const std::string &message) {}

		void onControlFlowEvent(klee::ExecutionState *kState, cloud9::worker::ControlFlowEvent event);
		void onStateDestroy(klee::ExecutionState *kState, bool silenced);
	};
}

#endif // #ifndef __SPA_H__
