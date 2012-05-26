/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CG_H__
#define __CG_H__

#include <map>
#include <set>

#include "CFG.h"

namespace SPA {
	class CG {
	private:
		std::map<llvm::Function *,std::set<llvm::Instruction *> > definiteCallers;
		std::map<llvm::Function *,std::set<llvm::Instruction *> > possibleCallers;
		std::set<llvm::Function *> functions;

	public:
		typedef std::set<llvm::Function *>::iterator iterator;

		CG( CFG &cfg );
		iterator begin() { return functions.begin(); }
		iterator end() { return functions.end(); }
		const std::set<llvm::Instruction *> &getDefiniteCallers( llvm::Function *function ) { return definiteCallers[function]; }
		const std::set<llvm::Instruction *> &getPossibleCallers( llvm::Function *function ) { return possibleCallers[function]; }
	};
}

#endif // #ifndef __CG_H__
