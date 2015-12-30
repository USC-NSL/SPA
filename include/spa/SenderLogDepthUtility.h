/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __SenderLogDepthUtility_H__
#define __SenderLogDepthUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
class SenderLogDepthUtility : public StateUtility {
public:
  SenderLogDepthUtility() {}
  double getUtility(klee::ExecutionState *state) {
    assert(state);

    if (state->senderPath) {
      return (double) state->senderPath->getSymbolLog().size();
    } else {
      return UTILITY_PROCESS_FIRST;
    }
  }
};
}

#endif // #ifndef __SenderLogDepthUtility_H__
