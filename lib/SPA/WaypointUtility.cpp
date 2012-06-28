/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/WaypointUtility.h>

#include <sstream>

#include "../../lib/Core/Memory.h"
#include <klee/Internal/Module/KInstruction.h>

#include <spa/SPA.h>
#include <spa/CFGBackwardIF.h>

namespace SPA {
	WaypointUtility::WaypointUtility( CFG &cfg, CG &cg, std::map<unsigned int, std::set<llvm::Instruction *> > &waypoints, bool _mandatory ) :
		mandatory( _mandatory ) {
		for ( std::map<unsigned int, std::set<llvm::Instruction *> >::iterator it = waypoints.begin(), ie = waypoints.end(); it != ie; it++ )
			filters[it->first] = new CFGBackwardIF( cfg, cg, it->second );
	}

	bool checkWaypoint( const klee::ExecutionState *state, unsigned int id ) {
		const klee::MemoryObject *waypointsMO = NULL;
		for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::const_iterator it = state->symbolics.begin(), ie = state->symbolics.end(); it != ie; it++ ) {
			if ( it->first->name == SPA_WAYPOINTS_VARIABLE ) {
				waypointsMO = it->first;
				break;
			}
		}
		if ( ! waypointsMO )
			return false;

		const klee::ObjectState *waypointsOS = state->addressSpace().findObject( waypointsMO );
		assert( waypointsOS && "Waypoints variable not set." );

		klee::ref<klee::Expr> waypointsExpr = waypointsOS->read( id>>3, 1 );
		assert( isa<klee::ConstantExpr>( waypointsExpr ) && "Waypoints variable is symbolic." );
		klee::ref<klee::ConstantExpr> waypoints = cast<klee::ConstantExpr>(waypointsExpr);

		return waypoints->getZExtValue( 8 ) & 1<<(id & 0x7);
	}

	double WaypointUtility::getUtility( const klee::ExecutionState *state ) {
		unsigned int count = 0;

		for ( std::map<unsigned int, InstructionFilter *>::iterator it = filters.begin(), ie = filters.end(); it != ie; it++ ) {
			if ( it->second->checkInstruction( state->pc()->inst ) || checkWaypoint( state, it->first ) ) {
				count++;
				continue;
			}
			if ( mandatory )
				return UTILITY_FILTER_OUT;
		}

		return mandatory ? UTILITY_DEFAULT : count;
	}

	std::string WaypointUtility::getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction ) {
		unsigned int count = 0;
		for ( std::map<unsigned int, InstructionFilter *>::iterator it = filters.begin(), ie = filters.end(); it != ie; it++ )
			if ( it->second->checkInstruction( instruction ) )
				count++;

		std::stringstream result;
		// None = White, All = Green
		result << "0.33+" << count / (double) filters.size() << "+1.0";
		return result.str();
	}
}
