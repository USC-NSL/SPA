/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __StateUtility_H__
#define __StateUtility_H__

#include "klee/ExecutionState.h"

namespace SPA {
	class StateUtility {
	public:
		/**
		 * Provides a utility metric to a given execution state, allowing some states to be explored earlier by priority.
		 * 
		 * @param state The state to check.
		 * @return The utility metric for the given state (higher = more useful).
		 */
		virtual double getUtility( const klee::ExecutionState *state ) = 0;

	protected:
		virtual ~StateUtility() { }
	};
}

#endif // #ifndef __StateUtility_H__
