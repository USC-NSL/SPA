/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __FunctionIF_H__
#define __FunctionIF_H__

#include <set>

#include <spa/CFG.h>
#include <spa/CG.h>
#include <spa/InstructionFilter.h>

namespace SPA {
	class FunctionIF : public InstructionFilter {
	private:
		std::set<llvm::Instruction *> filterOut;

	public:
		FunctionIF( CFG &cfg, CG &cg, llvm::Function *fn );
		bool checkInstruction( llvm::Instruction *instruction );
	};
}

#endif // #ifndef __FunctionIF_H__
