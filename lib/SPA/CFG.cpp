/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <sys/stat.h>

#include <sstream>
#include <fstream>

#include "llvm/IR/Instructions.h"

#include "../Core/Common.h"

#include "spa/CG.h"
#include "spa/CFG.h"
#include "spa/Util.h"
#include "spa/InstructionFilter.h"

using namespace llvm;

namespace SPA {
CFG::CFG(Module *module) {
  // Iterate functions.
  for (auto &fn : *module) {
    // Iterate basic blocks.
    for (auto &bb : fn) {
      // Connect across basic blocks.
      // Iterate successors.
      TerminatorInst *ti = bb.getTerminator();
      for (unsigned int i = 0; i < ti->getNumSuccessors(); i++) {
        // Add entry (even if with no successors) for the current predecessor.
        predecessors[&(bb.front())];
        predecessors[&(ti->getSuccessor(i)->front())].insert(ti);
        successors[ti].insert(&(ti->getSuccessor(i)->front()));
      }
      // Connect within basic block.
      // Iterate instructions.
      Instruction *prevInst = NULL;
      for (auto &inst : bb) {
        instructions.insert(&inst);
        functionInstructions[&fn].push_back(&inst);
        if (prevInst) {
          successors[&inst];
          predecessors[&inst].insert(prevInst);
          successors[prevInst].insert(&inst);
        }
        prevInst = &inst;
      }
    }
  }
}

CFG::iterator CFG::begin() { return instructions.begin(); }

CFG::iterator CFG::end() { return instructions.end(); }

const std::vector<llvm::Instruction *> &
CFG::getInstructions(llvm::Function *fn) {
  return functionInstructions[fn];
}

const std::set<Instruction *> &CFG::getSuccessors(Instruction *instruction) {
  return successors[instruction];
}

const std::set<Instruction *> &CFG::getPredecessors(Instruction *instruction) {
  return predecessors[instruction];
}

void CFG::dump(std::ostream &dotFile, CG &cg, InstructionFilter *filter,
               std::map<InstructionFilter *, std::string> &annotations,
               StateUtility *utility, compactness_t compactness) {
  // Generate CFG DOT file.
  dotFile << "digraph CFG {" << std::endl;

  // Add all instructions.
  for (iterator it = begin(), ie = end(); it != ie; it++) {
    if (!filter || filter->checkInstruction(*it)) {
      Instruction *inst = *it;
      std::stringstream attributes;
      // Annotate entry / exit points.
      if (getSuccessors(inst).empty())
        attributes << "shape = \"doublecircle\"";
      else if (inst ==
               &(inst->getParent()->getParent()->getEntryBlock().front()))
        attributes << "shape = \"box\"";
      else
        attributes << "shape = \"oval\"";
      // Annotate source line.
      attributes << " label = \"" << debugLocation(inst) << " | "
                 << utility->getStaticUtility(inst) << "\"";
      // Annotate utility color.
      if (utility)
        attributes << " style=\"filled\" fillcolor = \""
                   << utility->getColor(*this, cg, inst) << "\"";
      // Add user annotations.
      for (std::map<InstructionFilter *, std::string>::iterator
               it = annotations.begin(),
               ie = annotations.end();
           it != ie; it++)
        if (it->first->checkInstruction(inst))
          attributes << " " << it->second;

      dotFile << "	subgraph cluster_"
              << inst->getParent()->getParent()->getName().str() << " {"
              << std::endl << "		label = \""
              << inst->getParent()->getParent()->getName().str() << "\";"
              << std::endl;
      if (compactness == BASICBLOCK)
        dotFile << "		n" << (unsigned long)
            inst->getParent() << " [" << attributes.str() << "];" << std::endl;
      else if (compactness == FUNCTION)
        dotFile << "		n" << (unsigned long) inst->getParent()->getParent()
                << " [" << attributes.str() << "];" << std::endl;
      else // compactness == FULL
        dotFile << "		n" << (unsigned long)
            inst << " [" << attributes.str() << "];" << std::endl;

      dotFile << "	}" << std::endl;
    }
  }
  // Add edges.
  // Successors.
  dotFile << "	edge [color = \"green\"];" << std::endl;
  for (auto it1 : *this) {
    for (auto it2 : getSuccessors(it1)) {
      if (filter && (!filter->checkInstruction(it1)) &&
          (!filter->checkInstruction(it2)))
        continue;
      if (compactness == BASICBLOCK && it1->getParent() == it2->getParent())
        continue;
      if (compactness == FUNCTION &&
          it1->getParent()->getParent() == it2->getParent()->getParent())
        continue;

      if (compactness == BASICBLOCK)
        dotFile << "	n" << ((unsigned long) it1->getParent()) << " -> n"
                << ((unsigned long) it2->getParent()) << ";" << std::endl;
      else if (compactness == FULL)
        dotFile << "	n" << ((unsigned long) it1) << " -> n"
                << ((unsigned long) it2) << ";" << std::endl;
    }
  }
  // Callers.
  dotFile << "	edge [color = \"blue\"];" << std::endl;
  for (auto it1 : cg) {
    llvm::Function *fn = it1;
    for (auto it2 : cg.getDefiniteCallers(fn)) {
      if (!filter || filter->checkInstruction(it2)) {
        if (fn == NULL) {
          dotFile << "	IndirectFunction [label = \"*\" shape = \"box\"]"
                  << std::endl;
          if (compactness == BASICBLOCK)
            dotFile << "	n" << (unsigned long)
                it2->getParent() << " -> IndirectFunction;" << std::endl;
          else if (compactness == FUNCTION)
            dotFile << "	n" << (unsigned long) it2->getParent()->getParent()
                    << " -> IndirectFunction;" << std::endl;
          else // compactness == FULL
            dotFile << "	n" << (unsigned long)
                it2 << " -> IndirectFunction;" << std::endl;
        } else if (!fn->empty()) {
          if (compactness == BASICBLOCK)
            dotFile << "	n" << ((unsigned long) it2->getParent()) << " -> n"
                    << ((unsigned long) & (fn->getEntryBlock())) << ";"
                    << std::endl;
          else if (compactness == FUNCTION)
            dotFile << "	n" << ((unsigned long) it2->getParent()->getParent())
                    << " -> n" << ((unsigned long) fn) << ";" << std::endl;
          else // compactness == FULL
            dotFile << "	n" << ((unsigned long) it2) << " -> n"
                    << ((unsigned long) & (fn->getEntryBlock().front())) << ";"
                    << std::endl;
        } else {
          dotFile << "	n" << ((unsigned long) fn) << " [label = \""
                  << fn->getName().str() << "\" shape = \"box\"]" << std::endl;
          if (compactness == BASICBLOCK)
            dotFile << "	n" << (unsigned long) it2->getParent() << " -> n"
                    << ((unsigned long) fn) << ";" << std::endl;
          else if (compactness == FUNCTION)
            dotFile << "	n" << (unsigned long) it2->getParent()->getParent()
                    << " -> n" << ((unsigned long) fn) << ";" << std::endl;
          else // compactness == FULL
            dotFile << "	n" << (unsigned long)
                it2 << " -> n" << ((unsigned long) fn) << ";" << std::endl;
        }
      }
    }
  }
  dotFile << "	edge [color = \"cyan\"];" << std::endl;
  for (CG::iterator it1 = cg.begin(), ie1 = cg.end(); it1 != ie1; it1++) {
    llvm::Function *fn = *it1;
    for (auto it2 : cg.getPossibleCallers(fn)) {
      if (!filter || filter->checkInstruction(it2)) {
        if (fn == NULL) {
          dotFile << "	IndirectFunction [label = \"*\" shape = \"box\"]"
                  << std::endl;
          if (compactness == BASICBLOCK)
            dotFile << "	n" << (unsigned long)
                it2->getParent() << " -> IndirectFunction;" << std::endl;
          if (compactness == FUNCTION)
            dotFile << "	n" << (unsigned long) it2->getParent()->getParent()
                    << " -> IndirectFunction;" << std::endl;
          else // compactness == FULL
            dotFile << "	n" << (unsigned long)
                it2 << " -> IndirectFunction;" << std::endl;
        } else if (!fn->empty()) {
          if (compactness == BASICBLOCK)
            dotFile << "	n" << ((unsigned long) it2->getParent()) << " -> n"
                    << ((unsigned long) & (fn->getEntryBlock())) << ";"
                    << std::endl;
          else if (compactness == FUNCTION)
            dotFile << "	n" << ((unsigned long) it2->getParent()->getParent())
                    << " -> n" << ((unsigned long) fn) << ";" << std::endl;
          else // compactness == FULL
            dotFile << "	n" << ((unsigned long) it2) << " -> n"
                    << ((unsigned long) & (fn->getEntryBlock().front())) << ";"
                    << std::endl;
        } else {
          dotFile << "	n" << ((unsigned long) fn) << " [label = \""
                  << fn->getName().str() << "\" shape = \"box\"]" << std::endl;
          if (compactness == BASICBLOCK)
            dotFile << "	n" << (unsigned long) it2->getParent() << " -> n"
                    << ((unsigned long) fn) << ";" << std::endl;
          if (compactness == FUNCTION)
            dotFile << "	n" << (unsigned long) it2->getParent()->getParent()
                    << " -> n" << ((unsigned long) fn) << ";" << std::endl;
          else // compactness == FULL
            dotFile << "	n" << (unsigned long)
                it2 << " -> n" << ((unsigned long) fn) << ";" << std::endl;
        }
      }
    }
  }

  dotFile << "}" << std::endl;
}

class FunctionInstructionFilter : public InstructionFilter {
private:
  llvm::Function *function;

public:
  FunctionInstructionFilter(llvm::Function *fn) : function(fn) {}

