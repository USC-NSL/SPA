/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __WhitelistIF_H__
#define __WhitelistIF_H__

#include <set>

#include <llvm/IR/Instruction.h>

#include <spa/InstructionFilter.h>

namespace SPA {
	class WhitelistIF : public InstructionFilter {
	private:
		std::set<llvm::Instruction *> whitelist;

	public:
		WhitelistIF( std::set<llvm::Instruction *> _whitelist ) : whitelist( _whitelist ) { }
		bool checkInstruction( llvm::Instruction *instruction );
	};
}

#endif // #ifndef __WhitelistIF_H__
