/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/NegatedIF.h"

namespace SPA {
	bool NegatedIF::checkInstruction( llvm::Instruction *instruction ) {
		return ! subFilter->checkInstruction( instruction );
	}
}
