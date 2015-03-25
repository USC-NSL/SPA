#include <assert.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <map>

#include <llvm/Support/MemoryBuffer.h>
#include <klee/Constraints.h>
#include <klee/ExprBuilder.h>
#include <klee/Solver.h>

#include <spa/Path.h>
#include <spa/PathLoader.h>
#include <spa/max.h>
#include <spa/SPA.h>
#include <spa/spaRuntimeImpl.h>

#define HANDLER_NAME_TAG "max_HandlerName"

std::map<std::string, std::set<SPA::Path *> > paths;
std::map<std::string, std::pair<void *, size_t> > fixedInputs;
std::map<std::string, std::pair<void *, size_t> > varInputs;
klee::Solver *solver;

void initAdversarialModule() {
  static bool init = false;

  if (!init) {
    std::cerr << "Loading path data from file: " << MAX_PATH_FILE << std::endl;
    std::ifstream pathFile(MAX_PATH_FILE);
    assert(pathFile.is_open() && "Unable to open path file.");

    SPA::PathLoader pathLoader(pathFile);
    while (SPA::Path *path = pathLoader.getPath()) {
      if (!path->getTag(std::string(HANDLER_NAME_TAG)).empty()) {
        paths[path->getTag(std::string(HANDLER_NAME_TAG))].insert(path);
      }
    }

    pathFile.close();

    solver = new klee::STPSolver(false, true);
    solver = klee::createCexCachingSolver(solver);
    solver = klee::createCachingSolver(solver);
    solver = klee::createIndependentSolver(solver);

    init = true;
  }
}

void solvePath(char *name) {
  std::cerr << "Finding interesting paths for " << name << "." << std::endl;

  // Check if handler has known paths.
  if (paths.count(name)) {
    //     std::cerr << "Fixed inputs:" << std::endl;
    //     for (auto iit : fixedInputs) {
    //       std::cerr << "	" << iit.first << " =";
    //       for (unsigned i = 0; i < iit.second.second; i++) {
    //         std::cerr << " " << (int)((uint8_t *)iit.second.first)[i];
    //       }
    //       std::cerr << std::endl;
    //     }

    unsigned int numPaths = 0;
    // Check each path.
    for (auto pit : paths[name]) {
      numPaths++;
      klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
      // Load path constraints.
      klee::ConstraintManager cm(pit->getConstraints());
      // And all relevant fixed input constraints.
      for (auto iit : fixedInputs) {
        // Check if input is relevant.
        if (pit->getSymbol(iit.first)) {
          // Add the comparison of each byte.
          klee::UpdateList ul(pit->getSymbol(iit.first), 0);
          for (size_t p = 0; p < iit.second.second; p++) {
            cm.addConstraint(exprBuilder->Eq(
                klee::ReadExpr::create(
                    ul, klee::ConstantExpr::alloc(p, klee::Expr::Int32)),
                exprBuilder->Constant(
                    llvm::APInt(8, ((uint8_t *)iit.second.first)[p]))));
          }
        }
      }

      // Created list of variable inputs.
      std::vector<const klee::Array *> objects;
      for (auto iit : varInputs) {
        assert(pit->getSymbol(iit.first) && "Unrecognized variable.");
        objects.push_back(pit->getSymbol(iit.first));
      }
      assert(!objects.empty() && "No variable inputs to solve for.");

      std::vector<std::vector<unsigned char> > result;
      if (solver->getInitialValues(klee::Query(cm, exprBuilder->False()),
                                   objects, result)) {
        std::cerr << "Found interesting assignment. Searched " << numPaths
                  << "/" << paths[name].size() << " paths." << std::endl;
        std::cerr << "Assignment:" << std::endl;

        std::map<std::string, std::pair<void *, size_t> >::iterator iit, iie;
        std::vector<std::vector<unsigned char> >::iterator rit, rie;
        for (iit = varInputs.begin(), iie = varInputs.end(),
            rit = result.begin(), rie = result.end();
             iit != iie && rit != rie; iit++, rit++) {
          assert(iit->second.second == rit->size() &&
                 "Result size doesn't match declared size.");

          std::cerr << "	" << iit->first << ":";
          for (size_t i = 0; i < iit->second.second; i++) {
            std::cerr << " " << (int)((uint8_t *)iit->second.first)[i];
          }

          for (size_t i = 0; i < rit->size(); i++) {
            ((unsigned char *)iit->second.first)[i] = (*rit)[i];
          }

          std::cerr << " ->";
          for (size_t i = 0; i < iit->second.second; i++) {
            std::cerr << " " << (int)((uint8_t *)iit->second.first)[i];
          }
          std::cerr << std::endl;
        }
        return;
      } else {
        //         std::cerr << "Path does not match." << std::endl;
      }
    }
  }
  std::cerr << "No compatible paths found." << std::endl;
}

extern "C" {
void maxSolveSymbolicHandler(va_list args) {
  char *name = va_arg(args, char *);

  initAdversarialModule();
  solvePath(name);
  fixedInputs.clear();
  varInputs.clear();
}

void maxInputFixedHandler(va_list args) {
  void *var = va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  fixedInputs[std::string() + SPA_MESSAGE_INPUT_PREFIX + name].first = var;
  fixedInputs[std::string() + SPA_MESSAGE_INPUT_PREFIX + name].second = size;
}

void maxInputVarHandler(va_list args) {
  void *var = va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  varInputs[std::string() + SPA_MESSAGE_INPUT_PREFIX + name].first = var;
  varInputs[std::string() + SPA_MESSAGE_INPUT_PREFIX + name].second = size;
}

void spa_msg_input_handler(va_list args) {
  //   uint8_t *var = (uint8_t *)va_arg(args, void *);
  //   size_t size = va_arg(args, size_t);
  //   const char *name = va_arg(args, const char *);
}

void spa_tag_handler(va_list args) {
  //   const char *key = va_arg(args, const char *);
  //   const char *value = va_arg(args, const char *);
}
}
