/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __JSEUtility_H__
#define __JSEUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
class JSEUtility : public StateUtility {
public:
  double getUtility(klee::ExecutionState *state);

protected:
  virtual ~JSEUtility() {}
};
}

#endif // #ifndef __JSEUtility_H__
