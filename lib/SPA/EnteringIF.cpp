/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "spa/EnteringIF.h"

#include "llvm/IR/Instructions.h"
#include <../Core/Common.h>
#include <klee/Internal/Module/KInstruction.h>

namespace SPA {
EnteringIF::EnteringIF(CFG &cfg, CG &cg, InstructionFilter *filter) {
  for (auto inst : cfg) {
    if (filter->checkInstruction(inst)) {
      std::set<llvm::Instruction *> preds;
      if (inst == &(inst->getParent()->getParent()->getEntryBlock().front())) {
        auto c = cg.getPossibleCallers(inst->getParent()->getParent());
        preds.insert(c.begin(), c.end());
      } else {
        auto p = cfg.getPredecessors(inst);
        preds.insert(p.begin(), p.end());
      }

      bool isEntering =
          preds.empty() || !filter->checkInstruction(*preds.begin());
      for (auto p : preds) {
        assert(isEntering == !filter->checkInstruction(p) &&
               "Entry point in the middle of a segment.");
      }
      if (isEntering) {
        whitelist.insert(inst);
      }
    }
  }
}
}
