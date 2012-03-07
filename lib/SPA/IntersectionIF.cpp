/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/IntersectionIF.h"

namespace SPA {
	void IntersectionIF::addIF( InstructionFilter *instructionFilter ) {
		subFilters.insert( instructionFilter );
	}

	bool IntersectionIF::checkInstruction( llvm::Instruction *instruction ) {
		for ( std::set<InstructionFilter *>:: iterator it = subFilters.begin(), ie = subFilters.end(); it != ie; it++ )
			if ( ! (*it)->checkInstruction( instruction ) )
				return false;
		return true;
	}
}
