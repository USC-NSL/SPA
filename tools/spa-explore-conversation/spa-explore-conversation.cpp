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

llvm::cl::opt<bool> Debug("d", llvm::cl::desc("Show debug information."));
}

std::vector<SPA::Path *> paths;

std::map<std::string, std::string> seedSymbolMappings;

klee::Solver *solver = klee::createIndependentSolver(klee::createCachingSolver(
    klee::createCexCachingSolver(new klee::STPSolver(false, true))));

namespace SPA {
Path *buildDerivedPath(Path *basePath, Path *sourcePath) {
  // Don't augment the root path.
  if (basePath->symbolLog.empty() || sourcePath->symbolLog.empty()) {
    return NULL;
  }
  // Don't use derived paths to augment others (us the base that derived them).
  if (!sourcePath->derivedFromUUID.empty()) {
    return NULL;
  }
  // Find commonalities in each path's participant and symbol logs.
  // Position of first divergent participant entry.
  unsigned long commonParticipants;
  // Position of first new source symbol log entry.
  auto newSourceLog = sourcePath->symbolLog.begin();
  for (commonParticipants = 0;
       commonParticipants < basePath->getParticipants().size() &&
           commonParticipants < sourcePath->getParticipants().size();
       commonParticipants++) {
    if (basePath->getParticipants()[commonParticipants]->getPathUUID() ==
        sourcePath->getParticipants()[commonParticipants]->getPathUUID()) {
      while (newSourceLog != sourcePath->symbolLog.end() &&
             (*newSourceLog)->getParticipant() ==
                 basePath->getParticipants()[commonParticipants]->getName()) {
        newSourceLog++;
      }
    } else {
      break;
    }
  }
  // Does base have something new that source would be adding to?
  // Otherwise destination becomes the same as source.
  if (commonParticipants == basePath->getParticipants().size()) {
    return NULL;
  }
  // Only consider scenario where source is ahead by a single participant.
  if (sourcePath->getParticipants().size() - commonParticipants != 1) {
    return NULL;
  }
  // Check if the source contributor is already in the new part of base.
  for (auto i = commonParticipants; i < basePath->getParticipants().size();
       i++) {
    if (basePath->getParticipants()[i]->getName() ==
        sourcePath->getParticipants()[commonParticipants]->getName()) {
      return NULL;
    }
  }
  // Check if latest base participant sent anything that the source could use.
  for (auto bsit = basePath->symbolLog.rbegin();
       (*bsit)->getParticipant() ==
           basePath->getParticipants().back()->getName();
       bsit++) {
    for (auto ssit = newSourceLog, ssie = sourcePath->symbolLog.end();
         ssit != ssie; ssit++) {
      if (seedSymbolMappings[(*ssit)->getQualifiedName()] ==
              (*bsit)->getQualifiedName() ||
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
  // Connect API inputs.
  for (auto bsit = basePath->symbolLog.begin(),
            ssit = sourcePath->symbolLog.begin();
       ssit != newSourceLog; bsit++, ssit++) {
    if ((*bsit)->isInput() && (*bsit)->isAPI()) {
      assert((*bsit)->getFullName() == (*ssit)->getFullName() &&
             (*bsit)->getInputArray()->size == (*ssit)->getInputArray()->size &&
             "Common part of log is not in sync.");
      objectNames.push_back((*bsit)->getFullName());
      objects.push_back((*bsit)->getInputArray());
      //       *const_cast<std::string *>(&(*ssit)->getInputArray()->name) +=
      // "_dummy";
      for (size_t offset = 0; offset < (*bsit)->getInputArray()->size;
           offset++) {
        klee::UpdateList bul((*bsit)->getInputArray(), 0);
        klee::UpdateList sul((*ssit)->getInputArray(), 0);
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
  // Solve for API inputs.
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
                                    newSourceLog, sourcePath->symbolLog.end());
  // New symbols.
  destinationPath->inputSymbols = basePath->inputSymbols;
  destinationPath->outputSymbols = basePath->outputSymbols;
  for (; newSourceLog != sourcePath->symbolLog.end(); newSourceLog++) {
    if ((*newSourceLog)->isInput()) {
      destinationPath->inputSymbols[(*newSourceLog)->getQualifiedName()]
          .push_back(*newSourceLog);
    } else if ((*newSourceLog)->isOutput()) {
      destinationPath->outputSymbols[(*newSourceLog)->getQualifiedName()]
          .push_back(*newSourceLog);
    } else {
      assert(false && "Symbol is neither input nor output.");
    }
  }
  // Tags.
  destinationPath->tags = sourcePath->tags;
  destinationPath->tags.insert(basePath->tags.begin(), basePath->tags.end());
  // Explored line coverage.
  destinationPath->exploredLineCoverage = basePath->exploredLineCoverage;
  for (auto fit : sourcePath->exploredLineCoverage) {
    for (auto lit : fit.second) {
      destinationPath->exploredLineCoverage[fit.first][lit.first] |= lit.second;
    }
  }
  // Explored function coverage.
  destinationPath->exploredFunctionCoverage =
      basePath->exploredFunctionCoverage;
  for (auto fit : sourcePath->exploredFunctionCoverage) {
    destinationPath->exploredFunctionCoverage[fit.first] |= fit.second;
  }
  // Explored path.
  destinationPath->exploredPath = basePath->exploredPath;
  destinationPath
      ->exploredPath[sourcePath->getParticipants().back()->getName()] =
          sourcePath
      ->exploredPath[sourcePath->getParticipants().back()->getName()];
  // Test line coverage.
  destinationPath->testLineCoverage = basePath->testLineCoverage;
  for (auto fit : sourcePath->testLineCoverage) {
    for (auto lit : fit.second) {
      destinationPath->testLineCoverage[fit.first][lit.first] |= lit.second;
    }
  }
  // Test function coverage.
  destinationPath->testFunctionCoverage = basePath->testFunctionCoverage;
  for (auto fit : sourcePath->testFunctionCoverage) {
    destinationPath->testFunctionCoverage[fit.first] |= fit.second;
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
