/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <set>

#include <llvm/Instruction.h>

#include <spa/InstructionFilter.h>

#ifndef __WhitelistIF_H__
#define __WhitelistIF_H__

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
