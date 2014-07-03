/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __SPA_H__
#define __SPA_H__

#include <iostream>
#include <fstream>
#include <deque>
#include <list>
#include <map>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <klee/Interpreter.h>

#include <spa/StateUtility.h>
#include <spa/FilteringEventHandler.h>
#include <spa/PathFilter.h>
#include <spa/PathLoader.h>

#define SPA_API_ANNOTATION_FUNCTION				"spa_api_entry"
#define SPA_MESSAGE_HANDLER_ANNOTATION_FUNCTION	"spa_message_handler_entry"
#define SPA_CHECKPOINT_ANNOTATION_FUNCTION		"spa_checkpoint"
#define SPA_WAYPOINT_ANNOTATION_FUNCTION		"spa_waypoint"
#define SPA_RETURN_ANNOTATION_FUNCTION			"spa_return"
#define SPA_INPUT_ANNOTATION_FUNCTION			"spa_input"
// #define SPA_SEED_ANNOTATION_FUNCTION			"spa_seed"

#define SPA_PREFIX					"spa_"
#define SPA_TAG_PREFIX				SPA_PREFIX "tag_"
#define SPA_STATE_PREFIX			SPA_PREFIX "state_"
#define SPA_INPUT_PREFIX			SPA_PREFIX "in_"
#define SPA_INIT_PREFIX				SPA_PREFIX "init_"
#define SPA_API_INPUT_PREFIX		SPA_INPUT_PREFIX "api_"
#define SPA_MESSAGE_INPUT_PREFIX	SPA_INPUT_PREFIX "msg_"
#define SPA_OUTPUT_PREFIX			SPA_PREFIX "out_"
#define SPA_API_OUTPUT_PREFIX		SPA_OUTPUT_PREFIX "api_"
#define SPA_MESSAGE_OUTPUT_PREFIX	SPA_OUTPUT_PREFIX "msg_"
#define SPA_WAYPOINTS_VARIABLE		SPA_PREFIX "waypoints"

#define SPA_HANDLERTYPE_TAG			"HandlerType"
#define SPA_MESSAGEHANDLER_VALUE	"Message"
#define SPA_APIHANDLER_VALUE		"API"
#define SPA_OUTPUT_TAG				"Output"
#define SPA_OUTPUT_VALUE			"1"
#define SPA_MSGRECEIVED_TAG			"MsgReceived"
#define SPA_MSGRECEIVED_VALUE		"1"
#define SPA_VALIDPATH_TAG			"ValidPath"
#define SPA_VALIDPATH_VALUE			"1"

#define SPA_INITVALUEID_VARIABLE	"spa_internal_initValueID"
#define SPA_HANDLERID_VARIABLE		"spa_internal_HanderID"
// #define SPA_SEEDID_VARIABLE			"spa_internal_SeedID"

#define LOG_FILE_VARIABLE			"SPA_LOG_FILE"


namespace SPA {
	class SPA : public klee::InterpreterHandler, public klee::InterpreterEventListener, public FilteringEventHandler {
	private:
		klee::Interpreter *interpreter;
		llvm::Module *module;
		llvm::Function *entryFunction;
		llvm::Instruction *initHandlerPlaceHolder;
		llvm::GlobalVariable *handlerIDVarName;
		llvm::SwitchInst *initValueSwitchInst;
		llvm::SwitchInst *entryHandlerSwitchInst;
		uint32_t initValueID;
		uint32_t entryHandlerID;
		llvm::BasicBlock *firstHandlerBB;
		llvm::BasicBlock *nextHandlerBB;
		llvm::BasicBlock *returnBB;
		std::ostream &output;
		std::set<llvm::Instruction *> checkpoints;
		std::deque<StateUtility *> stateUtilities;
		std::deque<bool> outputFilteredPaths;
		PathFilter *pathFilter;
		bool outputTerminalPaths;

		unsigned long checkpointsFound, filteredPathsFound, terminalPathsFound, outputtedPaths;

		static llvm::Module *getModuleFromFile(std::string moduleFile);
		void generateMain();
		void showStats();
		void processPath( klee::ExecutionState *state );

	public:
		SPA( std::string moduleFile, std::ostream &_output ) : SPA(getModuleFromFile(moduleFile), _output) {}
		SPA( llvm::Module *_module, std::ostream &_output );
		void addInitFunction( llvm::Function *fn );
		void addEntryFunction( llvm::Function *fn );
// 		void addSeedEntryFunction( unsigned int seedID, llvm::Function *fn );
		void newEntryLevel();
		void addInitialValues( std::map<llvm::Value *, std::vector<std::vector<std::pair<bool,uint8_t> > > > values );
		void addSymbolicInitialValues() { addInitialValues( std::map<llvm::Value *, std::vector<std::vector<std::pair<bool,uint8_t> > > >() ); }
		void setSenderPathLoader( PathLoader *pathLoader, bool follow );
		void addStateUtilityFront( StateUtility *stateUtility, bool _outputFilteredPaths ) {
			stateUtilities.push_front( stateUtility );
			outputFilteredPaths.push_front( _outputFilteredPaths );
		}
		void addStateUtilityBack( StateUtility *stateUtility, bool _outputFilteredPaths ) {
			stateUtilities.push_back( stateUtility );
			outputFilteredPaths.push_back( _outputFilteredPaths );
		}
		void setPathFilter( PathFilter *_pathFilter ) { pathFilter = _pathFilter; }
		void addCheckpoint( llvm::Instruction *instruction ) { checkpoints.insert( instruction ); }
		void setOutputTerminalPaths( bool _outputTerminalPaths ) { outputTerminalPaths = _outputTerminalPaths; }
		llvm::Function *getMainFunction() { return entryFunction; }
		void start();

		llvm::Module *getModule() { return module; }

		llvm::raw_ostream &getInfoStream() const { return llvm::outs(); }
		std::string getOutputFilename(const std::string &filename) { return "/tmp/" + filename; }
		llvm::raw_fd_ostream *openOutputFile(const std::string &filename) {
			std::string err;
			return new llvm::raw_fd_ostream("/dev/null", err);
		}
		void incPathsExplored() {}
		void processTestCase(const klee::ExecutionState &state, const char *err, const char *suffix) {};

		void onStateBranching(klee::ExecutionState *state) {}
		void onStateBranched(klee::ExecutionState *kState, klee::ExecutionState *parent, int index) {}
		void onStep(klee::ExecutionState *kState);
		void onCall(klee::ExecutionState *kState) {};
		void onReturn(klee::ExecutionState *kState) {};
		void onStateDestroy(klee::ExecutionState *kState);

		void onStateFiltered( klee::ExecutionState *state, unsigned int id );
	};
}

#endif // #ifndef __SPA_H__
