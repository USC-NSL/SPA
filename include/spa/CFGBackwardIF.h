/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CFGBackwardIF_H__
#define __CFGBackwardIF_H__

#include <set>

#include <spa/CFG.h>
#include <spa/CG.h>
#include <spa/InstructionFilter.h>

namespace SPA {
	class CFGBackwardIF : public InstructionFilter {
	private:
		std::set<llvm::Instruction *> filterOut;

	public:
		CFGBackwardIF( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &targets );
		bool checkInstruction( llvm::Instruction *instruction );
	};
}

#endif // #ifndef __CFGBackwardIF_H__
