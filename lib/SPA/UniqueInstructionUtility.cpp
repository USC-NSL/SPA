/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/UniqueInstructionUtility.h>

#include <cloud9/worker/TreeObjects.h>

namespace SPA {
	double UniqueInstructionUtility::getUtility( klee::ExecutionState *state ) {
		assert( state );
		assert( state->getCloud9State() );

		double result = 0;
		std::set<klee::KInstruction *> uniqueInstrs;
		for ( std::vector<klee::KInstruction*>::iterator it = state->getCloud9State()->_instrProgress.begin(), ie = state->getCloud9State()->_instrProgress.end(); it != ie; it++ )
			if ( uniqueInstrs.insert( *it ).second )
				result++;

		return result;
	}
}
