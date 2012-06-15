/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/SpaSearcher.h>

#include <klee/ExecutionState.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include "cloud9/Logger.h"


namespace SPA {
	klee::ExecutionState *SpaSearcher::enqueueState( klee::ExecutionState *state ) {
		if ( stateUtility ) {
			// Invert search order to get high priority states at beginning of queue.
			double u = - stateUtility->getUtility( state );
			states.insert( std::pair<double, klee::ExecutionState *>( u, state ) );
			stateUtilities[state] = u;
		} else {
			states.insert( std::pair<double, klee::ExecutionState *>( 0, state ) );
		}

		return state;
	}

	klee::ExecutionState *SpaSearcher::dequeueState( klee::ExecutionState *state ) {
		if ( stateUtility ) {
			states.erase( std::pair<double, klee::ExecutionState *>( stateUtilities[state], state ) );
			stateUtilities.erase( state );
		} else {
			states.erase( std::pair<double, klee::ExecutionState *>( 0, state ) );
		}

		return state;
	}

	klee::ExecutionState &SpaSearcher::selectState() {
		enqueueState( dequeueState( states.begin()->second ) );
		CLOUD9_DEBUG( "[SpaSearcher] Selecting state at "
			<< (*(states.begin()->second->pc())).inst->getParent()->getParent()->getName().str()
			<< ":" << (*(states.begin()->second->pc())).inst->getDebugLoc().getLine()
			<< " with utility: " << - states.begin()->first );
		return *states.begin()->second;
	}

	void SpaSearcher::update( klee::ExecutionState *current, const std::set<klee::ExecutionState *> &addedStates, const std::set<klee::ExecutionState *> &removedStates) {
		for ( std::set<klee::ExecutionState*>::iterator sit = addedStates.begin(), sie = addedStates.end(); sit != sie; sit++ ) {
			if ( ! filter || filter->checkInstruction( (*((*sit)->pc())).inst ) ) {
				enqueueState( *sit );
			} else {
				CLOUD9_DEBUG( "[SpaSearcher] Filtering instruction at " << (*((*sit)->pc())).inst->getParent()->getParent()->getName().str() << ":" << (*((*sit)->pc())).inst->getDebugLoc().getLine() );
				statesFiltered++;
				for ( std::list<FilteringEventHandler *>::iterator hit = filteringEventHandlers.begin(), hie = filteringEventHandlers.end(); hit != hie; hit++ )
					(*hit)->onStateFiltered( *sit );
			}
		}
		for ( std::set<klee::ExecutionState *>::iterator it = removedStates.begin(), ie = removedStates.end(); it != ie; it++ )
			dequeueState( *it );
		statesDequeued += removedStates.size();

		// Re-insert to keep set coherent.
		if ( current )
			enqueueState( dequeueState( current ) );

		if ( removedStates.size() )
			CLOUD9_DEBUG( "[SpaSearcher] Queued: " << states.size()
				<< "; Utility Range: [" << (states.size() ? - states.rbegin()->first : 0)
				<< "; " << (states.size() ? - states.begin()->first : 0)
				<< "]; Processed: " << statesDequeued
				<< "; Filtered: " << statesFiltered );
	}
}
