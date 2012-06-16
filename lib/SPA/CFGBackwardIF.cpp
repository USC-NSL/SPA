/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Instructions.h"

#include "cloud9/Logger.h"

#include "spa/CFGBackwardIF.h"

namespace SPA {
	CFGBackwardIF::CFGBackwardIF( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets ) {
		// Use a worklist to add all predecessors of target instruction.
		std::set<llvm::Instruction *> worklist = targets;
		std::set<llvm::Instruction *> reaching;
		std::set<llvm::Function *> fnWorklist;
		std::set<llvm::Function *> fnProcessed;

		CLOUD9_DEBUG( "      Exploring reverse path." );
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
			// Add possible called functions to function work list (maybe a call-site).
			fnWorklist.insert( cg.getPossibleCallees( inst ).begin(), cg.getPossibleCallees( inst ).end() );
			// Check if entry instruction.
			if ( inst == &(inst->getParent()->getParent()->getEntryBlock().front()) ) {
				p = cg.getPossibleCallers( inst->getParent()->getParent() );
				// Add all non-reaching callers to work list.
				for ( it = p.begin(), ie = p.end(); it != ie; it++ )
					if ( reaching.count( *it ) == 0 )
						worklist.insert( *it );
			}
		}

		CLOUD9_DEBUG( "      Exploring called functions." );
		while ( ! fnWorklist.empty() ) {
			std::set<llvm::Function *>::iterator it = fnWorklist.begin(), ie;
			llvm::Function *fn = *it;
			fnWorklist.erase( it );

			// Mark function as processed.
			fnProcessed.insert( fn );

			// Add entire function to reaching set.
			for ( std::vector<llvm::Instruction *>::const_iterator it2 = cfg.getInstructions( fn ).begin(), ie2 = cfg.getInstructions( fn ).end(); it2 != ie2; it2++ ) {
				reaching.insert( *it2 );
				for ( std::set<llvm::Function *>::iterator it3 = cg.getPossibleCallees( *it2 ).begin(), ie3 = cg.getPossibleCallees( *it2 ).end(); it3 != ie3; it3++ )
					// Add possible called functions to function work list (maybe a call-site).
					if ( ! fnProcessed.count( *it3 ) )
						fnWorklist.insert( *it3 );
			}
		}

		// Define filter out set as opposite of reaching set.
		CLOUD9_DEBUG( "      Defining filter." );
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			if ( reaching.count( *it ) == 0 )
				filterOut.insert( *it );
	}

	bool CFGBackwardIF::checkInstruction( llvm::Instruction *instruction ) {
		return filterOut.count( instruction ) == 0;
	}
}
