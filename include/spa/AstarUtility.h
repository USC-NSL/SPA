/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __AstarUtility_H__
#define __AstarUtility_H__

#include "spa/StateUtility.h"
#include "spa/DepthUtility.h"
#include "spa/TargetDistanceUtility.h"
#include "spa/CFG.h"
#include "spa/CG.h"

namespace SPA {
	class AstarUtility : public StateUtility {
	private:
		DepthUtility depth;
		TargetDistanceUtility targetDistance;

	public:
		AstarUtility( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets ) :
			targetDistance( cfg, cg, targets ) { }
		AstarUtility( CFG &cfg, CG &cg, InstructionFilter &filter ) :
			targetDistance( cfg, cg, filter ) { }

		double getUtility( const klee::ExecutionState *state ) {
			return targetDistance.getUtility( state ) - depth.getUtility( state );
		}

		std::string getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction ) {
			return targetDistance.getColor( cfg, cg, instruction );
		}
	};
}

#endif // #ifndef __AstarUtility_H__
