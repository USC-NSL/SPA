/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __SymbolLogDepthUtility_H__
#define __SymbolLogDepthUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
class SymbolLogDepthUtility : public StateUtility {
public:
  SymbolLogDepthUtility() {}
  double getUtility(klee::ExecutionState *state) {
    assert(state);

    long logDepth = 0;

    for (auto it : state->symbolics) {
      std::string name = it.second->name;
      if (name.compare(0, strlen(SPA_INPUT_PREFIX), SPA_INPUT_PREFIX) == 0 ||
          name.compare(0, strlen(SPA_OUTPUT_PREFIX), SPA_OUTPUT_PREFIX) == 0) {
        logDepth++;
      }
    }

    return (double) logDepth;
  }
};
}

#endif // #ifndef __SymbolLogDepthUtility_H__
