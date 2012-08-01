/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __SPASEARCHER_H__
#define __SPASEARCHER_H__

#include <deque>
#include <ctime>

#include <klee/Searcher.h>
#include <cloud9/worker/JobManager.h>

#include <spa/StateUtility.h>
#include <spa/FilteringEventHandler.h>


namespace SPA {
	class SpaSearcher : public klee::Searcher {
	private:
		cloud9::worker::JobManager *jobManager;
		std::deque<StateUtility *> stateUtilities;
		std::set<std::pair<std::vector<double>,klee::ExecutionState *> > states;
		std::map<klee::ExecutionState *, std::vector<double> > oldUtilities;
		std::vector<FilteringEventHandler *> filteringEventHandlers;
		unsigned long statesDequeued;
		unsigned long statesFiltered;
		time_t lastSaved;

		bool checkState( klee::ExecutionState *state, unsigned int &id );
		void enqueueState( klee::ExecutionState *state );
		klee::ExecutionState *dequeueState( klee::ExecutionState *state );
		void filterState( klee::ExecutionState *state, unsigned int id );
		void reorderState( klee::ExecutionState *state );
		void saveStates();

	public:
		explicit SpaSearcher( cloud9::worker::JobManager *_jobManager, std::deque<StateUtility *> _stateUtilities )
			: jobManager( _jobManager ), stateUtilities( _stateUtilities ), statesDequeued( 0 ), statesFiltered( 0 ) { time( &lastSaved ); };
		void addFilteringEventHandler( FilteringEventHandler *handler ) { filteringEventHandlers.push_back( handler ); }
		~SpaSearcher() { };

		klee::ExecutionState &selectState();
		void update( klee::ExecutionState *current, const std::set<klee::ExecutionState *> &addedStates, const std::set<klee::ExecutionState *> &removedStates );
		bool empty() { return states.empty(); }
		void printName( std::ostream &os ) {
			os << "SpaSearcher" << std::endl;
		}
	};
}

#endif // #ifndef __SPASEARCHER_H__
