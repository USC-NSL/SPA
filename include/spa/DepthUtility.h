/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __DepthUtility_H__
#define __DepthUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
	class DepthUtility : public StateUtility {
	public:
		DepthUtility() { }
		double getUtility( const klee::ExecutionState *state );
	};
}

#endif // #ifndef __DepthUtility_H__