  bool checkStep(llvm::Instruction *preInstruction,
                 llvm::Instruction *postInstruction) {
    return checkInstruction(preInstruction) &&
           checkInstruction(postInstruction);
  }

  bool checkInstruction(llvm::Instruction *instruction) {
    return instruction->getParent()->getParent() == function;
  }
};

void CFG::dumpDir(std::string directory, CG &cg,
                  std::map<InstructionFilter *, std::string> &annotations,
                  StateUtility *utility) {
  auto result = mkdir(directory.c_str(), 0755);
  assert(result == 0 || errno == EEXIST);

  std::ofstream makefile(directory + "/Makefile");
  assert(makefile.good());
  makefile << "%.pdf: %.dot" << std::endl;
  makefile << "\tdot -Tpdf -o $@ $<" << std::endl << std::endl;

  for (auto it = cg.begin(), ie = cg.end(); it != ie; it++) {
    klee::klee_message("Generating %s.dot", (*it)->getName().str().c_str());

    std::ofstream dotFile(directory + "/" + (*it)->getName().str() + ".dot");
    assert(dotFile.good());
    FunctionInstructionFilter fif(*it);
    dump(dotFile, cg, &fif, annotations, utility, FULL);

    makefile << "default: " << (*it)->getName().str() << ".pdf" << std::endl;
  }
}
}
