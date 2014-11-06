/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __StateUtility_H__
#define __StateUtility_H__

#include <cfloat>

#include "llvm/IR/Instruction.h"

#include "klee/ExecutionState.h"

#define UTILITY_PROCESS_FIRST DBL_MAX
#define UTILITY_PROCESS_LAST -INFINITY
#define UTILITY_FILTER_OUT INFINITY
#define UTILITY_DEFAULT 0

namespace SPA {
class CFG;
class CG;

class StateUtility {
public:
  /**
   * Provides a utility metric to a given execution state, allowing some states
   * to be explored earlier by priority.
   *
   * @param state The state to check.
   * @return The utility metric for the given state (higher = more useful).
   */
  virtual double getUtility(klee::ExecutionState *state) = 0;

  /**
   * Provides a utility metric to a given instruction (used for static
   * debugging).
   * 
   * @param instruction The instruction to check.
   * @return The utility metric for the given instruction (higher = more
   * useful).
   */
  virtual double getStaticUtility(llvm::Instruction *instruction) { return 0; }

  /**
   * Gives a Graphviz color for a given node to aid in debugging.
   */
  virtual std::string getColor(CFG &cfg, CG &cg,
                               llvm::Instruction *instruction) {
    return "white";
  }

protected:
  virtual ~StateUtility() {}
};
}

#endif // #ifndef __StateUtility_H__
