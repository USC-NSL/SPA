/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __FilteredUtility_H__
#define __FilteredUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
	class FilteredUtility : public StateUtility {
	public:
		double getUtility( klee::ExecutionState *state ) {
			return state->filtered ? UTILITY_FILTER_OUT : UTILITY_DEFAULT;
		}

		std::string getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction ) {
			return "white";
		}
	};
}

#endif // #ifndef __FilteredUtility_H__
