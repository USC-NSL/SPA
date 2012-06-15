/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __TargetDistanceUtility_H__
#define __TargetDistanceUtility_H__

#include "spa/StateUtility.h"
#include "spa/CFG.h"
#include "spa/CG.h"

namespace SPA {
	class TargetDistanceUtility : public StateUtility {
	private:
		// Stores precomputed instruction distances.
		// The bool indicates whether it is partial (false) or final (true).
		// A state's distance is calculated by traversing the stack and adding up
		// partial distances until a final one is found.
		std::map<llvm::Instruction *, std::pair<double, bool> > distances;
		void processWorklist( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &worklist );
		double getDistance( llvm::Instruction *instruction ) {
			return distances.count( instruction )
				? distances[instruction].first
				: INFINITY;
		}
		bool isFinal( llvm::Instruction *instruction ) {
			return distances.count( instruction )
				? distances[instruction].second
				: true;
		}

	public:
		TargetDistanceUtility( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets );
		TargetDistanceUtility( CFG &cfg, CG &cg, InstructionFilter &filter );
		double getUtility( const klee::ExecutionState *state );
	};
}

#endif // #ifndef __TargetDistanceUtility_H__
