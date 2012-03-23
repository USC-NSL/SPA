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
		std::map<llvm::Function *,std::set<llvm::Instruction *> > callers;
		std::set<llvm::Function *> functions;
		
	public:
		typedef std::set<llvm::Function *>::iterator iterator;

		CG( CFG &cfg );
		iterator begin();
		iterator end();
		const std::set<llvm::Instruction *> &getCallers( llvm::Function *function );
	};
}

#endif // #ifndef __CG_H__
