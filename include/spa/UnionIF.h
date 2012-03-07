/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <set>

#include <spa/InstructionFilter.h>

#ifndef __UnionIF_H__
#define __UnionIF_H__

namespace SPA {
	class UnionIF : public InstructionFilter {
	private:
		std::set<InstructionFilter *> subFilters;

	public:
		UnionIF();
		UnionIF( std::set<InstructionFilter *> _subFilters ) : subFilters( _subFilters ) { }
		void addIF( InstructionFilter *instructionFilter );
		bool checkInstruction( llvm::Instruction *instruction );
	};
}

#endif // #ifndef __UnionIF_H__
