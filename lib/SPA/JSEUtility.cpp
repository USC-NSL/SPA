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

  // Find the sender path ID for the current state.
  int64_t pathID = -1, maxChoice = -1;
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
      } else if (eqExpr->left->getWidth() == klee::Expr::Bool &&
                 eqExpr->left->isFalse()) {
        if (klee::EqExpr *neqExpr =
                llvm::dyn_cast<klee::EqExpr>(eqExpr->right)) {
          if (klee::ConcatExpr *catExpr =
                  llvm::dyn_cast<klee::ConcatExpr>(neqExpr->right)) {
            if (klee::ReadExpr *rdExpr =
                    llvm::dyn_cast<klee::ReadExpr>(catExpr->getLeft())) {
              if (rdExpr->updates.root->name == SPA_PATHID_VARIABLE) {
                if (klee::ConstantExpr *constExpr =
                        llvm::dyn_cast<klee::ConstantExpr>(neqExpr->left)) {
                  int64_t choice = constExpr->getLimitedValue();
                  if (choice > maxChoice) {
                    maxChoice = choice;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // If loading path would block, load it last.
  if (pathID >= 0) {
    if (senderPaths->gotoPath(pathID) && senderPaths->skipPath()) {
      return UTILITY_DEFAULT;
    } else {
      return UTILITY_PROCESS_LAST;
    }
  } else {
    if (maxChoice >= 0) {
      if (senderPaths->gotoPath(maxChoice) && senderPaths->skipPath()) {
        return UTILITY_DEFAULT;
      } else {
        return UTILITY_PROCESS_LAST;
      }
    } else {
      return UTILITY_DEFAULT;
    }
  }
}
}
