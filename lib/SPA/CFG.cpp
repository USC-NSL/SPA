/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <sys/stat.h>

#include <sstream>
#include <fstream>

#include "llvm/IR/Instructions.h"
#include <llvm/Support/raw_ostream.h>

#include "../Core/Common.h"

#include "spa/CG.h"
#include "spa/CFG.h"
#include "spa/Util.h"
#include "spa/InstructionFilter.h"

using namespace llvm;

namespace SPA {
void CFG::init() {
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

void CFG::dump(std::ostream &dotFile, llvm::Function *fn, CG &cg,
               std::map<InstructionFilter *, std::string> &annotations,
               StateUtility *utility) {
  if (!initialized) {
    init();
  }
  // Generate CFG DOT file.
  dotFile << "digraph CFG {" << std::endl;

  std::string signature;
  llvm::raw_string_ostream ss(signature);
  fn->getFunctionType()->print(ss);
  ss.flush();
  std::replace(signature.begin(), signature.end(), '\"', '\'');
  dotFile << "	label = \"" << fn->getName().str() << ": " << signature << "\""
          << std::endl;
  dotFile << "	labelloc = \"t\"" << std::endl;

  unsigned long entryRef =
      fn->empty() ? (unsigned long) fn
                  : (unsigned long) & fn->getEntryBlock().front();
  // Add edges to definite callers.
  dotFile << "  edge [color = \"blue\"];" << std::endl;
  for (auto caller : cg.getDefiniteCallers(fn)) {
    dotFile << "  n" << ((unsigned long) caller) << " -> n" << entryRef << ";"
            << std::endl;
    dotFile << "  n" << ((unsigned long) caller) << " [label = \""
            << caller->getParent()->getParent()->getName().str() << "\\n"
            << debugLocation(caller) << "\" shape = \"invhouse\" href=\""
            << caller->getParent()->getParent()->getName().str() << ".html\"]"
            << std::endl;
  }
  // Add edges to possible callers.
  dotFile << "  edge [color = \"cyan\"];" << std::endl;
  for (auto caller : cg.getPossibleCallers(fn)) {
    if (!cg.getDefiniteCallers(fn).count(caller)) {
      dotFile << "  n" << ((unsigned long) caller) << " -> n" << entryRef << ";"
              << std::endl;
      dotFile << "  n" << ((unsigned long) caller) << " [label = \""
              << caller->getParent()->getParent()->getName().str() << "\\n"
              << debugLocation(caller) << "\" shape = \"invhouse\" href=\""
              << caller->getParent()->getParent()->getName().str() << ".html\"]"
              << std::endl;
    }
  }

  // Add all instructions in the function.
  for (auto &bb : *fn) {
    for (auto &inst : bb) {
      std::stringstream attributes;
      // Annotate entry / exit points.
      if (returns(&inst)) {
        attributes << "shape = \"doublecircle\"";
      } else if (&inst ==
                 &(inst.getParent()->getParent()->getEntryBlock().front())) {
        attributes << "shape = \"box\"";
      } else {
        attributes << "shape = \"circle\"";
      }
      // Annotate source line and instruction print-out.
      if (utility) {
        attributes << " label = \"" << utility->getStaticUtility(&inst) << "\"";
      }
      attributes << " tooltip = \"" << debugLocation(&inst) << "&#10;"
                 << inst.getOpcodeName() << "\"";
      // Annotate utility color.
      if (utility) {
        attributes << " style=\"filled\" fillcolor = \""
                   << utility->getColor(*this, cg, &inst) << "\"";
      }
      // Add user annotations.
      for (auto annotation : annotations) {
        if (annotation.first->checkInstruction(&inst)) {
          attributes << " " << annotation.second;
        }
      }
      dotFile << "	n" << ((unsigned long) & inst) << " [" << attributes.str()
              << "];" << std::endl;
      // Add edges to successors.
      dotFile << "	edge [color = \"black\"];" << std::endl;
      for (auto successor : getSuccessors(&inst)) {
        dotFile << "	n" << ((unsigned long) & inst) << " -> n"
                << ((unsigned long) successor) << ";" << std::endl;
      }
      // Add edges to definite callees.
      dotFile << "	edge [color = \"blue\"];" << std::endl;
      for (auto callee : cg.getDefiniteCallees(&inst)) {
        if (callee) {
          dotFile << "	n" << ((unsigned long) & inst) << " -> n"
                  << ((unsigned long) callee) << ";" << std::endl;
          dotFile << "	n" << ((unsigned long) callee) << " [label = \""
                  << callee->getName().str() << "\" shape = \"folder\" href=\""
                  << callee->getName().str() << ".html\"]" << std::endl;
        } else {
          dotFile << "	IndirectFunction [label = \"*\" shape = \"folder\"]"
                  << std::endl;
          dotFile << "	n" << ((unsigned long) & inst) << " -> IndirectFunction;"
                  << std::endl;
        }
      }
      // Add edges to possible callees.
      dotFile << "	edge [color = \"cyan\"];" << std::endl;
      for (auto callee : cg.getPossibleCallees(&inst)) {
        if (!cg.getDefiniteCallees(&inst).count(callee)) {
          if (callee) {
            dotFile << "	n" << ((unsigned long) callee) << " [label = \""
                    << callee->getName().str()
                    << "\" shape = \"folder\" href=\""
                    << callee->getName().str() << ".html\"]" << std::endl;
            dotFile << "	n" << ((unsigned long) & inst) << " -> n"
                    << ((unsigned long) callee) << ";" << std::endl;
          } else {
            dotFile << "	IndirectFunction [label = \"*\" shape = \"folder\"]"
                    << std::endl;
            dotFile << "	n" << ((unsigned long) & inst)
                    << " -> IndirectFunction;" << std::endl;
          }
        }
      }
    }
  }

  dotFile << "}" << std::endl;
}

void CFG::dumpDir(std::string directory, CG &cg,
                  std::map<InstructionFilter *, std::string> &annotations,
                  StateUtility *utility) {
  if (!initialized) {
    init();
  }
  auto result = mkdir(directory.c_str(), 0755);
  assert(result == 0 || errno == EEXIST);

  std::ofstream makefile(directory + "/Makefile");
  assert(makefile.good());
  makefile << "%.pdf: %.dot" << std::endl;
  makefile << "\tdot -Tpdf -o $@ $<" << std::endl << std::endl;

  makefile << "%.svg: %.dot" << std::endl;
  makefile << "\tdot -Tsvg -o $@ $<" << std::endl << std::endl;

  makefile << "%.cmapx: %.dot" << std::endl;
  makefile << "\tdot -Tcmapx -o $@ $<" << std::endl << std::endl;

  makefile << "%.html: %.cmapx" << std::endl;
  makefile
      << "\techo \"<html><img src='$(@:.html=.svg)' usemap='#CFG' />\" > $@"
      << std::endl;
  makefile << "\tcat $< >> $@" << std::endl;
  makefile << "\techo '</html>' >> $@" << std::endl << std::endl;

  makefile << "%.clean:" << std::endl;
  makefile << "\trm -f $(@:.clean=.html) $(@:.clean=.svg) $(@:.clean=.cmapx)"
           << std::endl << std::endl;

  makefile << "default: all" << std::endl;

  for (auto &fn : cg) {
    klee::klee_message("Generating %s.dot", fn.getName().str().c_str());

    std::ofstream dotFile(directory + "/" + fn.getName().str() + ".dot");
    assert(dotFile.good());
    dump(dotFile, &fn, cg, annotations, utility);

    makefile << "all: " << fn.getName().str() << std::endl;
    makefile << fn.getName().str() << ": " << fn.getName().str() << ".html "
             << fn.getName().str() << ".svg" << std::endl;
    makefile << "clean: " << fn.getName().str() << ".clean" << std::endl;
  }
}
}
