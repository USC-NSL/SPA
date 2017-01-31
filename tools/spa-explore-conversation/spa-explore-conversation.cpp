#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>

#include <fstream>

#include <llvm/Support/CommandLine.h>
#include "../../lib/Core/Common.h"
#include "klee/ExprBuilder.h"

#include <spa/Util.h>
#include <spa/PathLoader.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<std::string> InPaths(llvm::cl::Positional, llvm::cl::Required,
                                   llvm::cl::desc("<input path-file>"));

llvm::cl::opt<bool> FollowInPaths(
    "follow-in-paths",
    llvm::cl::desc(
        "Enables following the input path file as new data as added."));

llvm::cl::opt<std::string> OutputPaths(
    "out-paths",
    llvm::cl::desc("Sets the output path file (default: append to input."));

llvm::cl::opt<bool>
    OutputPathsAppend("out-paths-append",
                      llvm::cl::desc("Enable appending paths to output file."));

llvm::cl::opt<bool> NoDups(
    "no-dups",
    llvm::cl::desc(
        "Pre-scan the input file and avoid deriving any duplicate paths."));

llvm::cl::list<std::string>
    Connect("connect", llvm::cl::desc("Specifies symbols to connect in a "
                                      "receiverSymbol1=senderSymbol2 format."));

llvm::cl::opt<bool> ConnectSockets(
    "connect-sockets", llvm::cl::init(false),
    llvm::cl::desc("Automatically connect socket inputs to outputs."));

llvm::cl::list<std::string> ParticipantIP(
    "participant-ip",
    llvm::cl::desc(
        "Specifies the IP that can be assumed to belong entirely to a single "
        "participant in a <participant-name>:<IP-address> format."));

llvm::cl::opt<bool> Debug("d", llvm::cl::desc("Show debug information."));
}

// [base-UUID, source-UUID]
std::set<std::pair<std::string, std::string> > preDerivedPaths;
// Receiver symbol -> Sender symbol.
std::map<std::string, std::string> seedSymbolMappings;
// Participant name -> [IPs]
std::map<std::string, std::set<std::string> > participantIPs;
// UUID -> [UUID]
std::map<std::string, unsigned long> pathIDs;
// UUID -> [UUID]
std::map<std::string, std::string> parentPaths;
// UUID -> [UUID]
std::map<std::string, std::string> participantParentPaths;
// UUID -> [UUID]
std::map<std::string, std::set<std::string> > childrenPaths;
// UUID -> [UUID]
std::map<std::string, std::set<std::string> > participantChildrenPaths;

klee::Solver *solver = klee::createIndependentSolver(klee::createCachingSolver(
    klee::createCexCachingSolver(new klee::STPSolver(false, true))));

