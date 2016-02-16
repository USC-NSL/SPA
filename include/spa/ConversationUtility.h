/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __ConversationUtility_H__
#define __ConversationUtility_H__

#include "spa/StateUtility.h"

namespace SPA {
class ConversationUtility : public StateUtility {
private:
  std::set<std::vector<std::string> > conversations;

public:
  ConversationUtility(std::set<std::vector<std::string> > conversations)
      : conversations(conversations) {}
  double getUtility(klee::ExecutionState *state);

protected:
  virtual ~ConversationUtility() {}
};
}

#endif // #ifndef __ConversationUtility_H__
