/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/CG.h"

#include "llvm/Instructions.h"

using namespace llvm;

namespace SPA {
	CG::CG( CFG &cfg ) {
		// Iterate instructions.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			// Check for CallInst or InvokeInst.
			if ( const InvokeInst *ii = dyn_cast<InvokeInst>( *it ) ) {
				functions.insert( ii->getCalledFunction() );
				callers[ii->getCalledFunction()].insert( *it );
			}
			if ( const CallInst *ci = dyn_cast<CallInst>( *it ) ) {
				functions.insert( ci->getCalledFunction() );
				callers[ci->getCalledFunction()].insert( *it );
			}
		}
	}

	CG::iterator CG::begin() {
		return functions.begin();
	}

	CG::iterator CG::end() {
		return functions.end();
	}

	std::set<Instruction *> CG::getCallers( Function *function ) {
		return callers[function];
	}
}
