/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __FilteringEventHandler_H__
#define __FilteringEventHandler_H__

#include "klee/ExecutionState.h"

namespace SPA {
class FilteringEventHandler {
public:
  /**
   * Notifies the handler that a state has been filtered out and will not be
   * explored further.
   *
   * @param state The filtered state.
   * @param id The id (index) of the state utility that caused the filtering.
   */
  virtual void onStateFiltered(klee::ExecutionState *state,
                               unsigned int id) = 0;

protected:
  virtual ~FilteringEventHandler() {}
};
}

#endif // #ifndef __FilteringEventHandler_H__
