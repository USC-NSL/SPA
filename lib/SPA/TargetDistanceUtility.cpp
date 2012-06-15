/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/TargetDistanceUtility.h>

#include <klee/Internal/Module/KInstruction.h>


namespace SPA {
	TargetDistanceUtility::TargetDistanceUtility( CFG &cfg, CG &cg, InstructionFilter &filter ) {
		std::set<llvm::Instruction *> worklist;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			if ( filter.checkInstruction( *it ) ) {
				distances[*it] = std::pair<double, bool>( +INFINITY, false );
			} else {
				distances[*it] = std::pair<double, bool>( 0, true );
				worklist.insert( *it );
			}
		}

		processWorklist( cfg, cg, worklist );
	}

	TargetDistanceUtility::TargetDistanceUtility( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets ) {
		std::set<llvm::Instruction *> worklist;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			distances[*it] = std::pair<double, bool>( +INFINITY, false );

		for ( std::set<llvm::Instruction *>::iterator it = targets.begin(), ie = targets.end(); it != ie; it++ ) {
			distances[*it] = std::pair<double, bool>( 0, true );
			worklist.insert( cfg.getPredecessors( *it ).begin(), cfg.getPredecessors( *it ).end() );
		}

		processWorklist( cfg, cg, worklist );
	}

	void TargetDistanceUtility::processWorklist( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &worklist ) {
		CLOUD9_DEBUG( "      Processing direct paths." );
		while ( ! worklist.empty() ) {
			std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
			llvm::Instruction *inst = *it;
			worklist.erase( it );

			bool updated = false;

			// Check cost of successors.
			for ( it = cfg.getSuccessors( inst ).begin(), ie = cfg.getSuccessors( inst ).end(); it != ie; it++ ) {
				if ( distances[inst].first > distances[*it].first + 1 ) {
					distances[inst] = std::pair<double, bool>( distances[*it].first + 1, true );
					updated = true;
				}
			}
			// Check cost of callees.
			for ( std::set<llvm::Function *>::const_iterator it = cg.getDefiniteCallees( inst ).begin(), ie = cg.getDefiniteCallees( inst ).end(); it != ie; it++ ) {
				if ( (! (*it)->empty()) && distances[inst].first > distances[&(*it)->getEntryBlock().front()].first + 1 ) {
					distances[inst] = std::pair<double, bool>( distances[&(*it)->getEntryBlock().front()].first + 1, true );
					updated = true;
				}
			}

			if ( updated ) {
				// Propagate changes to predecessors.
				worklist.insert( cfg.getPredecessors( inst ).begin(), cfg.getPredecessors( inst ).end() );
				// Check if entry instruction and propagate to callers.
				if ( (! inst->getParent()->getParent()->empty()) && inst == &(inst->getParent()->getParent()->getEntryBlock().front()) )
					worklist.insert( cg.getPossibleCallers( inst->getParent()->getParent() ).begin(), cg.getPossibleCallers( inst->getParent()->getParent() ).end() );
			}
		}

		CLOUD9_DEBUG( "      Processing indirect paths." );
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			if ( (! distances[*it].second) && cfg.getSuccessors( *it ).empty() ) {
				distances[*it] = std::pair<double, bool>( 0, false );
				worklist.insert( cfg.getPredecessors( *it ).begin(), cfg.getPredecessors( *it ).end() );
			}
		}
		while ( ! worklist.empty() ) {
			std::set<llvm::Instruction *>::iterator it = worklist.begin(), ie;
			llvm::Instruction *inst = *it;
			worklist.erase( it );

			if ( distances[inst].second )
				continue;

			// Check cost of successors.
			bool updated = false;
			for ( it = cfg.getSuccessors( inst ).begin(), ie = cfg.getSuccessors( inst ).end(); it != ie; it++ ) {
				if ( distances[inst].first > distances[*it].first + 1 ) {
					distances[inst] = std::pair<double, bool>( distances[*it].first + 1, false );
					updated = true;
				}
			}
			if ( updated )
				// Propagate changes to predecessors.
				worklist.insert( cfg.getPredecessors( inst ).begin(), cfg.getPredecessors( inst ).end() );
		}
	}

	double TargetDistanceUtility::getUtility( const klee::ExecutionState *state ) {
		assert( state );

		double result = - getDistance( state->pc()->inst );
		if ( isFinal( state->pc()->inst ) )
			return result;

		for ( klee::ExecutionState::stack_ty::const_reverse_iterator it = state->stack().rbegin(), ie = state->stack().rend(); it != ie && it->caller; it++ ) {
			result -= getDistance( it->caller->inst );
			if ( isFinal( it->caller->inst ) )
				return result;
		}
		// No final state was found.
		return - INFINITY;
	}
}
