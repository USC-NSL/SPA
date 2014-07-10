/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <fstream>
#include <sstream>
#include <chrono>

#include <spa/SpaSearcher.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include "../Core/Common.h"
#include <../Core/Executor.h>
#include <klee/ExecutionState.h>
#include <klee/Internal/Module/KInstruction.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>

#define SAVE_STATE_PERIOD 30 // seconds
#define PRINT_STATS_PERIOD 1000 // ms
// #define QUEUE_LIMIT	1000000
// #define DROP_TO		 900000


namespace SPA {
// 	extern llvm::cl::opt<std::string> RecoverState;
// 	llvm::cl::opt<std::string> SaveState( "save-state",
// 		llvm::cl::desc( "Specifies a file to periodically save the processing queue to." ) );

	bool SpaSearcher::checkState( klee::ExecutionState *state, unsigned int &id ) {
		for ( std::deque<StateUtility *>::iterator it = stateUtilities.begin(), ie = stateUtilities.end(); it != ie; it++, id++ ) {
			if ( (*it)->getUtility( state ) == UTILITY_FILTER_OUT ) {
				klee::klee_message( "[SpaSearcher] Filtering state due to utility %d at %s:%d",
            id, state->pc->inst->getParent()->getParent()->getName().str().c_str(),
            state->pc->inst->getDebugLoc().getLine() );
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
// 		if ( RecoverState.size() == 0 || state->recovered )
			for ( std::vector<FilteringEventHandler *>::iterator hit = filteringEventHandlers.begin(), hie = filteringEventHandlers.end(); hit != hie; hit++ )
				(*hit)->onStateFiltered( state, id );
	}

	void SpaSearcher::reorderState( klee::ExecutionState *state ) {
		enqueueState( dequeueState( states.begin()->second ) );
	}

	void SpaSearcher::reorderAllStates() {
		std::set<std::pair<std::vector<double>,klee::ExecutionState *> > tempStates = states;
		states.clear();

		for ( std::set<std::pair<std::vector<double>,klee::ExecutionState *> >::iterator it = tempStates.begin(), ie = tempStates.end(); it != ie; it++ )
			enqueueState( it->second );
	}

// 	void SpaSearcher::saveStates() {
// 		std::vector<cloud9::worker::WorkerTree::Node *> nodes;
// 
// 		for ( std::set<std::pair<std::vector<double>,klee::ExecutionState *> >::iterator it = states.begin(), ie = states.end(); it != ie; it++ ) {
// 			// Check if any state is in recovery and abort.
// // 			if ( RecoverState.size() > 0 && ! it->second->recovered )
// // 				return;
// 			nodes.push_back( &*it->second->getCloud9State()->getNode() );
// 		}
// 
// 		cloud9::ExecutionPathSetPin paths = jobManager->getTree()->buildPathSet( nodes.begin(), nodes.end(), (std::map<cloud9::worker::WorkerTree::Node *,unsigned> *) NULL );
// 
// 		klee_message( "Saving processing queue to: " << SaveState.getValue() );
// 		std::ofstream stateFile( SaveState.getValue().c_str() );
// 		assert( stateFile.is_open() && "Unable to open state file." );
// 		for ( unsigned i = 0; i < paths->count(); i++ ) {
// 			std::vector<int> path = paths->getPath( i )->getAbsolutePath()->getPath();
// 			for ( std::vector<int>::const_iterator it = path.begin(), ie = path.end(); it != ie; it++ ) {
// 				stateFile << *it;
// 			}
// 			stateFile << std::endl;
// 		}
// 		stateFile.flush();
// 		stateFile.close();
// 	}

	klee::ExecutionState &SpaSearcher::selectState() {
		// Reorder head state to keep set coherent.
		unsigned int id = 0;
		if ( ! checkState( states.begin()->second, id ) )
			filterState( states.begin()->second, id );
		else
			reorderState( states.begin()->second );

#ifdef QUEUE_LIMIT
		if ( (! states.begin()->second->filtered) && states.size() > QUEUE_LIMIT ) {
			int n = states.size() - DROP_TO;
			klee::klee_message( "[SpaSearcher] Dropping %d states.", n );

			for ( std::set<std::pair<std::vector<double>,klee::ExecutionState *> >::reverse_iterator it = states.rbegin(); n-- > 0; it++ )
				it->second->filtered = true;
			reorderAllStates();
		}
#endif

// 		if ( SaveState.size() > 0 && difftime( time( NULL ), lastSaved ) > SAVE_STATE_PERIOD ) {
// 			saveStates();
// 			lastSaved = time( NULL );
// 		}

    printStats();

		klee::ExecutionState &state = *states.begin()->second;
// 		klee::klee_message( "[SpaSearcher] Selecting state with cost %s:",
//                         utilityStr( states.begin()->first ).c_str() );
// 		state.dumpStack(llvm::errs());
		return state;
	}

	void SpaSearcher::update( klee::ExecutionState *current, const std::set<klee::ExecutionState *> &addedStates, const std::set<klee::ExecutionState *> &removedStates) {
		for ( std::set<klee::ExecutionState*>::iterator sit = addedStates.begin(), sie = addedStates.end(); sit != sie; sit++ )
			enqueueState( *sit );
		for ( std::set<klee::ExecutionState *>::iterator it = removedStates.begin(), ie = removedStates.end(); it != ie; it++ ) {
			dequeueState( *it );
// 			klee_message( "[SpaSearcher] Dequeuing state at "
// 				<< (*((*it)->pc())).inst->getParent()->getParent()->getName().str()
// 				<< ":" << (*((*it)->pc())).inst->getDebugLoc().getLine() );
// 			klee::c9::printStateStack( std::cerr, **it ) << std::endl;
		}
		statesDequeued += removedStates.size();

    printStats();

		if ( states.empty() )
			klee::klee_message( "[SpaSearcher] State queue is empty!" );
	}

  void SpaSearcher::printStats() {
    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() > PRINT_STATS_PERIOD) {
      last = now;
      klee::klee_message( "[SpaSearcher] Queued: %ld; Utility Range: [%s; %s]; Processed: %ld; Filtered: %ld",
                          states.size(),
                          (states.size() ? utilityStr( states.rbegin()->first ).c_str() : ""),
                          (states.size() ? utilityStr( states.begin()->first ).c_str() : ""),
                          statesDequeued,
                          statesFiltered );
    }
  }
}

