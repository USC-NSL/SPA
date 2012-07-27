/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/RecoverStateUtility.h>

#include <cloud9/ExecutionPath.h>
#include <cloud9/worker/TreeNodeInfo.h>
#include <cloud9/worker/TreeObjects.h>

namespace SPA {
	RecoverStateUtility::RecoverStateUtility( cloud9::worker::JobManager *_jobManager, std::istream &stateFile )
		: jobManager( _jobManager ) {
		cloud9::ExecutionPathSetPin epsp = cloud9::ExecutionPathSet::parse( stateFile );

		for ( unsigned i = 0; i < epsp->count(); i++ )
			paths.insert( epsp->getPath( i )->getPath() );
	}

	double RecoverStateUtility::getUtility( const klee::ExecutionState *state ) {
		if ( ! paths.empty() ) {
			std::vector<cloud9::worker::WorkerTree::Node *> nodes;
			nodes.push_back( &*state->getCloud9State()->getNode() );
			std::vector<int> path = jobManager->getTree()->buildPathSet( nodes.begin(), nodes.end(), (std::map<cloud9::worker::WorkerTree::Node *,unsigned> *) NULL )->getPath( 0 )->getAbsolutePath()->getPath();

			for ( std::set< std::vector<int> >::iterator it = paths.begin(), ie = paths.end(); it != ie; it++ ) {
				unsigned size = std::min( path.size(), it->size() );

				unsigned i;
				for ( i = 0; i < size && path[i] == (*it)[i]; i++ );
				if ( i == size ) {
					if ( path.size() < it->size() ) {
						return UTILITY_DEFAULT + 1;
					} else {
						return UTILITY_DEFAULT;
					}
				}
			}

			return UTILITY_FILTER_OUT;
		} else {
			return UTILITY_DEFAULT;
		}
	}
}
