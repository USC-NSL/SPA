/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/RecoverStateUtility.h>

#include <cloud9/ExecutionPath.h>
#include <cloud9/worker/TreeNodeInfo.h>
#include <cloud9/worker/TreeObjects.h>

namespace SPA {
	RecoverStateUtility::RecoverStateUtility( cloud9::worker::JobManager *_jobManager, std::istream &stateFile )
		: jobManager( _jobManager ), recoveredPaths( 0 ) {
		cloud9::ExecutionPathSetPin epsp = cloud9::ExecutionPathSet::parse( stateFile );

		for ( unsigned i = 0; i < epsp->count(); i++ )
			paths.insert( epsp->getPath( i )->getPath() );
	}

// 	void printPath( std::string prefix, std::vector<int> path ) {
// 		std::cerr << prefix;
// 		for ( std::vector<int>::iterator it = path.begin(), ie = path.end(); it != ie; it++ )
// 			std::cerr << *it;
// 		std::cerr << std::endl;
// 	}

	double RecoverStateUtility::getUtility( klee::ExecutionState *state ) {
		if ( (! state->recovered) && (! paths.empty()) ) {
			std::vector<cloud9::worker::WorkerTree::Node *> nodes;
			nodes.push_back( &*state->getCloud9State()->getNode() );
			std::vector<int> path = jobManager->getTree()->buildPathSet( nodes.begin(), nodes.end(), (std::map<cloud9::worker::WorkerTree::Node *,unsigned> *) NULL )->getPath( 0 )->getAbsolutePath()->getPath();

			for ( std::set< std::vector<int> >::iterator it = paths.begin(), ie = paths.end(); it != ie; it++ ) {
				unsigned size = std::min( path.size(), it->size() );

				unsigned i;
				for ( i = 0; i < size && path[i] == (*it)[i]; i++ );
				if ( i == size ) {
					if ( path.size() < it->size() ) {
						CLOUD9_DEBUG( "Path still recovering (" << path.size() << "/" << it->size() << ")." );
// 						printPath( "Current: ", path );
// 						printPath( "Saved: ", *it );
						return path.size();
					} else {
// 						printPath( "Current: ", path );
// 						printPath( "Saved: ", *it );
						state->recovered = true;
						CLOUD9_DEBUG( "Path done recovering." );
						CLOUD9_DEBUG( "Recovered " << ++recoveredPaths << "/" << paths.size() << " paths." );
						return UTILITY_PROCESS_LAST;
					}
				} else {
					if ( path[i] < (*it)[i] ) {
// 						CLOUD9_DEBUG( "Path filtered." );
// 						printPath( "Current: ", path );
						return UTILITY_FILTER_OUT;
					}
				}
			}

// 			CLOUD9_DEBUG( "Path filtered." );
// 			printPath( "Current: ", path );
			return UTILITY_FILTER_OUT;
		} else {
			CLOUD9_DEBUG( "Path recovered." );
			return UTILITY_PROCESS_LAST;
		}
	}
}
