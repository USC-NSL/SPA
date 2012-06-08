/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/SpaSearcher.h>

#include <klee/ExecutionState.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>

namespace SPA {
	SpaSearcher::SpaSearcher( InstructionFilter *_filter )
		: filter( _filter ), statesDequeued( 0 ), statesFiltered( 0 ) { }

	SpaSearcher::~SpaSearcher() { }

	klee::ExecutionState &SpaSearcher::selectState() { return *states.front(); }

	void SpaSearcher::update( klee::ExecutionState *current, const std::set<klee::ExecutionState *> &addedStates, const std::set<klee::ExecutionState *> &removedStates) {
		for ( std::set<klee::ExecutionState*>::iterator it = addedStates.begin(), ie = addedStates.end(); it != ie; it++ ) {
			if ( filter->checkInstruction( (*((*it)->pc())).inst ) ) {
				states.push_back( *it );
			} else {
				std::cerr << "[SpaSearcher] Filtering instruction at " << (*((*it)->pc())).inst->getParent()->getParent()->getName().str() << ":" << (*((*it)->pc())).inst->getDebugLoc().getLine() << std::endl;
				statesFiltered++;
			}
		}
		for ( std::set<klee::ExecutionState *>::iterator it = removedStates.begin(), ie = removedStates.end(); it != ie; it++ )
			states.remove( *it );
		statesDequeued += removedStates.size();

		if ( addedStates.size() || removedStates.size() )
			std::cerr << "[SpaSearcher] Queued: " << states.size() << "; Processed: " << statesDequeued << "; Filtered: " << statesFiltered << std::endl;
	}
}
