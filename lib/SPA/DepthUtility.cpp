/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/DepthUtility.h>

namespace SPA {
	double DepthUtility::getUtility( klee::ExecutionState *state ) {
		assert( state );

		return (double) state->stateTime;
	}
}
