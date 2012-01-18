/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "CFG.h"

#include "llvm/Instructions.h"

using namespace llvm;

namespace SPA {
	CFG::CFG( Module *module ) {
		// Iterate functions.
		for ( Module::iterator mit = module->begin(), mie = module->end(); mit != mie; mit++ ) {
			Function &fn = *mit;
			// Iterate basic blocks.
			for ( Function::iterator fit = fn.begin(), fie = fn.end(); fit != fie; fit++ ) {
				BasicBlock &bb = *fit;
				// Connect across basic blocks.
				// Iterate successors.
				TerminatorInst *ti = bb.getTerminator();
				unsigned int ns = ti->getNumSuccessors();
				for ( unsigned int i = 0; i < ns; i++ ) {
					// Add entry (even if with no successors) for the current predecessor.
					predecessors[&(bb.front())];
					successors[ti];
					predecessors[&(ti->getSuccessor( i )->front())].insert( ti );
					successors[ti].insert( &(ti->getSuccessor( i )->front()) );
				}
				// Connect within basic block.
				// Iterate instructions.
				BasicBlock::iterator bbit = bb.begin(), bbie = bb.end();
				instructions.insert( &(*bbit) );
				Instruction *prevInst = &(*(bbit++));
				for ( ; bbit != bbie; bbit++ ) {
					Instruction *inst = &(*bbit);
					instructions.insert( inst );
					predecessors[inst];
					successors[inst];
					predecessors[inst].insert( prevInst );
					successors[prevInst].insert( inst );
					prevInst = inst;
				}
			}
		}
	}

	CFG::iterator CFG::begin() {
		return instructions.begin();
	}
	
	CFG::iterator CFG::end() {
		return instructions.end();
	}
	
	std::set<Instruction *> CFG::getSuccessors( Instruction *instruction ) {
		return successors[instruction];
	}

	std::set<Instruction *> CFG::getPredecessors( Instruction *instruction ) {
		return predecessors[instruction];
	}
}
