/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/CFGBackwardFilter.h"

#include "llvm/Instructions.h"
#include <klee/Internal/Module/KInstruction.h>
#include "cloud9/Logger.h"

namespace SPA {
	CFGBackwardFilter::CFGBackwardFilter( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets ) {
		// Use a worklist to add all predecessors of target instruction.
		std::set<llvm::Instruction *> worklist = targets;
		std::set<llvm::Function *> pathFunctions;

		CLOUD9_DEBUG( "      Exploring reverse path." );
		while ( ! worklist.empty() ) {
			std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
			llvm::Instruction *inst = *it;
			worklist.erase( it );

			// Mark instruction as reaching.
			reaching[inst] = true;
			std::set<llvm::Instruction *> p = cfg.getPredecessors( inst );
			// Add all non-reaching predecessors to work list.
			for ( it = p.begin(), ie = p.end(); it != ie; it++ )
				if ( reaching.count( *it ) == 0 )
					worklist.insert( *it );
			// Mark function as on direct path.
			pathFunctions.insert( inst->getParent()->getParent() );
			// Check if entry instruction.
			if ( inst == &(inst->getParent()->getParent()->getEntryBlock().front()) ) {
				p = cg.getPossibleCallers( inst->getParent()->getParent() );
				// Add all non-reaching callers to work list.
				for ( it = p.begin(), ie = p.end(); it != ie; it++ )
					if ( reaching.count( *it ) == 0 )
						worklist.insert( *it );
			}
		}

		// Define filter out set as opposite of reaching set within analyzed direct path functions.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			if ( pathFunctions.count( (*it)->getParent()->getParent() ) && ! reaching.count( *it ) )
				reaching[*it] = false;
	}

	bool CFGBackwardFilter::checkInstruction( llvm::Instruction *instruction ) {
		return reaching.count( instruction ) && reaching[instruction];
	}

	double CFGBackwardFilter::getUtility( const klee::ExecutionState *state ) {
		bool known = true;
		// Traverse call graph from root to current stack position.
		// To filter out, must traverse a known reaching point, followed by a known non-reaching point.
		// Assume a known reaching point was reached prior to the root.
		for ( klee::ExecutionState::stack_ty::const_iterator it = state->stack().begin(), ie = state->stack().end(); it != ie; it++ ) {
			if ( it->caller ) {
				if ( reaching.count( it->caller->inst ) == 0 )
					known = false;
				else if ( reaching[it->caller->inst] )
					known = true;
				else if ( known )
					return UTILITY_FILTER_OUT;
			}
		}
		if ( known && reaching.count( state->pc()->inst ) && ! reaching[state->pc()->inst] )
			return UTILITY_FILTER_OUT;
		return UTILITY_DEFAULT;
	}

	std::string CFGBackwardFilter::getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction ) {
		if ( reaching.count( instruction ) == 0 )
			return "grey";
		else if ( reaching[instruction] )
			return "white";
		else
			return "black";
	}
}
