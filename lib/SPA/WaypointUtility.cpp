/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/WaypointUtility.h>

#include <sstream>

#include "../../lib/Core/Memory.h"
#include <klee/Internal/Module/KInstruction.h>

#include <spa/SPA.h>

namespace SPA {
	WaypointUtility::WaypointUtility( CFG &cfg, CG &cg, llvm::Function *entryFunction, std::map<unsigned int, std::set<llvm::Instruction *> > &waypoints, bool _mandatory ) :
		mandatory( _mandatory ) {
		for ( std::map<unsigned int, std::set<llvm::Instruction *> >::iterator it = waypoints.begin(), ie = waypoints.end(); it != ie; it++ )
			filters[it->first] = new CFGBackwardFilter( cfg, cg, entryFunction, it->second );
	}

	bool checkWaypoint( const klee::ExecutionState *state, unsigned int id ) {
		const klee::MemoryObject *addrMO = NULL;
		for ( std::vector< std::pair<const klee::MemoryObject*,const klee::Array*> >::const_iterator it = state->symbolics.begin(), ie = state->symbolics.end(); it != ie; it++ ) {
			if ( it->first->name == SPA_WAYPOINTS_VARIABLE ) {
				addrMO = it->first;
				break;
			}
		}
		if ( ! addrMO )
			return false;

		const klee::ObjectState *addrOS = state->addressSpace().findObject( addrMO );
		assert( addrOS && "waypointsPtr not set." );

		klee::ref<klee::Expr> addrExpr = addrOS->read( 0, klee::Context::get().getPointerWidth() );
		assert( isa<klee::ConstantExpr>( addrExpr ) && "waypointsPtr is symbolic." );
		klee::ref<klee::ConstantExpr> address = cast<klee::ConstantExpr>(addrExpr);
		klee::ObjectPair op;
		assert( ((klee::AddressSpace) state->addressSpace()).resolveOne( address, op ) && "waypointsPtr is not uniquely defined." );
		const klee::MemoryObject *mo = op.first;
		const klee::ObjectState *os = op.second;

		unsigned ioffset = 0;
		klee::ref<klee::Expr> offset_expr = klee::SubExpr::create( address, op.first->getBaseExpr() );
		assert( isa<klee::ConstantExpr>( offset_expr ) && "waypoints is an invalid buffer." );
		klee::ref<klee::ConstantExpr> value = cast<klee::ConstantExpr>( offset_expr.get() );
		ioffset = value.get()->getZExtValue();
		assert( ioffset < mo->size );

		klee::ref<klee::Expr> wp = os->read8( (id>>3) + ioffset );

		assert( isa<klee::ConstantExpr>( wp ) && "Symbolic byte in waypoints." );
		return cast<klee::ConstantExpr>( wp )->getZExtValue( 8 ) & 1<<(id & 0x7);
	}

	double WaypointUtility::getUtility( const klee::ExecutionState *state ) {
		unsigned int count = 0;

		for ( std::map<unsigned int, CFGBackwardFilter *>::iterator it = filters.begin(), ie = filters.end(); it != ie; it++ ) {
			if ( it->second->getUtility( state ) != UTILITY_FILTER_OUT || checkWaypoint( state, it->first ) ) {
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
		for ( std::map<unsigned int, CFGBackwardFilter *>::iterator it = filters.begin(), ie = filters.end(); it != ie; it++ )
			if ( it->second->checkInstruction( instruction ) )
				count++;

		std::stringstream result;
		// None = White, All = Green
		result << "0.33+" << ((double) count / (double) filters.size()) << "+1.0";
		return result.str();
	}
}
