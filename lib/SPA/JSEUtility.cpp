/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/JSEUtility.h>

#include <spa/SPA.h>
#include <spa/PathLoader.h>

namespace SPA {
extern PathLoader *senderPaths;

double JSEUtility::getUtility(klee::ExecutionState *state) {
  assert(state);

  // Find teh sender path ID for the current state.
  int64_t pathID = -1;
  for (auto it : state->constraints) {
    if (klee::EqExpr *eqExpr = llvm::dyn_cast<klee::EqExpr>(it)) {
      if (klee::ConcatExpr *catExpr =
              llvm::dyn_cast<klee::ConcatExpr>(eqExpr->right)) {
        if (klee::ReadExpr *rdExpr =
                llvm::dyn_cast<klee::ReadExpr>(catExpr->getLeft())) {
          if (rdExpr->updates.root->name == SPA_PATHID_VARIABLE) {
            if (klee::ConstantExpr *constExpr =
                    llvm::dyn_cast<klee::ConstantExpr>(eqExpr->left)) {
              pathID = constExpr->getLimitedValue();
              break;
            }
          }
        }
      }
    }
  }

  // If loading path would block, load it last.
  if (pathID >= 0) {
    senderPaths->restart();
    if (senderPaths->skipPaths(pathID + 1)) {
      return UTILITY_DEFAULT;
    } else {
      return UTILITY_PROCESS_LAST;
    }
  } else {
    return UTILITY_PROCESS_LAST;
  }
}
}
