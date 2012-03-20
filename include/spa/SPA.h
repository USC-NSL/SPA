/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <iostream>
#include <list>
#include <map>

#include <llvm/Module.h>

#include <cloud9/worker/SymbolicEngine.h>

#include <spa/InstructionFilter.h>
#include <spa/PathFilter.h>

#ifndef __SPA_H__
#define __SPA_H__

#define SPA_PATH_START						"--- PATH START ---"
#define SPA_PATH_SYMBOLS_START				"--- SYMBOLS START ---"
#define SPA_PATH_SYMBOLS_DELIMITER			"	"
#define SPA_PATH_SYMBOLS_END				"--- SYMBOLS END ---"
#define SPA_PATH_CONSTRAINTS_START			"--- CONSTRAINTS START ---"
#define SPA_PATH_CONSTRAINTS_KLEAVER_START	"(query ["
#define SPA_PATH_CONSTRAINTS_KLEAVER_END	"] false)"
#define SPA_PATH_CONSTRAINTS_END			"--- CONSTRAINTS END ---"
#define SPA_PATH_END						"--- PATH END ---"
#define SPA_PATH_COMMENT					"#"
#define SPA_PATH_WHITE_SPACE				" 	\r\n"

namespace SPA {
	std::map<std::string, std::set< std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> > > loadPaths( std::istream &pathFile );

	class SPA : public cloud9::worker::StateEventHandler {
	private:
		llvm::Module *module;
		std::list<llvm::Function *> entryFunctions;
		InstructionFilter *instructionFilter;
		PathFilter *pathFilter;
		std::ostream &output;

		void generateMain();

	public:
		SPA( llvm::Module *_module, std::ostream &_output );
		void addEntryFunction( llvm::Function *fn ) { entryFunctions.push_back( fn ); }
		void setInstructionFilter( InstructionFilter *_instructionFilter ) { instructionFilter = _instructionFilter; }
		void setPathFilter( PathFilter *_pathFilter ) { pathFilter = _pathFilter; }
		void start();

		bool onStateBranching(klee::ExecutionState *state, klee::ForkTag forkTag) { return true; }
		void onStateBranched(klee::ExecutionState *kState, klee::ExecutionState *parent, int index, klee::ForkTag forkTag) {}
		void onOutOfResources(klee::ExecutionState *destroyedState) {}
		void onEvent(klee::ExecutionState *kState, unsigned int type, long int value) {}
		void onControlFlowEvent(klee::ExecutionState *kState, cloud9::worker::ControlFlowEvent event) {}
		void onDebugInfo(klee::ExecutionState *kState, const std::string &message) {}

		void onStateDestroy(klee::ExecutionState *kState, bool silenced);
		};
}

#endif // #ifndef __SPA_H__
