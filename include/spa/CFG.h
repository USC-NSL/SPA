/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <map>
#include <set>
#include <ostream>

#include "llvm/Module.h"
#include "llvm/Instruction.h"

#include "spa/InstructionFilter.h"

#ifndef __CFG_H__
#define __CFG_H__

namespace SPA {
	class CFG {
	private:
		std::map<llvm::Instruction *,std::set<llvm::Instruction *> > predecessors, successors;
		std::set<llvm::Instruction *> instructions;

	public:
		typedef std::set<llvm::Instruction *>::iterator iterator;

		CFG( llvm::Module *module );
		iterator begin();
		iterator end();
		std::set<llvm::Instruction *> getSuccessors( llvm::Instruction *instruction );
		std::set<llvm::Instruction *> getPredecessors( llvm::Instruction *instruction );
		void dump( std::ostream &dotFile, std::map<InstructionFilter *, std::string> &annotations );
	};
}

#endif // #ifndef __CFG_H__
