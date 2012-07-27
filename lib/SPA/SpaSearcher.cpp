/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <fstream>
#include <sstream>

#include <spa/SpaSearcher.h>

#include <llvm/Support/CommandLine.h>
#include <klee/ExecutionState.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <cloud9/worker/TreeNodeInfo.h>
#include <cloud9/worker/TreeObjects.h>
#include <cloud9/Logger.h>


namespace SPA {
	llvm::cl::opt<std::string> SaveState( "save-state",
		llvm::cl::desc( "Specifies a file to periodically save the processing queue to." ) );

	bool SpaSearcher::checkState( klee::ExecutionState *state, unsigned int &id ) {
		for ( std::deque<StateUtility *>::iterator it = stateUtilities.begin(), ie = stateUtilities.end(); it != ie; it++, id++ ) {
			if ( (*it)->getUtility( state ) == UTILITY_FILTER_OUT ) {
				CLOUD9_DEBUG( "[SpaSearcher] Filtering state due to utility " << id << " at "
					<< state->pc()->inst->getParent()->getParent()->getName().str()
					<< ":" << state->pc()->inst->getDebugLoc().getLine() );
// 				klee::c9::printStateStack( std::cerr, *state ) << std::endl;
// 				state->pc()->inst->dump();
				return false;
			}
		}
		return true;
	}

	void SpaSearcher::enqueueState( klee::ExecutionState *state ) {
		std::vector<double> u;
		for ( std::deque<StateUtility *>::iterator it = stateUtilities.begin(), ie = stateUtilities.end(); it != ie; it++ )
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

	void SpaSearcher::filterState( klee::ExecutionState *state, unsigned int id ) {
		state->filtered = true;
		statesFiltered++;
		for ( std::vector<FilteringEventHandler *>::iterator hit = filteringEventHandlers.begin(), hie = filteringEventHandlers.end(); hit != hie; hit++ )
			(*hit)->onStateFiltered( state, id );
	}

	void SpaSearcher::reorderState( klee::ExecutionState *state ) {
		enqueueState( dequeueState( states.begin()->second ) );
	}

	void SpaSearcher::saveStates( std::ostream &out ) {
		std::vector<cloud9::worker::WorkerTree::Node *> nodes;

		for ( std::set<std::pair<std::vector<double>,klee::ExecutionState *> >::iterator it = states.begin(), ie = states.end(); it != ie; it++ )
			nodes.push_back( &*it->second->getCloud9State()->getNode() );

		cloud9::ExecutionPathSetPin paths = jobManager->getTree()->buildPathSet( nodes.begin(), nodes.end(), (std::map<cloud9::worker::WorkerTree::Node *,unsigned> *) NULL );

		for ( unsigned i = 0; i < paths->count(); i++ ) {
			std::vector<int> path = paths->getPath( i )->getAbsolutePath()->getPath();
			for ( std::vector<int>::const_iterator it = path.begin(), ie = path.end(); it != ie; it++ ) {
				out << *it;
			}
			out << std::endl;
		}
	}

	klee::ExecutionState &SpaSearcher::selectState() {
		// Reorder head state to keep set coherent.
		unsigned int id = 0;
		if ( ! checkState( states.begin()->second, id ) )
			filterState( states.begin()->second, id );
		else
			reorderState( states.begin()->second );
		CLOUD9_DEBUG( "[SpaSearcher] Queued: " << states.size()
			<< "; Utility Range: [" << (states.size() ? utilityStr( states.rbegin()->first ) : "")
			<< "; " << (states.size() ? utilityStr( states.begin()->first ) : "")
			<< "]; Processed: " << statesDequeued
			<< "; Filtered: " << statesFiltered );

		if ( SaveState.size() > 0 ) {
			CLOUD9_DEBUG( "Saving processing queue to: " << SaveState.getValue() );
			std::ofstream cpFile( SaveState.getValue().c_str() );
			assert( cpFile.is_open() && "Unable to open state file." );
			saveStates( cpFile );
			cpFile.flush();
			cpFile.close();
		}

		CLOUD9_DEBUG( "[SpaSearcher] Selecting state at "
			<< (*(states.begin()->second->pc())).inst->getParent()->getParent()->getName().str()
			<< ":" << (*(states.begin()->second->pc())).inst->getDebugLoc().getLine() );
		klee::c9::printStateStack( std::cerr, *states.begin()->second ) << std::endl;
		return *states.begin()->second;
	}

	void SpaSearcher::update( klee::ExecutionState *current, const std::set<klee::ExecutionState *> &addedStates, const std::set<klee::ExecutionState *> &removedStates) {
		for ( std::set<klee::ExecutionState*>::iterator sit = addedStates.begin(), sie = addedStates.end(); sit != sie; sit++ )
			enqueueState( *sit );
		for ( std::set<klee::ExecutionState *>::iterator it = removedStates.begin(), ie = removedStates.end(); it != ie; it++ ) {
			dequeueState( *it );
			CLOUD9_DEBUG( "[SpaSearcher] Dequeuing state at "
				<< (*((*it)->pc())).inst->getParent()->getParent()->getName().str()
				<< ":" << (*((*it)->pc())).inst->getDebugLoc().getLine() );
// 			klee::c9::printStateStack( std::cerr, **it ) << std::endl;
		}
		statesDequeued += removedStates.size();

// 		if ( current )
// 			reorderState( current );

		if ( states.empty() )
			CLOUD9_DEBUG( "[SpaSearcher] State queue is empty!" );
	}
}
