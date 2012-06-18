/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/TargetDistanceUtility.h>

#include <sstream>
#include <klee/Internal/Module/KInstruction.h>


namespace SPA {
	TargetDistanceUtility::TargetDistanceUtility( CFG &cfg, CG &cg, InstructionFilter &filter ) {
		std::set<llvm::Instruction *> worklist;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			if ( filter.checkInstruction( *it ) ) {
				distances[*it] = std::pair<double, bool>( +INFINITY, false );
			} else {
				distances[*it] = std::pair<double, bool>( 0, true );
				propagateChanges( cfg, cg, worklist, *it );
			}
		}

		assert( (! worklist.empty()) && "Filter must exclude something." );
		processWorklist( cfg, cg, worklist );
	}

	TargetDistanceUtility::TargetDistanceUtility( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets ) {
		std::set<llvm::Instruction *> worklist;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ )
			distances[*it] = std::pair<double, bool>( +INFINITY, false );

		for ( std::set<llvm::Instruction *>::iterator it = targets.begin(), ie = targets.end(); it != ie; it++ ) {
			distances[*it] = std::pair<double, bool>( 0, true );
			propagateChanges( cfg, cg, worklist, *it );
		}

		processWorklist( cfg, cg, worklist );
	}

	void TargetDistanceUtility::propagateChanges( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &worklist, llvm::Instruction *instruction ) {
		// Propagate changes to predecessors.
		worklist.insert( cfg.getPredecessors( instruction ).begin(), cfg.getPredecessors( instruction ).end() );
		// Check if entry instruction and propagate to callers.
		if ( (! instruction->getParent()->getParent()->empty()) && instruction == &(instruction->getParent()->getParent()->getEntryBlock().front()) )
			worklist.insert( cg.getPossibleCallers( instruction->getParent()->getParent() ).begin(), cg.getPossibleCallers( instruction->getParent()->getParent() ).end() );
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
			for ( std::set<llvm::Function *>::const_iterator it = cg.getPossibleCallees( inst ).begin(), ie = cg.getPossibleCallees( inst ).end(); it != ie; it++ ) {
				if ( (! (*it)->empty()) && distances[inst].first > distances[&(*it)->getEntryBlock().front()].first + 1 ) {
					distances[inst] = std::pair<double, bool>( distances[&(*it)->getEntryBlock().front()].first + 1, true );
					updated = true;
				}
			}

			if ( updated )
				propagateChanges( cfg, cg, worklist, inst );
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
		return -INFINITY;
	}

	std::string TargetDistanceUtility::getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction ) {
		double minPartial = +INFINITY, maxPartial = -INFINITY,
			minFinal = +INFINITY, maxFinal = -INFINITY;

		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			if ( getDistance( *it ) == INFINITY )
				continue;
			if ( isFinal( *it ) ) {
				minFinal = std::min( minFinal, getDistance( *it ) );
				maxFinal = std::max( maxFinal, getDistance( *it ) );
			} else  if ( (*it)->getParent()->getParent() == instruction->getParent()->getParent() ) {
				minPartial = std::min( minPartial, getDistance( *it ) );
				maxPartial = std::max( maxPartial, getDistance( *it ) );
			}
		}

		std::stringstream result;
		if ( isFinal( instruction ) ) {
			// Min = White, Max = Green
			if ( minFinal < maxFinal && getDistance( instruction ) != INFINITY )
				result << "0.33+" << ((maxFinal - getDistance( instruction )) / (maxFinal - minFinal)) << "+1.0";
			else
				result << "0.33+0.0+1.0";
		} else {
			// Min = White, Max = Blue
			if ( minPartial < maxPartial && getDistance( instruction ) != INFINITY )
				result << "0.67+" << ((maxPartial - getDistance( instruction )) / (maxPartial - minPartial)) << "+1.0";
			else
				result << "0.67+0.0+1.0";
		}
		return result.str();
	}
}
