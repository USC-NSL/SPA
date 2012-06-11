/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __SPA_H__
#define __SPA_H__

#include <iostream>
#include <list>
#include <map>

#include <llvm/Module.h>
#include "llvm/Instructions.h"

#include <cloud9/worker/SymbolicEngine.h>

#include <spa/InstructionFilter.h>
#include <spa/PathFilter.h>

#define SPA_API_ANNOTATION_FUNCTION				"spa_api_entry"
#define SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION	"spa_message_handler_entry"
#define SPA_CHECKPOINT_ANNOTATION_FUNCTION		"spa_checkpoint"

#define SPA_PREFIX					"spa_"
#define SPA_TAG_PREFIX				SPA_PREFIX "tag_"
#define SPA_STATE_PREFIX			SPA_PREFIX "state_"
#define SPA_INPUT_PREFIX			SPA_PREFIX "in_"
#define SPA_API_INPUT_PREFIX		SPA_INPUT_PREFIX "api_"
#define SPA_MESSAGE_INPUT_PREFIX	SPA_INPUT_PREFIX "msg_"
#define SPA_OUTPUT_PREFIX			SPA_PREFIX "out_"
#define SPA_API_OUTPUT_PREFIX		SPA_OUTPUT_PREFIX "api_"
#define SPA_MESSAGE_OUTPUT_PREFIX	SPA_OUTPUT_PREFIX "msg_"

#define SPA_HANDLERTYPE_TAG			"HandlerType"
#define SPA_MESSAGEHANDLER_VALUE	"Message"
#define SPA_APIHANDLER_VALUE		"API"
#define SPA_OUTPUT_TAG				"Output"
#define SPA_VALIDPATH_TAG			"ValidPath"
#define SPA_VALIDPATH_VALUE			"1"

namespace SPA {
	class SPA : public cloud9::worker::StateEventHandler {
	private:
		llvm::Module *module;
		llvm::Function *entryFunction;
		llvm::Instruction *initHandlerPlaceHolder;
		llvm::SwitchInst *entrySwitchInst;
		uint32_t handlerID;
		llvm::BasicBlock *entryReturnBB;
		std::ostream &output;
		std::set<llvm::Instruction *> checkpoints;
		InstructionFilter *instructionFilter;
		PathFilter *pathFilter;
		bool outputTerminalPaths;

		unsigned long checkpointsFound, terminalPathsFound, outputtedPaths;

		void generateMain();

	public:
		SPA( llvm::Module *_module, std::ostream &_output );
		void addInitFunction( llvm::Function *fn );
		void addEntryFunction( llvm::Function *fn );
		void setInstructionFilter( InstructionFilter *_instructionFilter ) { instructionFilter = _instructionFilter; }
		void setPathFilter( PathFilter *_pathFilter ) { pathFilter = _pathFilter; }
		void addCheckpoint( llvm::Instruction *instruction ) { checkpoints.insert( instruction ); }
		void setOutputTerminalPaths( bool _outputTerminalPaths ) { outputTerminalPaths = _outputTerminalPaths; }
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
