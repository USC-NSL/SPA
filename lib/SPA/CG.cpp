/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/Util.h"
#include "spa/CG.h"

#include <vector>

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/DebugInfo.h"
#include "../Core/Common.h"

using namespace llvm;

namespace SPA {
CG::CG(llvm::Module *module) {
  // Iterate functions.
  for (auto &mit : *module) {
    functions.insert(&mit);
    // Iterate basic blocks.
    for (auto &fit : mit) {
      // Iterate instructions.
      for (auto &bbit : fit) {
        // Check for CallInst or InvokeInst.
        if (const InvokeInst *ii = dyn_cast<InvokeInst>(&bbit)) {
          if (ii->getCalledFunction()) {
            definiteCallees[&bbit].insert(ii->getCalledFunction());
            possibleCallees[&bbit].insert(ii->getCalledFunction());
          } else {
            // 							klee_message( "Indirect function call at " <<
            // ii->getParent()->getParent()->getName().str() <<
            // ii->getDebugLoc().getLine() );
          }
          definiteCallers[ii->getCalledFunction()].insert(&bbit);
          possibleCallers[ii->getCalledFunction()].insert(&bbit);
        }
        if (const CallInst *ci = dyn_cast<CallInst>(&bbit)) {
          if (!ci->isInlineAsm()) {
            if (ci->getCalledFunction()) {
              definiteCallees[&bbit].insert(ci->getCalledFunction());
              possibleCallees[&bbit].insert(ci->getCalledFunction());
            } else {
              // 							klee_message( "Indirect function call at " <<
              // ci->getParent()->getParent()->getName().str() << ":" <<
              // ci->getDebugLoc().getLine() );
            }
            definiteCallers[ci->getCalledFunction()].insert(&bbit);
            possibleCallers[ci->getCalledFunction()].insert(&bbit);
          }
        }
      }
    }
  }

  // Revisit indirect calls.
  for (auto iit : definiteCallers[NULL]) {
    // Make a list of argument types.
    std::vector<const llvm::Type *> argTypes;
    if (const InvokeInst *ii = dyn_cast<InvokeInst>(iit)) {
      for (unsigned i = 0; i < ii->getNumArgOperands(); i++)
        argTypes.push_back(ii->getArgOperand(i)->getType());
    }
    if (const CallInst *ci = dyn_cast<CallInst>(iit))
      for (unsigned i = 0; i < ci->getNumArgOperands(); i++)
        argTypes.push_back(ci->getArgOperand(i)->getType());
    // Look for functions of same type.
    for (CG::iterator fit = begin(), fie = end(); fit != fie; fit++) {
      // Compare argument arity and type.
      if (argTypes.size() == (*fit)->getFunctionType()->getNumParams()) {
        unsigned i;
        for (i = 0; i < argTypes.size(); i++)
          if (argTypes[i] != (*fit)->getFunctionType()->getParamType(i))
            break;
        if (i == argTypes.size()) {
          // Found possible match.
          // 						klee_message( "Resolving indirect call at " <<
          // (iit)->getParent()->getParent()->getName().str() << ":" <<
          // (iit)->getDebugLoc().getLine() << " to " <<
          // (*fit)->getName().str() );
          possibleCallers[*fit].insert(iit);
          possibleCallees[iit].insert(*fit);
        }
      }
    }
  }

  // Warn about functions with no callers.
  for (auto it : possibleCallers)
    if (it.second.empty())
      klee::klee_message("Found function without any callers: %s",
                         it.first->getName().str().c_str());
  // Warn about indirect calls without callees.
  for (auto it : possibleCallees)
    if (it.second.empty())
      klee::klee_message("Found call-site without any callees: %s",
                         debugLocation(it.first).c_str());
}

void CG::dump(std::ostream &dotFile) {
  std::set<std::pair<llvm::Function *, llvm::Function *> > definiteCG;
  std::set<std::pair<llvm::Function *, llvm::Function *> > possibleCG;

  // Find CG edges.
  for (auto it1 : definiteCallees)
    for (auto it2 : it1.second)
      definiteCG.insert(std::pair<llvm::Function *, llvm::Function *>(
          it1.first->getParent()->getParent(), it2));
  for (auto it1 : possibleCallees) {
    if (!it1.second.empty()) {
      for (auto it2 : it1.second)
        if (definiteCG.count(std::pair<llvm::Function *, llvm::Function *>(
                it1.first->getParent()->getParent(), it2)) == 0) {
          possibleCG.insert(std::pair<llvm::Function *, llvm::Function *>(
              it1.first->getParent()->getParent(), it2));
        }
    } else {
      possibleCG.insert(std::pair<llvm::Function *, llvm::Function *>(
          it1.first->getParent()->getParent(), NULL));
    }
  }

  // Generate CG DOT file.
  dotFile << "digraph CG {" << std::endl;
  // Add all functions.
  for (auto it : *this) {
    dotFile << "	f" << it << " [label = \"" << it->getName().str() << "\"];"
            << std::endl;
  }

  // Definite CG.
  dotFile << "	edge [color = \"blue\"];" << std::endl;
  for (auto it : definiteCG)
    dotFile << "	f" << it.first << " -> f" << it.second << ";" << std::endl;
  dotFile << "	edge [color = \"cyan\"];" << std::endl;
  for (auto it : possibleCG)
    dotFile << "	f" << it.first << " -> f" << it.second << ";" << std::endl;

  dotFile << "}" << std::endl;
}
}