namespace SPA {
Path *buildDerivedPath(Path *basePath, Path *sourcePath) {
  // Check if already derived.
  if (NoDups && preDerivedPaths.count(std::make_pair(basePath->getUUID(),
                                                     sourcePath->getUUID()))) {
    klee::klee_message("Suppressing duplicate derived from path %s with %s.",
                       basePath->getUUID().c_str(),
                       sourcePath->getUUID().c_str());
    preDerivedPaths.erase(
        std::make_pair(basePath->getUUID(), sourcePath->getUUID()));
    return NULL;
  }
  // Don't augment the root path.
  if (basePath->getSymbolLog().empty() || sourcePath->getSymbolLog().empty()) {
    //     klee::klee_message("Fail 1.");
    return NULL;
  }
  // Don't use derived paths to augment others
  // (use the source that they were derived from).
  if (!sourcePath->getDerivedFromUUID().empty()) {
    //     klee::klee_message("Fail 2.");
    return NULL;
  }
  // Find commonalities in each path's symbol logs.
  // Position of first divergent entries.
  unsigned long commonEntries;
  for (commonEntries = 0; commonEntries < basePath->getSymbolLog().size() &&
                              commonEntries < sourcePath->getSymbolLog().size();
       commonEntries++) {
    if (basePath->getSymbolLog()[commonEntries]->getPathUUID() !=
            sourcePath->getSymbolLog()[commonEntries]->getPathUUID() ||
        basePath->getSymbolLog()[commonEntries]->getFullName() !=
            sourcePath->getSymbolLog()[commonEntries]->getFullName()) {
      break;
    }
  }
  // Position of first log entry not in common.
  // May be ahead of the entries that came from the common participants if base
  // has multiple successive derivations.
  unsigned long newLogPos;
  for (newLogPos = commonEntries;
       newLogPos < basePath->getSymbolLog().size() &&
           newLogPos < sourcePath->getSymbolLog().size();
       newLogPos++) {
    if (basePath->getSymbolLog()[newLogPos]->getFullName() !=
        sourcePath->getSymbolLog()[newLogPos]->getFullName()) {
      break;
    }
  }
  // Does base have something new that source would be adding to?
  // Otherwise destination becomes the same as source.
  if (commonEntries == basePath->getSymbolLog().size() ||
      newLogPos == basePath->getSymbolLog().size()) {
    //     klee::klee_message("Fail 3.");
    return NULL;
  }
  // Does source have something new to add?
  // Otherwise destination becomes the same as base.
  if (commonEntries == sourcePath->getSymbolLog().size() ||
      newLogPos == sourcePath->getSymbolLog().size()) {
    //     klee::klee_message("Fail 4.");
    return NULL;
  }
  // Only consider scenario where source has a single contribution.
  std::string newLogPathUUID =
      sourcePath->getSymbolLog()[newLogPos]->getPathUUID();
  for (auto it = sourcePath->getSymbolLog().begin() + newLogPos,
            ie = sourcePath->getSymbolLog().end();
       it != ie; it++) {
    if ((*it)->getPathUUID() != newLogPathUUID) {
      //       klee::klee_message("Fail 5.");
      return NULL;
    }
  }
  // The parent path to the source must either be the common path
  // or the path that the base was derived from.
  if (basePath->getDerivedFromUUID().empty()) {
    if (newLogPos > commonEntries) {
      //       klee::klee_message("Fail 6.");
      return NULL;
    }
  } else {
    if (newLogPos == 0 ||
        (sourcePath->getSymbolLog()[newLogPos - 1]->getPathUUID() !=
             basePath->getSymbolLog()[newLogPos - 1]->getPathUUID() &&
         sourcePath->getSymbolLog()[newLogPos - 1]->getPathUUID() !=
             basePath->getDerivedFromUUID())) {
      //       klee::klee_message("Fail 7.");
      return NULL;
    }
  }
  // Check if the source contribution is already in the new part of base.
  for (auto i = newLogPos; i < basePath->getSymbolLog().size(); i++) {
    if (basePath->getSymbolLog()[i]->getFullName() ==
        sourcePath->getSymbolLog()[newLogPos]->getFullName()) {
      //       klee::klee_message("Fail 8.");
      return NULL;
    }
  }
  // Check if any of the new base participants sent anything to the source.
  for (auto bsit = basePath->getSymbolLog().begin() + newLogPos,
            bsie = basePath->getSymbolLog().end();
       bsit != bsie; bsit++) {
    if (ConnectSockets && (*bsit)->isOutput() && (*bsit)->isMessage() &&
        (!(*bsit)->isDropped()) &&
        participantIPs[sourcePath->getSymbolLog().back()->getParticipant()]
            .count((*bsit)->getMessageDestinationIP())) {
      //       klee::klee_message("Fail 9.");
      return NULL;
    }
    for (auto ssit = sourcePath->getSymbolLog().begin() + newLogPos,
              ssie = sourcePath->getSymbolLog().end();
         ssit != ssie; ssit++) {
      if (seedSymbolMappings[(*ssit)->getQualifiedName()] ==
          (*bsit)->getQualifiedName()) {
        //         klee::klee_message("Fail 10.");
        return NULL;
      }
      if (ConnectSockets && (*bsit)->isOutput() && (*bsit)->isMessage() &&
          (!(*bsit)->isDropped()) && (*ssit)->isInput() &&
          (*ssit)->isMessage() &&
          checkMessageCompatibility(*bsit, (*ssit)->getLocalName())) {
        //         klee::klee_message("Fail 11.");
        return NULL;
      }
    }
  }

  Path *destinationPath = new Path();

  // AND together the path-constraints then get test-case to check if SAT.
  // Unknowns to solve for.
  std::vector<std::string> objectNames;
  std::vector<const klee::Array *> objects;
  // Big AND.
  for (auto cit : basePath->constraints) {
    // Don't add internal symbols as they may collide.
    if (klee::EqExpr *eqExpr = llvm::dyn_cast<klee::EqExpr>(cit)) {
      if (klee::ConcatExpr *catExpr =
              llvm::dyn_cast<klee::ConcatExpr>(eqExpr->right)) {
        if (klee::ReadExpr *rdExpr =
                llvm::dyn_cast<klee::ReadExpr>(catExpr->getLeft())) {
          if (rdExpr->updates.root->name.compare(0, strlen(SPA_INTERNAL_PREFIX),
                                                 SPA_INTERNAL_PREFIX) == 0) {
            continue;
          }
        }
      }
    }
    if (!destinationPath->constraints.addAndCheckConstraint(cit)) {
      delete destinationPath;
      return NULL;
    }
  }
  for (auto cit : sourcePath->constraints) {
    if (!destinationPath->constraints.addAndCheckConstraint(cit)) {
      delete destinationPath;
      return NULL;
    }
  }
  // Connect API/Model inputs.
  for (unsigned long lit = 0; lit < newLogPos; lit++) {
    if (basePath->getSymbolLog()[lit]->isInput() &&
        (basePath->getSymbolLog()[lit]->isAPI() ||
         basePath->getSymbolLog()[lit]->isModel())) {
      assert(basePath->getSymbolLog()[lit]->getFullName() ==
                 sourcePath->getSymbolLog()[lit]->getFullName() &&
             basePath->getSymbolLog()[lit]->getInputArray()->size ==
                 sourcePath->getSymbolLog()[lit]->getInputArray()->size &&
             "Common part of log is not in sync.");
      objectNames.push_back(basePath->getSymbolLog()[lit]->getFullName());
      objects.push_back(basePath->getSymbolLog()[lit]->getInputArray());
      for (size_t offset = 0;
           offset < basePath->getSymbolLog()[lit]->getInputArray()->size;
           offset++) {
        klee::UpdateList bul(basePath->getSymbolLog()[lit]->getInputArray(), 0);
        klee::UpdateList sul(sourcePath->getSymbolLog()[lit]->getInputArray(),
                             0);
        llvm::OwningPtr<klee::ExprBuilder> exprBuilder(
            klee::createDefaultExprBuilder());
        klee::ref<klee::Expr> e = exprBuilder->Eq(
            exprBuilder->Read(bul,
                              exprBuilder->Constant(offset, klee::Expr::Int32)),
            exprBuilder->Read(
                sul, exprBuilder->Constant(offset, klee::Expr::Int32)));
        if (!destinationPath->constraints.addAndCheckConstraint(e)) {
          delete destinationPath;
          return NULL;
        }
      }
    }
  }
  // Solve for API/Model inputs.
  std::vector<std::vector<unsigned char> > result;
  if (solver->getInitialValues(
          klee::Query(destinationPath->constraints,
                      klee::createDefaultExprBuilder()->False()),
          objects, result)) {
    for (size_t i = 0; i < result.size(); i++) {
      destinationPath->testInputs[objectNames[i]] = result[i];
    }
  } else {
    delete destinationPath;
    return NULL;
  }

  // Append new data from source to base.
  // New symbol log entries.
  destinationPath->symbolLog = basePath->getSymbolLog();
  for (auto it = sourcePath->getSymbolLog().begin() + newLogPos,
            ie = sourcePath->getSymbolLog().end();
       it != ie; it++) {
    destinationPath->symbolLog.emplace_back(new Symbol(**it));
    destinationPath->symbolLog.back()->pathUUID = destinationPath->getUUID();
    destinationPath->symbolLog.back()->derivedFromUUID = sourcePath->getUUID();
  }
  // New symbols.
  destinationPath->inputSymbols = basePath->inputSymbols;
  destinationPath->outputSymbols = basePath->outputSymbols;
  for (unsigned long slit = newLogPos; slit < sourcePath->getSymbolLog().size();
       slit++) {
    if (sourcePath->getSymbolLog()[slit]->isInput()) {
      destinationPath
          ->inputSymbols[sourcePath->getSymbolLog()[slit]->getQualifiedName()]
          .push_back(sourcePath->getSymbolLog()[slit]);
    } else if (sourcePath->getSymbolLog()[slit]->isOutput()) {
      destinationPath
          ->outputSymbols[sourcePath->getSymbolLog()[slit]->getQualifiedName()]
          .push_back(sourcePath->getSymbolLog()[slit]);
    } else {
      assert(false && "Symbol is neither input nor output.");
    }
  }
  // Tags.
  destinationPath->tags = sourcePath->tags;
  destinationPath->tags.insert(basePath->tags.begin(), basePath->tags.end());
  // Explored line coverage.
  destinationPath->exploredLineCoverage = basePath->exploredLineCoverage;
  for (auto pit : sourcePath->exploredLineCoverage) {
    for (auto fit : pit.second) {
      for (auto lit : fit.second) {
        destinationPath
            ->exploredLineCoverage[pit.first][fit.first][lit.first] |=
            lit.second;
      }
    }
  }
  // Explored function coverage.
  destinationPath->exploredFunctionCoverage =
      basePath->exploredFunctionCoverage;
  for (auto pit : sourcePath->exploredFunctionCoverage) {
    for (auto fit : pit.second) {
      destinationPath->exploredFunctionCoverage[pit.first][fit.first] |=
          fit.second;
    }
  }
  // Explored path.
  destinationPath->exploredPath = basePath->exploredPath;
  destinationPath
      ->exploredPath[sourcePath->getSymbolLog().back()->getParticipant()] =
          sourcePath
      ->exploredPath[sourcePath->getSymbolLog().back()->getParticipant()];
  // Test line coverage.
  destinationPath->testLineCoverage = basePath->testLineCoverage;
  for (auto pit : sourcePath->testLineCoverage) {
    for (auto fit : pit.second) {
      for (auto lit : fit.second) {
        destinationPath->testLineCoverage[pit.first][fit.first][lit.first] |=
            lit.second;
      }
    }
  }
  // Test function coverage.
  destinationPath->testFunctionCoverage = basePath->testFunctionCoverage;
  for (auto pit : sourcePath->testFunctionCoverage) {
    for (auto fit : pit.second) {
      destinationPath->testFunctionCoverage[pit.first][fit.first] |= fit.second;
    }
  }

  return destinationPath;
}
}

