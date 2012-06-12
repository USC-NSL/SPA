/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/SpaSearcher.h>

#include <klee/ExecutionState.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>

namespace SPA {
	klee::ExecutionState &SpaSearcher::selectState() { return *states.front(); }

	void SpaSearcher::update( klee::ExecutionState *current, const std::set<klee::ExecutionState *> &addedStates, const std::set<klee::ExecutionState *> &removedStates) {
		for ( std::set<klee::ExecutionState*>::iterator sit = addedStates.begin(), sie = addedStates.end(); sit != sie; sit++ ) {
			if ( filter->checkInstruction( (*((*sit)->pc())).inst ) ) {
				states.push_back( *sit );
			} else {
				std::cerr << "[SpaSearcher] Filtering instruction at " << (*((*sit)->pc())).inst->getParent()->getParent()->getName().str() << ":" << (*((*sit)->pc())).inst->getDebugLoc().getLine() << std::endl;
				statesFiltered++;
				for ( std::list<FilteringEventHandler *>::iterator hit = filteringEventHandlers.begin(), hie = filteringEventHandlers.end(); sit != sie; sit++ )
					(*hit)->onStateFiltered( *sit );
			}
		}
		for ( std::set<klee::ExecutionState *>::iterator it = removedStates.begin(), ie = removedStates.end(); it != ie; it++ )
			states.remove( *it );
		statesDequeued += removedStates.size();

		if ( addedStates.size() || removedStates.size() )
			std::cerr << "[SpaSearcher] Queued: " << states.size() << "; Processed: " << statesDequeued << "; Filtered: " << statesFiltered << std::endl;
	}
}
