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
		unsigned int u = 0;
		for ( std::vector<StateUtility *>::iterator it = stateUtilities.begin(), ie = stateUtilities.end(); it != ie; it++, u++ ) {
			if ( (*it)->getUtility( state ) == UTILITY_FILTER_OUT ) {
				CLOUD9_DEBUG( "[SpaSearcher] Filtering state due to utility " << u << " at:" );
				klee::c9::printStateStack( std::cerr, *state ) << std::endl;
				return false;
			}
		}
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

	void SpaSearcher::filterState( klee::ExecutionState *state ) {
		dequeueState( state );
		statesFiltered++;
		for ( std::vector<FilteringEventHandler *>::iterator hit = filteringEventHandlers.begin(), hie = filteringEventHandlers.end(); hit != hie; hit++ )
			(*hit)->onStateFiltered( state );
	}

	void SpaSearcher::reorderState( klee::ExecutionState *state ) {
		enqueueState( dequeueState( states.begin()->second ) );
		// Remove trailing filtered states, unless the queue would become empty.
		for ( std::set<std::pair<std::vector<double>,klee::ExecutionState *> >::reverse_iterator it = states.rbegin(), ie = states.rend(); it != ie; it++ ) {
			if ( (! checkState( it->second )) && states.size() > 1 )
				filterState( it->second );
			else
				break;
		}
	}

	klee::ExecutionState &SpaSearcher::selectState() {
		// Reorder head state to keep set coherent.
		reorderState( states.begin()->second );
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
				filterState( *sit );
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

		if ( current )
			reorderState( current );
	}
}
