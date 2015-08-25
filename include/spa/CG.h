/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CG_H__
#define __CG_H__

#include <map>
#include <set>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

namespace SPA {
class CG {
private:
  bool initialized = false;
  llvm::Module *module;
  std::map<llvm::Function *, std::set<llvm::Instruction *> > definiteCallers;
  std::map<llvm::Function *, std::set<llvm::Instruction *> > possibleCallers;
  std::map<llvm::Instruction *, std::set<llvm::Function *> > definiteCallees;
  std::map<llvm::Instruction *, std::set<llvm::Function *> > possibleCallees;

  void init();

public:
  typedef llvm::Module::iterator iterator;

  CG(llvm::Module *module) : module(module) {}
  iterator begin() { return module->begin(); }
  iterator end() { return module->end(); }
  const std::set<llvm::Instruction *> &
  getDefiniteCallers(llvm::Function *function) {
    if (!initialized) {
      init();
    }
    return definiteCallers[function];
  }
  const std::set<llvm::Instruction *> &
  getPossibleCallers(llvm::Function *function) {
    if (!initialized) {
      init();
    }
    return possibleCallers[function];
  }
  const std::set<llvm::Function *> &
  getDefiniteCallees(llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    return definiteCallees[instruction];
  }
  const std::set<llvm::Function *> &
  getPossibleCallees(llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    return possibleCallees[instruction];
  }
  // Dumps CG as a GraphViz DOT-file.
  void dump(std::ostream &dotFile);
};
}

#endif // #ifndef __CG_H__