#define UUID_SIZE 36
std::string getUUID(SPA::PathLoader *pathLoader) {
  char *result = (char *)mmap(NULL, UUID_SIZE + 1, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  auto pos = pathLoader->save();
  if (fork() == 0) {
    SPA::Path *path = pathLoader->getPath();
    assert(path && path->getUUID().length() <= UUID_SIZE && "Bad UUID.");
    strncpy(result, path->getUUID().c_str(), UUID_SIZE + 1);
    exit(0);
  } else {
    wait(NULL);
  }
  pathLoader->load(pos);

  std::string result_str = result;
  munmap(result, UUID_SIZE + 1);
  return result_str;
}

std::string getParentUUID(SPA::PathLoader *pathLoader) {
  char *result = (char *)mmap(NULL, UUID_SIZE + 1, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  auto pos = pathLoader->save();
  if (fork() == 0) {
    SPA::Path *path = pathLoader->getPath();
    assert(path && path->getParentUUID().length() <= UUID_SIZE && "Bad UUID.");
    strncpy(result, path->getParentUUID().c_str(), UUID_SIZE + 1);
    exit(0);
  } else {
    wait(NULL);
  }
  pathLoader->load(pos);

  std::string result_str = result;
  munmap(result, UUID_SIZE + 1);
  return result_str;
}

std::string getParticipantParentUUID(SPA::PathLoader *pathLoader) {
  char *result = (char *)mmap(NULL, UUID_SIZE + 1, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  auto pos = pathLoader->save();
  if (fork() == 0) {
    SPA::Path *path = pathLoader->getPath();
    assert(path && path->getParentUUID().length() <= UUID_SIZE && "Bad UUID.");
    strncpy(result, path->getParticipantParentUUID().c_str(), UUID_SIZE + 1);
    exit(0);
  } else {
    wait(NULL);
  }
  pathLoader->load(pos);

  std::string result_str = result;
  munmap(result, UUID_SIZE + 1);
  return result_str;
}

std::string getDerivedFromUUID(SPA::PathLoader *pathLoader) {
  char *result = (char *)mmap(NULL, UUID_SIZE + 1, PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  auto pos = pathLoader->save();
  if (fork() == 0) {
    SPA::Path *path = pathLoader->getPath();
    assert(path && path->getDerivedFromUUID().length() <= UUID_SIZE &&
           "Bad UUID.");
    strncpy(result, path->getDerivedFromUUID().c_str(), UUID_SIZE + 1);
    exit(0);
  } else {
    wait(NULL);
  }
  pathLoader->load(pos);

  std::string result_str = result;
  munmap(result, UUID_SIZE + 1);
  return result_str;
}

void processNewPath(SPA::PathLoader *pathLoader, unsigned long pathID,
                    std::ofstream &outFile) {
  klee::klee_message("Processing path %ld", pathID);
  // Get path data.
  pathLoader->gotoPath(pathID);
  std::string pathUUID = getUUID(pathLoader);
  std::string pathParent = getParentUUID(pathLoader);
  std::string pathParticipantParent = getParticipantParentUUID(pathLoader);
  // Build path ancestry tree.
  pathIDs[pathUUID] = pathID;
  parentPaths[pathUUID] = pathParent;
  participantParentPaths[pathUUID] = pathParticipantParent;
  childrenPaths[pathParent].insert(pathUUID);
  participantChildrenPaths[pathParticipantParent].insert(pathUUID);
  // Find all potential base paths that can be augmented by the new one.
  // These are all of the progeny of the new path's parents.
  std::set<std::string> basePaths, worklist;
  worklist.insert(participantParentPaths[pathUUID]);
  while (!worklist.empty()) {
    std::string path = *worklist.begin();
    worklist.erase(path);

    if (path != "") {
      basePaths.insert(path);
    }
    worklist.insert(childrenPaths[path].begin(), childrenPaths[path].end());
  }
  // Find all potential source paths that can augment the new one.
  // These are all of the immediate children of the new paths ancestry.
  std::set<std::string> srcPaths;
  std::string path = pathUUID;
  do {
    std::string child = path;
    path = parentPaths[path];
    srcPaths.insert(participantChildrenPaths[path].begin(),
                    participantChildrenPaths[path].end());
    // Exclude the ancestor itself from the list.
    srcPaths.erase(child);
  } while (path != "");

  // The child process will mess with the file position but not the
  // pathLoader,
  // so checkpoint and recover.
  auto pos = pathLoader->save();
  if (fork() == 0) {
    std::shared_ptr<SPA::Path> path(pathLoader->getPath(pathID));
    assert(path && "Path not available.");

    for (auto basePathUUID : basePaths) {
      std::shared_ptr<SPA::Path> srcPath(path);
      assert(pathIDs.count(basePathUUID) && "Path not in path file.");
      std::unique_ptr<SPA::Path> basePath(
          pathLoader->getPath(pathIDs[basePathUUID]));

      //       klee::klee_message("Trying to Augment path %ld (%s) with %ld
      // (%s).",
      //                          pathIDs[basePathUUID], basePathUUID.c_str(),
      // pathID,
      //                          srcPath->getUUID().c_str());
      std::unique_ptr<SPA::Path> derivedPath(
          SPA::buildDerivedPath(basePath.get(), srcPath.get()));
      if (derivedPath) {
        klee::klee_message(
            "Augmented path %ld (%s) with %ld (%s) to produce %s.",
            pathIDs[basePathUUID], basePathUUID.c_str(), pathID,
            srcPath->getUUID().c_str(), derivedPath->getUUID().c_str());
        outFile << *derivedPath;
      }
    }

    for (auto srcPathUUID : srcPaths) {
      // The child process will mess with the file position but not the
      // pathLoader,
      // so checkpoint and recover.
      std::shared_ptr<SPA::Path> basePath(path);
      assert(pathIDs.count(srcPathUUID) && "Path not in path file.");
      std::unique_ptr<SPA::Path> srcPath(
          pathLoader->getPath(pathIDs[srcPathUUID]));

      //       klee::klee_message("Trying to augment path %ld (%s) with %ld
      // (%s).",
      //                          pathID, basePath->getUUID().c_str(),
      //                          pathIDs[srcPathUUID], srcPathUUID.c_str());
      std::unique_ptr<SPA::Path> derivedPath(
          SPA::buildDerivedPath(basePath.get(), srcPath.get()));
      if (derivedPath) {
        klee::klee_message(
            "Augmented path %ld (%s) with %ld (%s) to produce %s.", pathID,
            basePath->getUUID().c_str(), pathIDs[srcPathUUID],
            srcPathUUID.c_str(), derivedPath->getUUID().c_str());
        outFile << *derivedPath;
      }
    }
    exit(0);
  } else {
    wait(NULL);
  }
  pathLoader->load(pos);
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(
      argc, argv, "Systematic Protocol Analysis - Explore Conversation");

  klee::klee_message("Reading input from: %s", InPaths.c_str());
  std::ifstream ifs(InPaths);
  assert(ifs.good() && "Unable to open input path file.");
  std::unique_ptr<SPA::PathLoader> pathLoader(new SPA::PathLoader(ifs, true));

  if (OutputPaths.empty()) {
    OutputPaths = InPaths;
    OutputPathsAppend = true;
  }
  klee::klee_message("Writing output to: %s", OutputPaths.c_str());
  std::ofstream outFile(
      OutputPaths.c_str(),
      std::ios_base::out |
          (OutputPathsAppend ? std::ios_base::app : std::ios_base::trunc));
  assert(outFile.is_open() && "Unable to open output path-file.");

  for (auto connection : Connect) {
    auto delim = connection.find('=');
    std::string rValue = connection.substr(0, delim);
    std::string sValue = connection.substr(delim + 1);
    seedSymbolMappings[rValue] = sValue;
  }

  for (auto participantIP : ParticipantIP) {
    auto delim = participantIP.find(':');
    std::string participant = participantIP.substr(0, delim);
    std::string ip = participantIP.substr(delim + 1);
    participantIPs[participant].insert(ip);
  }

  if (NoDups) {
    for (unsigned long pathID = 0;; pathID++) {
      auto pos = pathLoader->save();
      if (!pathLoader->skipPath()) {
        break;
      }
      pathLoader->load(pos);

      klee::klee_message("Scanning path %ld.", pathID);
      std::string parentUUID = getParentUUID(pathLoader.get());
      std::string derivedFromUUID = getDerivedFromUUID(pathLoader.get());

      if (!derivedFromUUID.empty()) {
        klee::klee_message("  Already augmented %s with %s.",
                           parentUUID.c_str(), derivedFromUUID.c_str());
        preDerivedPaths.insert(std::make_pair(parentUUID, derivedFromUUID));
      }

      assert(pathLoader->skipPath());
    }
  }

  unsigned long pathID = 0;
  while (true) {
    pathLoader->gotoPath(pathID);
    if (FollowInPaths) {
      while (!pathLoader->skipPath()) {
        sleep(1);
        klee::klee_message("Waiting for path %ld.", pathID);
      }
    } else {
      if (!pathLoader->skipPath()) {
        break;
      }
    }

    processNewPath(pathLoader.get(), pathID++, outFile);
  }

  outFile.flush();
  klee::klee_message("Done.");

  return 0;
}
