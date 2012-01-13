/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <map>
#include <set>

#include "CFG.h"

#ifndef __CG_H__
#define __CG_H__

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
		std::set<llvm::Instruction *> getCallers( llvm::Function *function );
	};
}

#endif // #ifndef __CG_H__
