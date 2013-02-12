/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/CG.h"

#include <vector>

#include "llvm/Instructions.h"
#include "llvm/Analysis/DebugInfo.h"

#include "cloud9/Logger.h"

using namespace llvm;

namespace SPA {
	CG::CG( llvm::Module *module ) {
		// Iterate functions.
		for ( Module::iterator mit = module->begin(), mie = module->end(); mit != mie; mit++ ) {
			functions.insert( &*mit );
			// Iterate basic blocks.
			for ( Function::iterator fit = mit->begin(), fie = mit->end(); fit != fie; fit++ ) {
				// Iterate instructions.
				for ( BasicBlock::iterator bbit = fit->begin(), bbie = fit->end(); bbit != bbie; bbit++ ) {
					// Check for CallInst or InvokeInst.
					if ( const InvokeInst *ii = dyn_cast<InvokeInst>( &*bbit ) ) {
						if ( ii->getCalledFunction() ) {
							definiteCallees[&*bbit].insert( ii->getCalledFunction() );
							possibleCallees[&*bbit].insert( ii->getCalledFunction() );
						} else {
// 							CLOUD9_DEBUG( "Indirect function call at " << ii->getParent()->getParent()->getName().str() << ii->getDebugLoc().getLine() );
						}
						definiteCallers[ii->getCalledFunction()].insert( &*bbit );
						possibleCallers[ii->getCalledFunction()].insert( &*bbit );
					}
					if ( const CallInst *ci = dyn_cast<CallInst>( &*bbit ) ) {
						if ( ci->getCalledFunction() ) {
							definiteCallees[&*bbit].insert( ci->getCalledFunction() );
							possibleCallees[&*bbit].insert( ci->getCalledFunction() );
						} else {
// 							CLOUD9_DEBUG( "Indirect function call at " << ci->getParent()->getParent()->getName().str() << ":" << ci->getDebugLoc().getLine() );
						}
						definiteCallers[ci->getCalledFunction()].insert( &*bbit );
						possibleCallers[ci->getCalledFunction()].insert( &*bbit );
					}
				}
			}
		}

		// Revisit indirect calls.
		for ( std::set<llvm::Instruction *>::iterator iit = definiteCallers[NULL].begin(), iie = definiteCallers[NULL].end(); iit != iie; iit++ ) {
			// Make a list of argument types.
			std::vector<const llvm::Type *> argTypes;
			if ( const InvokeInst *ii = dyn_cast<InvokeInst>( *iit ) ) {
				for ( unsigned i = 0; i < ii->getNumArgOperands(); i++ )
					argTypes.push_back( ii->getArgOperand( i )->getType() );
			}
			if ( const CallInst *ci = dyn_cast<CallInst>( *iit ) )
				for ( unsigned i = 0; i < ci->getNumArgOperands(); i++ )
					argTypes.push_back( ci->getArgOperand( i )->getType() );
			// Look for functions of same type.
			for ( CG::iterator fit = begin(), fie = end(); fit != fie; fit++ ) {
				// Compare argument arity and type.
				if ( argTypes.size() == (*fit)->getFunctionType()->getNumParams() ) {
					unsigned i;
					for ( i = 0; i < argTypes.size(); i++ )
						if ( argTypes[i] != (*fit)->getFunctionType()->getParamType( i ) )
							break;
					if ( i == argTypes.size() ) {
						// Found possible match.
// 						CLOUD9_DEBUG( "Resolving indirect call at " << (*iit)->getParent()->getParent()->getName().str() << ":" << (*iit)->getDebugLoc().getLine() << " to " << (*fit)->getName().str() );
						possibleCallers[*fit].insert( *iit );
						possibleCallees[*iit].insert( *fit );
					}
				}
			}
		}

		// Warn about functions with no callers.
		for ( std::map<llvm::Function *,std::set<llvm::Instruction *> >::iterator it = possibleCallers.begin(), ie = possibleCallers.end(); it != ie; it++ )
			if ( it->second.empty() )
				CLOUD9_INFO( "Found function without any callers: " << it->first->getName().str() );
		// Warn about indirect calls without callees.
		for ( std::map<llvm::Instruction *,std::set<llvm::Function *> >::iterator it = possibleCallees.begin(), ie = possibleCallees.end(); it != ie; it++ )
			if ( it->second.empty() )
				CLOUD9_INFO( "Found call-site without any callees: " << it->first );
	}

	void CG::dump( std::ostream &dotFile ) {
		std::set<std::pair<llvm::Function *, llvm::Function *> > definiteCG;
		std::set<std::pair<llvm::Function *, llvm::Function *> > possibleCG;

		// Find CG edges.
		for ( std::map<llvm::Instruction *,std::set<llvm::Function *> >::iterator it1 = definiteCallees.begin(), ie1 = definiteCallees.end(); it1 != ie1; it1++ )
			for ( std::set<llvm::Function *>::iterator it2 = it1->second.begin(), ie2 = it1->second.end(); it2 != ie2; it2++ )
				definiteCG.insert( std::pair<llvm::Function *, llvm::Function *>( it1->first->getParent()->getParent(), *it2 ) );
		for ( std::map<llvm::Instruction *,std::set<llvm::Function *> >::iterator it1 = possibleCallees.begin(), ie1 = possibleCallees.end(); it1 != ie1; it1++ ) {
			if ( ! it1->second.empty() ) {
				for ( std::set<llvm::Function *>::iterator it2 = it1->second.begin(), ie2 = it1->second.end(); it2 != ie2; it2++ )
					if ( definiteCG.count( std::pair<llvm::Function *, llvm::Function *>( it1->first->getParent()->getParent(), *it2 ) ) == 0 )
						possibleCG.insert( std::pair<llvm::Function *, llvm::Function *>( it1->first->getParent()->getParent(), *it2 ) );
			} else {
				possibleCG.insert( std::pair<llvm::Function *, llvm::Function *>( it1->first->getParent()->getParent(), NULL ) );
			}
		}

		// Generate CG DOT file.
		dotFile<< "digraph CG {" << std::endl;
		// Add all functions.
		for ( iterator it = begin(), ie = end(); it != ie; it++ ) {
			dotFile << "	f" << *it << " [label = \"" << (*it)->getName().str() << "\"];" << std::endl;
		}

		// Definite CG.
		dotFile << "	edge [color = \"blue\"];" << std::endl;
		for ( std::set<std::pair<llvm::Function *, llvm::Function *> >::iterator it = definiteCG.begin(), ie = definiteCG.end(); it != ie; it++ )
			dotFile << "	f" << it->first << " -> f" << it->second << ";" << std::endl;
		dotFile << "	edge [color = \"cyan\"];" << std::endl;
		for ( std::set<std::pair<llvm::Function *, llvm::Function *> >::iterator it = possibleCG.begin(), ie = possibleCG.end(); it != ie; it++ )
			dotFile << "	f" << it->first << " -> f" << it->second << ";" << std::endl;

		dotFile << "}" << std::endl;
	}
}
