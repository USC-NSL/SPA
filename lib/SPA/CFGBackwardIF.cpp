/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Instructions.h"

#include "spa/CFGBackwardIF.h"

namespace SPA {
	CFGBackwardIF::CFGBackwardIF( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets ) {
		// Use a worklist to add all predecessors of target instruction.
		std::set<llvm::Instruction *> worklist = targets;
		std::set<llvm::Instruction *> reaching;
		std::set<llvm::Function *> reachingFns;

		while ( ! worklist.empty() ) {
			std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
			llvm::Instruction *inst = *it;
			worklist.erase( it );

			// Mark instruction as reaching.
			reaching.insert( inst );
			std::set<llvm::Instruction *> p = cfg.getPredecessors( inst );
			// Add all non-reaching predecessors to work list.
			for ( it = p.begin(), ie = p.end(); it != ie; it++ )
				if ( reaching.count( *it ) == 0 )
					worklist.insert( *it );
				// Check for CallInst or InvokeInst and add entire function to work list.
				llvm::Function *fn = NULL;
				if ( llvm::InvokeInst *ii = llvm::dyn_cast<llvm::InvokeInst>( inst ) )
					fn = ii->getCalledFunction();
				if ( llvm::CallInst *ci = llvm::dyn_cast<llvm::CallInst>( inst ) )
					fn = ci->getCalledFunction();
				if ( fn != NULL && reachingFns.count( fn ) == 0 ) {
					reachingFns.insert( fn );
					for ( CFG::iterator it2 = cfg.begin(), ie2 = cfg.end(); it2 != ie2; it2++ )
						if ( (*it2)->getParent()->getParent() == fn && reaching.count( *it2 ) == 0 )
							worklist.insert( *it2 );
				}
				// Check if entry instruction.
				if ( inst == &(inst->getParent()->getParent()->getEntryBlock().front()) ) {
					p = cg.getPossibleCallers( inst->getParent()->getParent() );
					// Add all non-reaching callers to work list.
					for ( it = p.begin(), ie = p.end(); it != ie; it++ )
						if ( reaching.count( *it ) == 0 )
							worklist.insert( *it );
				}
		}

		// Define filter out set as opposite of reaching set.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			if ( reaching.count( *it ) == 0 )
				filterOut.insert( *it );
	}
	
	bool CFGBackwardIF::checkInstruction( llvm::Instruction *instruction ) {
		return filterOut.count( instruction ) == 0;
	}
}
