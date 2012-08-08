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
	CG::CG( CFG &cfg ) {
		// Iterate instructions.
		for ( CFG::iterator it = cfg.begin(), ie = cfg.end(); it != ie; it++ ) {
			// Check for CallInst or InvokeInst.
			if ( const InvokeInst *ii = dyn_cast<InvokeInst>( *it ) ) {
				if ( ii->getCalledFunction() ) {
					functions.insert( ii->getCalledFunction() );
					definiteCallees[*it].insert( ii->getCalledFunction() );
					possibleCallees[*it].insert( ii->getCalledFunction() );
				} else {
// 					CLOUD9_INFO( "Indirect function call at " << (*it)->getParent()->getParent()->getName().str() << (*it)->getDebugLoc().getLine() );
				}
				definiteCallers[ii->getCalledFunction()].insert( *it );
				possibleCallers[ii->getCalledFunction()].insert( *it );
			}
			if ( const CallInst *ci = dyn_cast<CallInst>( *it ) ) {
				if ( ci->getCalledFunction() ) {
					functions.insert( ci->getCalledFunction() );
					definiteCallees[*it].insert( ci->getCalledFunction() );
					possibleCallees[*it].insert( ci->getCalledFunction() );
				} else {
// 					CLOUD9_INFO( "Indirect function call at " << (*it)->getParent()->getParent()->getName().str() << ":" << (*it)->getDebugLoc().getLine() );
				}
				definiteCallers[ci->getCalledFunction()].insert( *it );
				possibleCallers[ci->getCalledFunction()].insert( *it );
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
			if ( const CallInst *ci = dyn_cast<CallInst>( *iit ) ) {
				for ( unsigned i = 0; i < ci->getNumArgOperands(); i++ )
					argTypes.push_back( ci->getArgOperand( i )->getType() );
			}
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

		// Generate CFG DOT file.
		dotFile<< "digraph CG {" << std::endl;
		// Add all functions.
		for ( iterator it = begin(), ie = end(); it != ie; it++ ) {
			dotFile << "	f" << *it << " [label = \"" << (*it)->getName().str() << "\"];";
		}

		// Definite CG.
		dotFile << "	edge [color = \"blue\"];" << std::endl;
		for ( std::set<std::pair<llvm::Function *, llvm::Function *> >::iterator it = definiteCG.begin(), ie = definiteCG.end(); it != ie; it++ )
			dotFile << "	f" << it->first << " -> f" << it->second << ";";
		dotFile << "	edge [color = \"cyan\"];" << std::endl;
		for ( std::set<std::pair<llvm::Function *, llvm::Function *> >::iterator it = possibleCG.begin(), ie = possibleCG.end(); it != ie; it++ )
			dotFile << "	f" << it->first << " -> f" << it->second << ";";

		dotFile << "}" << std::endl;
	}
}
