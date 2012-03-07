/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/UnionIF.h"

namespace SPA {
	void UnionIF::addIF( InstructionFilter *instructionFilter ) {
		subFilters.insert( instructionFilter );
	}

	bool UnionIF::checkInstruction( llvm::Instruction *instruction ) {
		for ( std::set<InstructionFilter *>:: iterator it = subFilters.begin(), ie = subFilters.end(); it != ie; it++ )
			if ( (*it)->checkInstruction( instruction ) )
				return true;
		return false;
	}
}
