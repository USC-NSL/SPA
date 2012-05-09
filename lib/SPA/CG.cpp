/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/CG.h"

#include "llvm/Instructions.h"
#include "llvm/Analysis/DebugInfo.h"

#include "cloud9/Logger.h"

using namespace llvm;

namespace SPA {
	CG::CG( CFG &cfg ) {
		// Iterate instructions.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			// Check for CallInst or InvokeInst.
			if ( const InvokeInst *ii = dyn_cast<InvokeInst>( *it ) ) {
				if ( ! ii->getCalledFunction() ) {
					DILocation *l = (DILocation *) (*it)->getDebugLoc().getAsMDNode( (*it)->getContext() );
					CLOUD9_INFO( "Indirect function call found in function " << (*it)->getParent()->getParent()->getName().str() << " in file " << l->getFilename().str() << ":" << l->getLineNumber() << "." );
				}
				functions.insert( ii->getCalledFunction() );
				callers[ii->getCalledFunction()].insert( *it );
			}
			if ( const CallInst *ci = dyn_cast<CallInst>( *it ) ) {
				if ( ! ci->getCalledFunction() ) {
					DILocation *l = (DILocation *) (*it)->getDebugLoc().getAsMDNode( (*it)->getContext() );
					if ( l )
						CLOUD9_INFO( "Indirect function call found in function " << (*it)->getParent()->getParent()->getName().str() << " in file " << l->getFilename().str() << ":" << l->getLineNumber() << "." );
					else
						CLOUD9_INFO( "Indirect function call found in function " << (*it)->getParent()->getParent()->getName().str() << "." );
				}
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

	const std::set<Instruction *> &CG::getCallers( Function *function ) {
		return callers[function];
	}
}
