/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <spa/ConversationUtility.h>

#include "llvm/Support/CommandLine.h"
#include <spa/Path.h>

namespace SPA {
extern llvm::cl::opt<std::string> ParticipantName;

long __attribute__((weak))
    conversationUtility(std::vector<std::string> source,
                        std::vector<std::string> destination) {
  // Calculate how much of destination sequence was followed by source.
  auto spos = source.begin(), dpos = destination.begin();
  for (; dpos != destination.end(); dpos++) {
    spos = std::find(spos, source.end(), *dpos);
    if (spos == source.end()) {
      break;
    } else {
      spos++;
    }
  }

  // Compute distance from source to destination as how many extra participants
  // would be needed to make source have all of destination sequence in it.
  // Use this distance to compute an A* metric by subtracting the distance
  // traveled so far, i.e. the size of the source sequence. Return in the form
  // of a utility to be maximized.
  return -(destination.end() - dpos) - source.size();
}

double ConversationUtility::getUtility(klee::ExecutionState *state) {
  assert(state);

  if (state->senderPath) {
    std::vector<std::string> currentConversation;

    std::string pathUUID = "";
    for (auto it : state->senderPath->getSymbolLog()) {
      if (it->getPathUUID() != pathUUID) {
        currentConversation.push_back(it->getParticipant());
        pathUUID = it->getPathUUID();
      }
    }
    currentConversation.push_back(ParticipantName);

    std::set<long> conversationUtilities;
    std::for_each(conversations.begin(), conversations.end(),
                  [&](std::vector<std::string> conversation) {
      conversationUtilities.insert(
          conversationUtility(currentConversation, conversation));
    });

    return *conversationUtilities.rbegin();
  } else {
    return UTILITY_PROCESS_FIRST;
  }
}
}
