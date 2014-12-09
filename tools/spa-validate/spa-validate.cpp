#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cctype>
#include <fstream>
#include <chrono>

#include <llvm/ADT/OwningPtr.h>
#include <llvm/Support/CommandLine.h>
#include "../../lib/Core/Common.h"
#include <spa/FilterExpression.h>
#include <spa/PathLoader.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<bool> EnableDbg("d", llvm::cl::init(false),
                              llvm::cl::desc("Output debug information."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));

llvm::cl::opt<std::string> OutFileName(llvm::cl::Positional, llvm::cl::Required,
                                       llvm::cl::desc("<output path-file>"));

llvm::cl::opt<std::string> Commands(llvm::cl::Positional, llvm::cl::Required,
                                    llvm::cl::desc("<validation commands>"));
}

class Action {
public:
  virtual void operator()(SPA::Path *path) {
    assert(false && "Not implemented.");
  }
  virtual ~Action() {}
};

class Condition {
public:
  virtual bool operator()() = 0;
  virtual ~Condition() {}
};

class RunAction : public Action {
private:
  std::string command;

public:
  RunAction(std::string command) : command(command) {}

  //   void operator()(SPA::Path *path) { assert(false && "Not implemented."); }
};

class WaitAction : public Action {
private:
  llvm::OwningPtr<Condition> condition;

public:
  WaitAction(Condition *condition) : condition(condition) {}

  //   void operator()(SPA::Path *path) { assert(false && "Not implemented."); }
};

#define TCP_CONN_TABLE "/proc/net/tcp"
#define UDP_CONN_TABLE "/proc/net/udp"
#define ANY_CONN_STATE 0
#define TCP_LISTEN 10
class ListenCondition : public Condition {
private:
  std::string connTable;
  uint16_t port;
  uint8_t listenState;

public:
  ListenCondition(std::string connTable, uint16_t port,
                  uint8_t listenState = ANY_CONN_STATE)
      : connTable(connTable), port(port), listenState(listenState) {}
  bool operator()() { assert(false && "Not implemented."); }
};

class TimedCondition : public Condition {
private:
  uint64_t duration_ms;

public:
  TimedCondition(uint64_t duration_ms) : duration_ms(duration_ms) {}
  bool operator()() { assert(false && "Not implemented."); }
};

std::vector<Action> actions;
llvm::OwningPtr<SPA::FilterExpression> checkExpression;
std::chrono::duration<long, std::milli> timeout;

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Systematic Protocol Analysis - Validate");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  std::ofstream outFile(OutFileName);
  assert(outFile.is_open() && "Unable to open output path-file.");

  size_t pos = -1;
  do {
    pos++;
    std::string cmd = Commands.substr(pos, Commands.find(";", pos));
    // Trim
    cmd.erase(0, cmd.find_first_not_of(" \n\r\t"));
    cmd.erase(cmd.find_last_not_of(" \n\r\t") + 1);

    if (cmd.empty())
      continue;

    if (EnableDbg) {
      klee::klee_message("Parsing command: %s", cmd.c_str());
    }

    std::istringstream iss(cmd);
    std::vector<std::string> args;
    copy(std::istream_iterator<std::string>(iss),
         std::istream_iterator<std::string>(), back_inserter(args));

    if (args[0] == "RUN") {
      actions.push_back(RunAction(cmd.substr(cmd.find(' '))));
    } else if (args[0] == "WAIT") {
      Condition *condition;
      if (args[1] == "LISTEN") {
        int port = atoi(args[3].c_str());

        if (args[2] == "TCP") {
          condition = new ListenCondition(TCP_CONN_TABLE, port, TCP_LISTEN);
        } else if (args[2] == "UDP") {
          condition = new ListenCondition(UDP_CONN_TABLE, port);
        } else {
          assert(false && "Unknown protocol to listen for.");
        }
      } else if (std::all_of(args[1].begin(), args[1].end(), isdigit)) {
        condition = new TimedCondition(atoi(args[2].c_str()));
      } else {
        assert(false && "Invalid WAIT command.");
      }

      actions.push_back(WaitAction(condition));
    } else if (args[0] == "CHECK") {
      SPA::FilterExpression *expr =
          SPA::FilterExpression::parse(cmd.substr(cmd.find(' ')));
      assert(expr && "Invalid CHECK expression.");

      if (checkExpression) {
        checkExpression.reset(new SPA::AndFE(checkExpression.take(), expr));
      } else {
        checkExpression.reset(expr);
      }
    } else if (args[0] == "TIMEOUT") {
      timeout = std::chrono::duration<long, std::milli>(atol(args[1].c_str()));
    } else {
      assert(false && "Unknown command.");
    }
  } while ((pos = Commands.find(";", pos)) != std::string::npos);

  SPA::PathLoader pathLoader(inFile);
  SPA::Path *path;
  while ((path = pathLoader.getPath())) {
    if (timeout.count()) {
      assert(false && "Not implemented.");
      //       std::chrono::time_point<std::chrono::system_clock> watchdog =
      //           std::chrono::system_clock::now() + timeout;
    }

    for (auto action : actions)
      action(path);

    klee::klee_message("Checking validity condition.");
    if (checkExpression->check(path)){
      klee::klee_message("Path valid. Outputting.");
      outFile << path;
    } else {
      klee::klee_message("Path invalid. Dropping.");
    }
  }

  return 0;
}
