/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Instructions.h"

#include "spa/FunctionIF.h"

namespace SPA {
	FunctionIF::FunctionIF( CFG &cfg, CG &cg, llvm::Function *fn ) {
		// Use a worklist to add fn and all functions called by it.
		std::set<llvm::Instruction *> filterIn;
		std::set<llvm::Function *> fnWorklist;
		std::set<llvm::Function *> fnProcessed;

		fnWorklist.insert( fn );

		while ( ! fnWorklist.empty() ) {
			std::set<llvm::Function *>::iterator it = fnWorklist.begin(), ie;
			llvm::Function *fn = *it;
			fnWorklist.erase( it );

			if ( fnProcessed.count( fn ) )
				continue;

			// Mark function as processed.
			fnProcessed.insert( fn );

			// Add entire function to filterIn set.
			for ( CFG::iterator it2 = cfg.begin(), ie2 = cfg.end(); it2 != ie2; it2++ ) {
				if ( (*it2)->getParent()->getParent() == fn ) {
					filterIn.insert( *it2 );
					// Add possible called functions to function work list (maybe a call-site).
					fnWorklist.insert( cg.getPossibleCallees( *it2 ).begin(), cg.getPossibleCallees( *it2 ).end() );
				}
			}
		}

		// Define filter out set as opposite of filterIn set.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			if ( filterIn.count( *it ) == 0 )
				filterOut.insert( *it );
	}

	bool FunctionIF::checkInstruction( llvm::Instruction *instruction ) {
		return filterOut.count( instruction ) == 0;
	}
}
