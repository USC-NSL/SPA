/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __CFG_H__
#define __CFG_H__

#include <map>
#include <set>
#include <ostream>

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"

#include "spa/CG.h"
#include "spa/StateUtility.h"

namespace SPA {
class InstructionFilter;

class CFG {
private:
  bool initialized = false;
  llvm::Module *module;
  std::set<llvm::Instruction *> instructions;
  std::map<llvm::Function *, std::vector<llvm::Instruction *> >
      functionInstructions;
  std::map<llvm::Instruction *, std::set<llvm::Instruction *> > predecessors,
      successors;

  void init();

public:
  typedef std::set<llvm::Instruction *>::iterator iterator;

  CFG(llvm::Module *module) : module(module) {}
  // Iterates over all instructions in an arbitrary order.
  iterator begin() {
    if (!initialized) {
      init();
    }
    return instructions.begin();
  }
  iterator end() {
    if (!initialized) {
      init();
    }
    return instructions.end();
  }
  // Gets the instructions of a function.
  const std::vector<llvm::Instruction *> &getInstructions(llvm::Function *fn) {
    if (!initialized) {
      init();
    }
    return functionInstructions[fn];
  }
  // Gets CFG data.
  const std::set<llvm::Instruction *> &
  getSuccessors(llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    return successors[instruction];
  }
  const std::set<llvm::Instruction *> &
  getPredecessors(llvm::Instruction *instruction) {
    if (!initialized) {
      init();
    }
    return predecessors[instruction];
  }
  bool calls(llvm::Instruction *inst) {
    return isa<llvm::CallInst>(inst) || isa<llvm::InvokeInst>(inst);
  }
  bool returns(llvm::Instruction *inst) {
    return isa<llvm::ReturnInst>(inst) || isa<llvm::ResumeInst>(inst);
  }
  bool terminates(llvm::Instruction *inst) {
    return getSuccessors(inst).empty() && !returns(inst);
  }
  // Dumps CFG as a GraphViz DOT-file.
  void dump(std::ostream &dotFile, llvm::Function *fn, SPA::CG &cg,
            std::map<InstructionFilter *, std::string> &annotations,
            StateUtility *utility);
  // Dumps CFG as a browseable directory with GraphViz DOT-files.
  void dumpDir(std::string directory, SPA::CG &cg,
               std::map<InstructionFilter *, std::string> &annotations,
               StateUtility *utility);
};
}

#endif // #ifndef __CFG_H__
