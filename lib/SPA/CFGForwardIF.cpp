/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Instructions.h"

#include "spa/CFGForwardIF.h"

namespace SPA {
	CFGForwardIF::CFGForwardIF( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &startingPoints ) {
		// Use a work list to add all successors of starting instruction.
		std::set<llvm::Instruction *> worklist = startingPoints;
		std::set<llvm::Instruction *> reachable;

		while ( ! worklist.empty() ) {
			std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
			llvm::Instruction *inst = *it;
			worklist.erase( it );

			// Mark instruction as reachable.
			reachable.insert( inst );
			std::set<llvm::Instruction *> s = cfg.getSuccessors( inst );
			// Add all not reached successors to work list.
			for ( it = s.begin(), ie = s.end(); it != ie; it++ )
				if ( reachable.count( *it ) == 0 )
					worklist.insert( *it );
				// Check for CallInst or InvokeInst and add entire function to work list.
				llvm::Function *fn = NULL;
				if ( llvm::InvokeInst *ii = llvm::dyn_cast<llvm::InvokeInst>( inst ) )
					fn = ii->getCalledFunction();
				if ( llvm::CallInst *ci = llvm::dyn_cast<llvm::CallInst>( inst ) )
					fn = ci->getCalledFunction();
				if ( fn != NULL )
					for ( CFG::iterator it2 = cfg.begin(), ie2 = cfg.end(); it2 != ie2; it2++ )
						if ( (*it2)->getParent()->getParent() == fn && reachable.count( *it2 ) == 0 )
							worklist.insert( *it2 );
		}

		// Define filter out set as opposite of reachable set.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			if ( reachable.count( *it ) == 0 )
				filterOut.insert( *it );
	}

	bool CFGForwardIF::checkInstruction( llvm::Instruction *instruction ) {
		return filterOut.count( instruction ) == 0;
	}
}
