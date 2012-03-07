/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/WhitelistIF.h"

namespace SPA {
	bool WhitelistIF::checkInstruction( llvm::Instruction *instruction ) {
		return whitelist.count( instruction );
	}
}
