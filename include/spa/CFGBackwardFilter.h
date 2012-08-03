/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CFGBackwardFilter_H__
#define __CFGBackwardFilter_H__

#include <set>

#include <spa/CFG.h>
#include <spa/CG.h>
#include <spa/InstructionFilter.h>

namespace SPA {
	class CFGBackwardFilter : public InstructionFilter, public StateUtility {
	private:
		std::map<llvm::Instruction *, bool> reaching;

	public:
		CFGBackwardFilter( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets );
		bool checkInstruction( llvm::Instruction *instruction );
		double getUtility( klee::ExecutionState *state );
		std::string getColor( CFG &cfg, CG &cg, llvm::Instruction *instruction );
	};
}

#endif // #ifndef __CFGBackwardFilter_H__
