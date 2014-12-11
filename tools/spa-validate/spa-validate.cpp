#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cctype>
#include <fstream>
#include <chrono>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

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

#define RECHECK_WAIT 100000 // us

class Action;

std::vector<Action *> actions;
llvm::OwningPtr<SPA::FilterExpression> checkExpression;
std::chrono::duration<long, std::milli> timeout;
std::chrono::time_point<std::chrono::system_clock> watchdog;
std::set<pid_t> waitPIDs;

class Action {
public:
  virtual void run(SPA::Path *path) = 0;
  virtual ~Action() {}
};

class Condition {
public:
  virtual bool check() = 0;
  virtual ~Condition() {}
};

class RunAction : public Action {
private:
  std::string command;

public:
  RunAction(std::string command) : command(command) {}

  void run(SPA::Path *path) {
    pid_t pid = fork();
    assert(pid >= 0 && "Error forking process.");
    if (pid == 0) {
      setpgid(0, 0);
      exit(system(command.c_str()));
    }
    klee::klee_message("Running: %s [%d]", command.c_str(), pid);
    waitPIDs.insert(pid);
  }
};

class WaitAction : public Action {
private:
  llvm::OwningPtr<Condition> condition;

public:
  WaitAction(Condition *condition) : condition(condition) {}

  void run(SPA::Path *path) {
    while ((timeout.count() == 0 || std::chrono::system_clock::now() <=
                                        watchdog) && !condition->check()) {
      usleep(RECHECK_WAIT);
    }
  }
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
  bool check() {
    klee::klee_message("Waiting for server to listen on %s port %d.",
                       connTable.substr(connTable.rfind("/") + 1).c_str(),
                       port);

    std::ifstream table(connTable);
    assert(table.is_open());
    std::string line;
    std::getline(table, line); // Ignore table header.
    while (table.good()) {
      std::getline(table, line);
      if (!line.empty()) {
        uint16_t local_port, state;

        std::istringstream tokenizer(line);
        std::string token;
        tokenizer >> token >> token;
        std::istringstream hexconverter(token.substr(token.find(':') + 1));
        hexconverter >> std::hex >> local_port;
        tokenizer >> token >> std::hex >> state;

        if (local_port == port && (listenState == 0 || state == listenState))
          return true;
      }
    }
    return false;
  }
};

class ProcsDoneCondition : public Condition {
public:
  bool check() {
    klee::klee_message("Waiting for %ld processes to terminate.",
                       waitPIDs.size());

    for (auto pid : waitPIDs) {
      int status;
      assert(waitpid(pid, &status, WNOHANG) != -1);
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        klee::klee_message("Process %d terminated.", pid);
        waitPIDs.erase(pid);
      }
    }

    return waitPIDs.empty();
  }
};

class TimedCondition : public Condition {
private:
  std::chrono::duration<long, std::milli> duration;
  std::chrono::time_point<std::chrono::system_clock> expire_time;

public:
  TimedCondition(uint64_t duration_ms) : duration(duration_ms) {}
  bool check() {
    if (expire_time.time_since_epoch().count() == 0) {
      expire_time = std::chrono::system_clock::now() + duration;
      return false;
    } else {
      return std::chrono::system_clock::now() > expire_time;
    }
  }
};

void waitFinish() {
  do {
    klee::klee_message("Waiting for %ld processes to finish.", waitPIDs.size());

    while ((!waitPIDs.empty()) &&
           (timeout.count() == 0 ||
            std::chrono::system_clock::now() <= watchdog)) {
      pid_t pid = *waitPIDs.begin();
      int status;
      assert(waitpid(pid, &status, WNOHANG) != -1);
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        klee::klee_message("Process %d terminated.", pid);
        waitPIDs.erase(pid);
      } else {
        usleep(RECHECK_WAIT);
      }
    }

    if (!waitPIDs.empty()) {
      for (pid_t pid : waitPIDs) {
        klee::klee_message("Killing process %d.", pid);
        kill(-pid, SIGKILL);
      }

      usleep(RECHECK_WAIT);
    }
  } while (!waitPIDs.empty());
}

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
    std::string cmd = Commands.substr(pos, Commands.find(";", pos) - pos);
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
      actions.push_back(new RunAction(cmd.substr(cmd.find(' '))));
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
      } else if (args[1] == "DONE") {
        condition = new ProcsDoneCondition();
      } else if (std::all_of(args[1].begin(), args[1].end(), isdigit)) {
        condition = new TimedCondition(atol(args[1].c_str()));
      } else {
        assert(false && "Invalid WAIT command.");
      }

      actions.push_back(new WaitAction(condition));
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
      watchdog = std::chrono::system_clock::now() + timeout;
    }

    for (auto action : actions) {
      if (timeout.count() && std::chrono::system_clock::now() > watchdog)
        break;
      action->run(path);
    }
    waitFinish();

    klee::klee_message("Checking validity condition.");
    if (checkExpression->check(path)) {
      klee::klee_message("Path valid. Outputting.");
      outFile << path;
    } else {
      klee::klee_message("Path invalid. Dropping.");
    }
  }

  return 0;
}
