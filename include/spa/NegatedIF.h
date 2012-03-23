/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __NegatedIF_H__
#define __NegatedIF_H__

#include <set>

#include <spa/InstructionFilter.h>

namespace SPA {
	class NegatedIF : public InstructionFilter {
	private:
		InstructionFilter *subFilter;

	public:
		NegatedIF( InstructionFilter *_subFilters ) : subFilter( _subFilters ) { }
		bool checkInstruction( llvm::Instruction *instruction );
	};
}

#endif // #ifndef __NegatedIF_H__
