/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/CG.h"

#include <vector>

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
				if ( ii->getCalledFunction() )
					functions.insert( ii->getCalledFunction() );
				definiteCallers[ii->getCalledFunction()].insert( *it );
				possibleCallers[ii->getCalledFunction()].insert( *it );
			}
			if ( const CallInst *ci = dyn_cast<CallInst>( *it ) ) {
				if ( ci->getCalledFunction() )
					functions.insert( ci->getCalledFunction() );
				definiteCallers[ci->getCalledFunction()].insert( *it );
				possibleCallers[ci->getCalledFunction()].insert( *it );
			}
		}

		// Revisit indirect calls.
		for ( std::set<llvm::Instruction *>::iterator iit = definiteCallers[NULL].begin(), iie = definiteCallers[NULL].end(); iit != iie; iit++ ) {
			// Make a list of argument types.
			std::vector<const llvm::Type *> argTypes;
			if ( const InvokeInst *ii = dyn_cast<InvokeInst>( *iit ) ) {
				for ( unsigned i = 0; i < ii->getNumArgOperands(); i++ )
					argTypes.push_back( ii->getArgOperand( i )->getType() );
			}
			if ( const CallInst *ci = dyn_cast<CallInst>( *iit ) ) {
				for ( unsigned i = 0; i < ci->getNumArgOperands(); i++ )
					argTypes.push_back( ci->getArgOperand( i )->getType() );
			}
			// Look for functions of same type.
			for ( CG::iterator fit = begin(), fie = end(); fit != fie; fit++ ) {
				// Compare argument arity and type.
				if ( argTypes.size() == (*fit)->getFunctionType()->getNumParams() ) {
					unsigned i;
					for ( i = 0; i < argTypes.size(); i++ )
						if ( argTypes[i] != (*fit)->getFunctionType()->getParamType( i ) )
							break;
					if ( i == argTypes.size() )
						possibleCallers[*fit].insert( *iit );
				}
			}
		}
	}
}
