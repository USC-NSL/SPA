/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CFGForwardIF_H__
#define __CFGForwardIF_H__

#include <set>

#include <spa/CFG.h>
#include <spa/CG.h>
#include <spa/InstructionFilter.h>

namespace SPA {
	class CFGForwardIF : public InstructionFilter {
	private:
		std::set<llvm::Instruction *> filterOut;

	public:
		CFGForwardIF( CFG &cfg, CG &cg, std::set<llvm::Instruction *> &startingPoints );
		bool checkInstruction( llvm::Instruction *instruction );
	};
}

#endif // #ifndef __CFGForwardIF_H__
