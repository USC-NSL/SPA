/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __UniqueInstructionUtility_H__
#define __UniqueInstructionUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
	class UniqueInstructionUtility : public StateUtility {
	public:
		UniqueInstructionUtility() { }
		double getUtility( const klee::ExecutionState *state );
	};
}

#endif // #ifndef __UniqueInstructionUtility_H__
