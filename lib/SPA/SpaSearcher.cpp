/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <sstream>

#include <spa/SpaSearcher.h>

#include <klee/ExecutionState.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include "cloud9/Logger.h"


namespace SPA {
	bool SpaSearcher::checkState( klee::ExecutionState *state ) {
		for ( std::vector<StateUtility *>::iterator it = stateUtilities.begin(), ie = stateUtilities.end(); it != ie; it++ )
			if ( (*it)->getUtility( state ) == UTILITY_FILTER_OUT )
				return false;
		return true;
	}

	void SpaSearcher::enqueueState( klee::ExecutionState *state ) {
		std::vector<double> u;
		for ( std::vector<StateUtility *>::iterator it = stateUtilities.begin(), ie = stateUtilities.end(); it != ie; it++ )
			// Invert search order to get high priority states at beginning of queue.
			u.push_back( - (*it)->getUtility( state ) );
		states.insert( std::pair<std::vector<double>, klee::ExecutionState *>( u, state ) );
		oldUtilities[state] = u;
	}

	klee::ExecutionState *SpaSearcher::dequeueState( klee::ExecutionState *state ) {
		states.erase( std::pair<std::vector<double>, klee::ExecutionState *>( oldUtilities[state], state ) );
		oldUtilities.erase( state );

		return state;
	}

	std::string utilityStr( const std::vector<double> &utility ) {
		std::stringstream result;
		for ( std::vector<double>::const_iterator it = utility.begin(), ie = utility.end(); it != ie; it++ )
			result << (it != utility.begin() ? "," : "") << (-*it);
		return result.str();
	}

	klee::ExecutionState &SpaSearcher::selectState() {
		if ( checkState( states.begin()->second ) ) {
			// If head instruction is still in, re-insert to keep set coherent.
			enqueueState( dequeueState( states.begin()->second ) );
		} else if ( states.size() > 1 ) {
			// If head instruction is out, remove it, unless the queue would become empty.
			CLOUD9_DEBUG( "[SpaSearcher] Filtering ongoing state at instruction " << states.begin()->second->pc()->inst->getParent()->getParent()->getName().str() << ":" << states.begin()->second->pc()->inst->getDebugLoc().getLine() );
			statesFiltered++;
			for ( std::vector<FilteringEventHandler *>::iterator hit = filteringEventHandlers.begin(), hie = filteringEventHandlers.end(); hit != hie; hit++ )
				(*hit)->onStateFiltered( states.begin()->second );
			dequeueState( states.begin()->second );
		}
		CLOUD9_DEBUG( "[SpaSearcher] Queued: " << states.size()
			<< "; Utility Range: [" << (states.size() ? utilityStr( states.rbegin()->first ) : "")
			<< "; " << (states.size() ? utilityStr( states.begin()->first ) : "")
			<< "]; Processed: " << statesDequeued
			<< "; Filtered: " << statesFiltered );
		CLOUD9_DEBUG( "[SpaSearcher] Selecting state at "
			<< (*(states.begin()->second->pc())).inst->getParent()->getParent()->getName().str()
			<< ":" << (*(states.begin()->second->pc())).inst->getDebugLoc().getLine() );
		klee::c9::printStateStack( std::cerr, *states.begin()->second ) << std::endl;
		return *states.begin()->second;
	}

	void SpaSearcher::update( klee::ExecutionState *current, const std::set<klee::ExecutionState *> &addedStates, const std::set<klee::ExecutionState *> &removedStates) {
		for ( std::set<klee::ExecutionState*>::iterator sit = addedStates.begin(), sie = addedStates.end(); sit != sie; sit++ ) {
			if ( checkState( *sit ) ) {
				enqueueState( *sit );
			} else {
				CLOUD9_DEBUG( "[SpaSearcher] Filtering new state at instruction " << (*sit)->pc()->inst->getParent()->getParent()->getName().str() << ":" << (*sit)->pc()->inst->getDebugLoc().getLine() );
				statesFiltered++;
				for ( std::vector<FilteringEventHandler *>::iterator hit = filteringEventHandlers.begin(), hie = filteringEventHandlers.end(); hit != hie; hit++ )
					(*hit)->onStateFiltered( *sit );
			}
		}
		for ( std::set<klee::ExecutionState *>::iterator it = removedStates.begin(), ie = removedStates.end(); it != ie; it++ ) {
			dequeueState( *it );
			CLOUD9_DEBUG( "[SpaSearcher] Dequeuing state at "
				<< (*((*it)->pc())).inst->getParent()->getParent()->getName().str()
				<< ":" << (*((*it)->pc())).inst->getDebugLoc().getLine() );
			klee::c9::printStateStack( std::cerr, **it ) << std::endl;
		}
		statesDequeued += removedStates.size();

		// Re-insert to keep set coherent.
		if ( current )
			enqueueState( dequeueState( current ) );
	}
}
