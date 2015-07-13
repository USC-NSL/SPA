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

// Blacklist of functions that can be assumed to never be indirectly called.
std::set<std::string> indirectCallBlacklist = {
  "__spa_output", "__spa_tag", "spa_api_entry", "spa_api_input_handler",
  "spa_api_output_handler", "spa_checkpoint", "spa_input",
  "spa_invalid_path_handler", "spa_message_handler_entry",
  "spa_msg_input_handler", "spa_msg_input_point", "spa_msg_input_size_handler",
  "spa_msg_output_handler", "spa_msg_output_point", "spa_return",
  "spa_runtime_call", "spa_seed", "spa_seed_symbol", "spa_seed_symbol_check",
  "spa_state_handler", "spa_tag_handler", "spa_valid_path_handler",
  "spa_valid_path_point", "spa_waypoint"
};

namespace SPA {
CG::CG(llvm::Module *module) {
  // Iterate functions.
  for (auto &mit : *module) {
    functions.insert(&mit);
    // Iterate basic blocks.
    for (auto &fit : mit) {
      // Iterate instructions.
      for (auto &bbit : fit) {
        llvm::Function *calledFunction = NULL;
        // Check for CallInst or InvokeInst.
        if (llvm::InvokeInst *ii = llvm::dyn_cast<llvm::InvokeInst>(&bbit)) {
          calledFunction = ii->getCalledFunction();
        } else if (llvm::CallInst *ci = llvm::dyn_cast<llvm::CallInst>(&bbit)) {
          if (!ci->isInlineAsm()) {
            calledFunction = ci->getCalledFunction();
          }
        }
        // Check if function is called indirectly through a constant pointer.
        if (calledFunction == NULL) {
          llvm::Value *calledValue = NULL;
          // Check for CallInst or InvokeInst.
          if (llvm::InvokeInst *ii = llvm::dyn_cast<llvm::InvokeInst>(&bbit)) {
            calledValue = ii->getCalledValue();
          } else if (llvm::CallInst *ci =
                         llvm::dyn_cast<llvm::CallInst>(&bbit)) {
            if (!ci->isInlineAsm()) {
              calledValue = ci->getCalledValue();
            }
          }
          if (calledValue) {
            calledFunction = llvm::dyn_cast<llvm::Function>(
                calledValue->stripPointerCasts());
          }
        }
        if (calledFunction) {
          definiteCallees[&bbit].insert(calledFunction);
          possibleCallees[&bbit].insert(calledFunction);
        }
        definiteCallers[calledFunction].insert(&bbit);
        possibleCallers[calledFunction].insert(&bbit);
      }
    }
  }

  // Revisit indirect calls.
  for (auto iit : definiteCallers[NULL]) {
    // Make a list of argument types.
    std::vector<const llvm::Type *> argTypes;
    if (llvm::InvokeInst *ii = llvm::dyn_cast<llvm::InvokeInst>(iit)) {
      for (unsigned i = 0; i < ii->getNumArgOperands(); i++)
        argTypes.push_back(ii->getArgOperand(i)->getType());
    }
    if (llvm::CallInst *ci = llvm::dyn_cast<llvm::CallInst>(iit))
      for (unsigned i = 0; i < ci->getNumArgOperands(); i++)
        argTypes.push_back(ci->getArgOperand(i)->getType());
    // Look for functions of same type.
    for (auto fit : *this) {
      // Ignore blacklisted functions.
      if (indirectCallBlacklist.count(fit->getName())) {
        continue;
      }
      // Compare argument arity and type.
      if (argTypes.size() == fit->getFunctionType()->getNumParams()) {
        unsigned i;
        for (i = 0; i < argTypes.size(); i++)
          if (argTypes[i] != fit->getFunctionType()->getParamType(i))
            break;
        if (i == argTypes.size()) {
          // Found possible match.
          // 						klee_message( "Resolving indirect call at " <<
          // (iit)->getParent()->getParent()->getName().str() << ":" <<
          // (iit)->getDebugLoc().getLine() << " to " <<
          // (*fit)->getName().str() );
          possibleCallers[fit].insert(iit);
          possibleCallees[iit].insert(fit);
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
      definiteCG.insert(
          std::make_pair(it1.first->getParent()->getParent(), it2));
  for (auto it1 : possibleCallees) {
    if (!it1.second.empty()) {
      for (auto it2 : it1.second)
        if (definiteCG.count(std::make_pair(it1.first->getParent()->getParent(),
                                            it2)) == 0) {
          possibleCG.insert(
              std::make_pair(it1.first->getParent()->getParent(), it2));
        }
    } else {
      possibleCG.insert(std::make_pair(it1.first->getParent()->getParent(),
                                       (llvm::Function *)NULL));
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
