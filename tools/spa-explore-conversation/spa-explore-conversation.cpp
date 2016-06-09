#include <unistd.h>

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

// Path ID -> Path
std::vector<SPA::Path *> paths;
// Receiver symbol -> Sender symbol.
std::map<std::string, std::string> seedSymbolMappings;
// Participant name -> [IPs]
std::map<std::string, std::set<std::string> > participantIPs;

klee::Solver *solver = klee::createIndependentSolver(klee::createCachingSolver(
    klee::createCexCachingSolver(new klee::STPSolver(false, true))));

namespace SPA {
Path *buildDerivedPath(Path *basePath, Path *sourcePath) {
  // Don't augment the root path.
  if (basePath->symbolLog.empty() || sourcePath->symbolLog.empty()) {
    return NULL;
  }
  // Don't use derived paths to augment others
  // (use the source that they were derived from).
  if (!sourcePath->derivedFromUUID.empty()) {
    return NULL;
  }
  // Find commonalities in each path's participant and symbol logs.
  // Position of first divergent entries.
  unsigned long commonParticipants;
  for (commonParticipants = 0;
       commonParticipants < basePath->getParticipants().size() &&
           commonParticipants < sourcePath->getParticipants().size();
       commonParticipants++) {
    if (basePath->getParticipants()[commonParticipants]->getPathUUID() !=
        sourcePath->getParticipants()[commonParticipants]->getPathUUID()) {
      break;
    }
  }
  // Position of first log entry not in common.
  // May be ahead of the entries that came from the common participants if base
  // has multiple successive derivations.
  unsigned long newLogPos;
  for (newLogPos = 0; newLogPos < basePath->symbolLog.size() &&
                          newLogPos < sourcePath->symbolLog.size();
       newLogPos++) {
    if (basePath->symbolLog[newLogPos]->getFullName() !=
        sourcePath->symbolLog[newLogPos]->getFullName()) {
      break;
    }
  }
  // Does base have something new that source would be adding to?
  // Otherwise destination becomes the same as source.
  if (commonParticipants == basePath->getParticipants().size() ||
      newLogPos == basePath->symbolLog.size()) {
    return NULL;
  }
  // Does source have something new to add?
  // Otherwise destination becomes the same as base.
  if (commonParticipants == sourcePath->getParticipants().size() ||
      newLogPos == sourcePath->symbolLog.size()) {
    return NULL;
  }
  // Only consider scenario where source has a single contribution, i.e. the
  // source's immediate parent is either the common point or the point the base
  // was derived from itself (if it was derived).
  if (sourcePath->getParticipants().size() - 1 != commonParticipants &&
      sourcePath->getParticipants().size() <= 1 &&
      sourcePath->getParticipants().rbegin()[1]->getPathUUID() !=
          basePath->getDerivedFromUUID()) {
    return NULL;
  }
  // Check if any of the new base participants sent anything to the source.
  for (auto bsit = basePath->symbolLog.begin() + newLogPos,
            bsie = basePath->symbolLog.end();
       bsit != bsie; bsit++) {
    if (ConnectSockets && (*bsit)->isOutput() && (*bsit)->isMessage() &&
        (!(*bsit)->isDropped()) &&
        participantIPs[sourcePath->getParticipants().back()->getName()]
            .count((*bsit)->getMessageDestinationIP())) {
      return NULL;
    }
    for (auto ssit = sourcePath->symbolLog.begin() + newLogPos,
              ssie = sourcePath->symbolLog.end();
         ssit != ssie; ssit++) {
      if (seedSymbolMappings[(*ssit)->getQualifiedName()] ==
          (*bsit)->getQualifiedName()) {
        return NULL;
      }
      if (ConnectSockets && (*bsit)->isOutput() && (*bsit)->isMessage() &&
          (!(*bsit)->isDropped()) && (*ssit)->isInput() &&
          (*ssit)->isMessage() &&
          checkMessageCompatibility(*bsit, (*ssit)->getLocalName())) {
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
    if (basePath->symbolLog[lit]->isInput() &&
        (basePath->symbolLog[lit]->isAPI() ||
         basePath->symbolLog[lit]->isModel())) {
      assert(basePath->symbolLog[lit]->getFullName() ==
                 sourcePath->symbolLog[lit]->getFullName() &&
             basePath->symbolLog[lit]->getInputArray()->size ==
                 sourcePath->symbolLog[lit]->getInputArray()->size &&
             "Common part of log is not in sync.");
      objectNames.push_back(basePath->symbolLog[lit]->getFullName());
      objects.push_back(basePath->symbolLog[lit]->getInputArray());
      for (size_t offset = 0;
           offset < basePath->symbolLog[lit]->getInputArray()->size; offset++) {
        klee::UpdateList bul(basePath->symbolLog[lit]->getInputArray(), 0);
        klee::UpdateList sul(sourcePath->symbolLog[lit]->getInputArray(), 0);
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
  // New participant.
  destinationPath->participants = basePath->participants;
  destinationPath->participants.emplace_back(new Participant(
      sourcePath->participants.back()->getName(), destinationPath->uuid));
  // Keep track of where destination was derived from.
  destinationPath->derivedFromUUID = sourcePath->uuid;
  // New symbol log entries.
  destinationPath->symbolLog = basePath->symbolLog;
  destinationPath->symbolLog.insert(destinationPath->symbolLog.end(),
                                    sourcePath->symbolLog.begin() + newLogPos,
                                    sourcePath->symbolLog.end());
  // New symbols.
  destinationPath->inputSymbols = basePath->inputSymbols;
  destinationPath->outputSymbols = basePath->outputSymbols;
  for (unsigned long slit = newLogPos; slit < sourcePath->symbolLog.size();
       slit++) {
    if (sourcePath->symbolLog[slit]->isInput()) {
      destinationPath
          ->inputSymbols[sourcePath->symbolLog[slit]->getQualifiedName()]
          .push_back(sourcePath->symbolLog[slit]);
    } else if (sourcePath->symbolLog[slit]->isOutput()) {
      destinationPath
          ->outputSymbols[sourcePath->symbolLog[slit]->getQualifiedName()]
          .push_back(sourcePath->symbolLog[slit]);
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
      ->exploredPath[sourcePath->getParticipants().back()->getName()] =
          sourcePath
      ->exploredPath[sourcePath->getParticipants().back()->getName()];
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

  SPA::Path *newPath;
  for (unsigned long pathID = 0;; pathID++) {
    while ((!(newPath = pathLoader->getPath())) && FollowInPaths) {
      sleep(1);
      klee::klee_message("Waiting for path %ld.", pathID);
    }
    if (!newPath) {
      break;
    }
    klee::klee_message("Loading path %ld.", pathID);

    for (auto pairPath : paths) {
      std::unique_ptr<SPA::Path> derivedPath(
          SPA::buildDerivedPath(newPath, pairPath));
      if (derivedPath) {
        klee::klee_message("Augmented path %s with %s to produce %s.",
                           newPath->getUUID().c_str(),
                           pairPath->getUUID().c_str(),
                           derivedPath->getUUID().c_str());
        outFile << *derivedPath;
      }

      derivedPath.reset(SPA::buildDerivedPath(pairPath, newPath));
      if (derivedPath) {
        klee::klee_message("Augmented path %s with %s to produce %s.",
                           pairPath->getUUID().c_str(),
                           newPath->getUUID().c_str(),
                           derivedPath->getUUID().c_str());
        outFile << *derivedPath;
      }
    }

    paths.push_back(newPath);
  }

  outFile.flush();
  klee::klee_message("Done.");

  return 0;
}
