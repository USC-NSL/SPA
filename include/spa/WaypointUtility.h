/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __WaypointUtility_H__
#define __WaypointUtility_H__

#include "spa/StateUtility.h"

#include <spa/CFGBackwardFilter.h>

namespace SPA {
	class WaypointUtility : public StateUtility {
	private:
		std::map<unsigned int, CFGBackwardFilter *> filters;
		bool mandatory;

	public:
		WaypointUtility( CFG &cfg, CG &cg, std::map<unsigned int, std::set<llvm::Instruction *> > &_waypoints, bool _mandatory );
		double getUtility( const klee::ExecutionState *state );
		std::string getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction );
	};
}

#endif // #ifndef __WaypointUtility_H__
