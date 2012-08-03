/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __RecoverStateUtility_H__
#define __RecoverStateUtility_H__

#include "spa/StateUtility.h"

#include <cloud9/worker/JobManager.h>


namespace SPA {
	class RecoverStateUtility : public StateUtility {
	private:
		cloud9::worker::JobManager *jobManager;
		std::set< std::vector<int> > paths;

	public:
		RecoverStateUtility( cloud9::worker::JobManager *_jobManager, std::istream &stateFile );
		double getUtility( klee::ExecutionState *state );
		std::string getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction ) { return "white"; }
	};
}

#endif // #ifndef __RecoverStateUtility_H__
