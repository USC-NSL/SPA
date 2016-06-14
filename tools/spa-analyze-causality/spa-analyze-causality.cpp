#include <fstream>
#include <unistd.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/OwningPtr.h>
#include "../../lib/Core/Common.h"
#include <spa/FilterExpression.h>

#include <spa/PathLoader.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#define FOLLOW_WAIT 1 // s

namespace {
llvm::cl::opt<bool> EnableDbg("d", llvm::cl::init(false),
                              llvm::cl::desc("Output debug information."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));

llvm::cl::opt<std::string> Criteria(llvm::cl::Positional, llvm::cl::Required,
                                    llvm::cl::desc("<criteria>"));
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(
      argc, argv, "Systematic Protocol Analysis - Causality Analysis");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  klee::klee_message("Parsing criteria.");
  llvm::OwningPtr<SPA::FilterExpression> expr(
      SPA::FilterExpression::parse(Criteria));
  assert(expr);
  if (EnableDbg) {
    klee::klee_message("Success expression parsed as: %s",
                       expr->dbg_str().c_str());
  }

  // uuid -> path.
  std::map<std::string, SPA::Path *> pathsByUUID;
  // path -> id.
  std::map<SPA::Path *, unsigned long> pathIDs;
  // path -> [paths].
  std::map<SPA::Path *, std::set<SPA::Path *> > childrenPaths;
  std::set<SPA::Path *> successes, pivots, worklist;

  SPA::PathLoader pathLoader(inFile);
  unsigned long pathID = 0;
  SPA::Path *path;
  while ((path = pathLoader.getPath())) {
    klee::klee_message("Loading path %ld, (%s).", pathID++,
                       path->getUUID().c_str());

    pathsByUUID[path->getUUID()] = path;
    pathIDs[path] = pathID;
    std::string parentUUID = path->getParentUUID();
    if (!parentUUID.empty()) {
      if (pathsByUUID.count(parentUUID)) {
        childrenPaths[pathsByUUID[parentUUID]].insert(path);
      }
    } else {
      childrenPaths[NULL].insert(path);
    }

    if (expr->checkPath(*path)) {
      successes.insert(path);

      if (pathsByUUID.count(parentUUID)) {
        worklist.insert(pathsByUUID[parentUUID]);
      }
    }
  }
  assert((!successes.empty()) && "No paths match criteria.");

  klee::klee_message("Finding pivots.");
  while (!worklist.empty()) {
    path = *worklist.begin();
    worklist.erase(path);

    if (successes.count(path)) {
      continue;
    }

    if (!childrenPaths[path].empty()) {
      bool allSuccess = true, someSuccess = false;
      for (auto child : childrenPaths[path]) {
        if (successes.count(child)) {
          someSuccess = true;
        } else {
          allSuccess = false;
        }
      }

      if (allSuccess) {
        pivots.erase(path);
        successes.insert(path);
        std::string parentUUID = path->getParentUUID();
        if (pathsByUUID.count(parentUUID)) {
          worklist.insert(pathsByUUID[parentUUID]);
        }
      } else if (someSuccess) {
        pivots.insert(path);
      }
    }
  }
  klee::klee_message("Found %ld pivots.", pivots.size());

  unsigned long totalUnexplained = 0;
  klee::klee_message("Finding differences.");
  for (auto pivot : pivots) {
    klee::klee_message("At path %ld, (%s):", pathIDs[pivot],
                       pivot->getUUID().c_str());
    std::set<SPA::Path *> pathsToExplain;
    std::copy_if(childrenPaths[pivot].begin(), childrenPaths[pivot].end(),
                 std::inserter(pathsToExplain, pathsToExplain.begin()),
                 [&](SPA::Path * path) {
      return successes.count(path);
    });

    // Check if participant causes success.
    std::set<std::string> successParticipants, failureParticipants;
    for (auto child : childrenPaths[pivot]) {
      if (successes.count(child)) {
        successParticipants.insert(
            child->getSymbolLog().back()->getParticipant());
      } else {
        failureParticipants.insert(
            child->getSymbolLog().back()->getParticipant());
      }
    }
    for (auto participant : successParticipants) {
      if (!failureParticipants.count(participant)) {
        klee::klee_message("  Having %s participate next leads to a match.",
                           participant.c_str());
        for (auto p : pathsToExplain) {
          if (p->getSymbolLog().back()->getParticipant() == participant) {
            pathsToExplain.erase(p);
          }
        }
      }
    }

    // Check if a log entry causes success.
    std::set<std::string> successEntries, failureEntries;
    for (auto child : childrenPaths[pivot]) {
      if (successes.count(child)) {
        for (auto it =
                 child->getSymbolLog().begin() + pivot->getSymbolLog().size(),
                  ie = child->getSymbolLog().end();
             it != ie; it++) {
          successEntries.insert((*it)->getFullName());
        }
      } else {
        for (auto it =
                 child->getSymbolLog().begin() + pivot->getSymbolLog().size(),
                  ie = child->getSymbolLog().end();
             it != ie; it++) {
          failureEntries.insert((*it)->getFullName());
        }
      }
    }
    for (auto entry : successEntries) {
      if (!failureEntries.count(entry)) {
        klee::klee_message("  The %s symbol leads to a match.", entry.c_str());
        for (auto p : pathsToExplain) {
          for (auto e : p->getSymbolLog()) {
            if (e->getFullName() == entry) {
              pathsToExplain.erase(p);
              break;
            }
          }
        }
      }
    }

    if (!pathsToExplain.empty()) {
      klee::klee_message("  %ld unexplained eventual matches:",
                         pathsToExplain.size());
      for (auto p : pathsToExplain) {
        klee::klee_message("    Path %ld (%s)", pathIDs[p],
                           p->getUUID().c_str());
      }
      totalUnexplained += pathsToExplain.size();
    }
  }
  klee::klee_message("%ld total unexplained eventual matches",
                     totalUnexplained);

  return 0;
}
