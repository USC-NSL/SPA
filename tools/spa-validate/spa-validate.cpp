#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <cctype>
#include <fstream>
#include <chrono>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ftw.h>

#include <llvm/ADT/OwningPtr.h>
#include <llvm/Support/CommandLine.h>
#include "../../lib/Core/Common.h"
#include <spa/FilterExpression.h>
#include <spa/PathLoader.h>
#include <spa/SPA.h>
#include <spa/Path.h>
#include <spa/Util.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

namespace {
llvm::cl::opt<bool> EnableDbg("d", llvm::cl::init(false),
                              llvm::cl::desc("Output debug information."));

llvm::cl::opt<bool> Follow("f", llvm::cl::init(false),
                           llvm::cl::desc("Follow inputs file."));

llvm::cl::opt<std::string> gcnoDirMap(
    "gcno-dir-map",
    llvm::cl::desc(
        "Map one directory to another when searching for GCNO files."));

llvm::cl::opt<std::string> InFileName(llvm::cl::Positional, llvm::cl::Required,
                                      llvm::cl::desc("<input path-file>"));

llvm::cl::opt<std::string> OutFileName(llvm::cl::Positional, llvm::cl::Required,
                                       llvm::cl::desc("<output path-file>"));

llvm::cl::opt<std::string>
    FPOutFileName(llvm::cl::Positional, llvm::cl::Optional,
                  llvm::cl::desc("[output false-positive path-file]"));

llvm::cl::opt<std::string> Commands(llvm::cl::Positional, llvm::cl::Required,
                                    llvm::cl::desc("<validation commands>"));
}

#define RECHECK_WAIT 100000 // us
#define FOLLOW_WAIT 1000000 // us
#define GCOV_DIR_TEMPLATE "/tmp/spa-validate-XXXXXX"

// Signals to try on a process we're trying to kill.
int signals[] = { SIGUSR1, SIGINT, SIGTERM, SIGKILL };

class Action;
bool waitFinish(bool wait, bool terminate, pid_t pid = 0);

std::vector<Action *> actions;
llvm::OwningPtr<SPA::FilterExpression> checkExpression;
std::chrono::duration<long, std::milli> timeout;
std::chrono::time_point<std::chrono::system_clock> watchdog;
char gcovDir[] = GCOV_DIR_TEMPLATE;
std::string gcovFiles;
std::map<std::string, std::map<long, bool> > gcovLineCoverage;
std::map<std::string, bool> gcovFunctionCoverage;
std::set<pid_t> waitPIDs;
std::vector<pid_t> runPIDs;

class Action {
public:
  virtual bool run(SPA::Path *path) = 0;
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

  bool run(SPA::Path *path) {
    if (path->getTestInputs().empty()) {
      klee::klee_message("Path has no test-case.");
      return false;
    }

    pid_t pid = fork();
    assert(pid >= 0 && "Error forking process.");
    if (pid == 0) {
      setpgid(0, 0);

      // Set up test-case environment.
      if (EnableDbg) {
        klee::klee_message("Test inputs:");
      }
      for (auto variable : path->getTestInputs()) {
        // Only inject API inputs.
        if (variable.first.compare(0, strlen(SPA_API_INPUT_PREFIX),
                                   SPA_API_INPUT_PREFIX) == 0) {
          std::stringstream ss;
          for (auto b : variable.second) {
            ss << " " << std::hex << (int) b;
          }
          if (EnableDbg) {
            klee::klee_message("  %s = %s", variable.first.c_str(),
                               ss.str().substr(1).c_str());
          }
          setenv(variable.first.c_str(), ss.str().substr(1).c_str(), 1);
        }
      }
      // Set up GCOV environment.
      setenv("GCOV_PREFIX", gcovDir, 1);

      exit(system(command.c_str()));
    }
    klee::klee_message("Running: %s [%d]", command.c_str(), pid);
    waitPIDs.insert(pid);
    runPIDs.push_back(pid);
    return true;
  }
};

class KillAction : public Action {
private:
  int procNum;

public:
  KillAction(int procNum = -1) : procNum(procNum) {}

  bool run(SPA::Path *path) {
    if (procNum >= 0) {
      klee::klee_message("Killing process %d [%d].", procNum + 1,
                         runPIDs[procNum]);
      waitFinish(false, true, runPIDs[procNum]);
    } else {
      klee::klee_message("Killing all process.");
      for (auto pid : runPIDs)
        waitFinish(false, true, pid);
    }
    return true;
  }
};

class WaitAction : public Action {
private:
  llvm::OwningPtr<Condition> condition;

public:
  WaitAction(Condition *condition) : condition(condition) {}

  bool run(SPA::Path *path) {
    while ((timeout.count() == 0 || std::chrono::system_clock::now() <=
                                        watchdog) && !condition->check()) {
      usleep(RECHECK_WAIT);
    }
    return true;
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
private:
  int procNum;

public:
  ProcsDoneCondition(int procNum = -1) : procNum(procNum) {}

  bool check() {
    if (procNum >= 0) {
      klee::klee_message("Waiting for process %d [%d] to end.", procNum + 1,
                         runPIDs[procNum]);
      return waitFinish(false, false, runPIDs[procNum]);
    } else {
      klee::klee_message("Waiting for all processes to end.");
      return waitFinish(false, false);
    }
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
      klee::klee_message("Waiting for %ld ms.", duration.count());
      expire_time = std::chrono::system_clock::now() + duration;
      return false;
    } else {
      return std::chrono::system_clock::now() > expire_time;
    }
  }
};

bool waitFinishPid(pid_t pid) {
  int status;
  assert((pid = waitpid(pid, &status, WNOHANG)) != -1);
  if (pid > 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
    if (EnableDbg) {
      klee::klee_message("Process %d terminated.", pid);
    }
    waitPIDs.erase(pid);
    return true;
  } else {
    return false;
  }
}

bool waitFinish(bool wait, bool terminate, pid_t pid) {
  uint8_t sig_counter = 0;
  do {
    if (EnableDbg && wait) {
      klee::klee_message("Waiting for %ld processes to finish.",
                         pid ? 1 : waitPIDs.size());
    }

    do {
      if (pid) {
        if (!waitFinishPid(pid)) {
          usleep(RECHECK_WAIT);
        }
      } else {
        for (auto pid : waitPIDs) {
          if (!waitFinishPid(pid)) {
            usleep(RECHECK_WAIT);
          }
        }
      }
    } while ((pid ? waitPIDs.count(pid) : !waitPIDs.empty()) &&
             (wait && (timeout.count() == 0 ||
                       std::chrono::system_clock::now() <= watchdog)));

    if (terminate && (pid ? waitPIDs.count(pid) : !waitPIDs.empty())) {
      if (pid) {
        if (EnableDbg) {
          klee::klee_message("Killing process %d with signal %s.", pid,
                             strsignal(signals[sig_counter]));
        }
        assert(kill(-pid, signals[sig_counter]) == 0);
      } else {
        for (pid_t pid : waitPIDs) {
          if (EnableDbg) {
            klee::klee_message("Killing process %d with signal %s.", pid,
                               strsignal(signals[sig_counter]));
          }
          assert(kill(-pid, signals[sig_counter]) == 0);
        }
      }

      if (sig_counter < sizeof(signals) / sizeof(signals[0]) - 1)
        sig_counter++;

      usleep(RECHECK_WAIT);
    }
  } while (terminate && (pid ? waitPIDs.count(pid) : !waitPIDs.empty()));

  return pid ? waitPIDs.count(pid) == 0 : waitPIDs.empty();
}

int gcovGcnaWalker(const char *fpath, const struct stat *sb, int typeflag) {
  std::string fpathstr = fpath;

  if (typeflag != FTW_F || fpathstr.substr(fpathstr.rfind(".")) != ".gcda")
    return FTW_CONTINUE;

  if (EnableDbg) {
    klee::klee_message("Found GCOV data file: %s", fpath);
  }
  std::string basePath = fpathstr.substr(strlen(gcovDir));
  basePath = basePath.substr(0, basePath.rfind("."));
  std::string dirPath = fpathstr.substr(0, fpathstr.rfind("/"));
  std::string gcnoPath = basePath + ".gcno";
  if (!gcnoDirMap.empty()) {
    auto delim = gcnoDirMap.find("=");
    assert(delim != std::string::npos);
    std::string lvalue = gcnoDirMap.substr(0, delim);
    std::string rvalue = gcnoDirMap.substr(delim + 1);
    if (gcnoPath.substr(0, lvalue.size()) == lvalue) {
      gcnoPath = rvalue + gcnoPath.substr(lvalue.size());
    }
  }
  gcovFiles += std::string(" ") + basePath;

  if (EnableDbg) {
    klee::klee_message("Copying %s to %s", gcnoPath.c_str(), dirPath.c_str());
  }
  assert(system((std::string("cp ") + gcnoPath + " " + dirPath).c_str()) == 0);

  return FTW_CONTINUE;
}

#define SOURCE_KEY_PREFIX "Source:"
#define FUNCTION_SUMMARY_PREFIX "function "
#define FUNCTION_SUMMARY_CALLED_PREFIX " called "
int gcovGcovWalker(const char *fpath, const struct stat *sb, int typeflag) {
  std::string fpathstr = fpath;

  if (typeflag != FTW_F || fpathstr.substr(fpathstr.rfind(".")) != ".gcov")
    return FTW_CONTINUE;

  if (EnableDbg) {
    klee::klee_message("Found GCOV annotated source file: %s", fpath);
  }

  std::string srcFile;

  std::ifstream ifs(fpath);
  std::string line;
  while (getline(ifs, line)) {
    // LTrim
    line = line.substr(line.find_first_not_of(" "));

    if (line.substr(0, strlen(FUNCTION_SUMMARY_PREFIX)) ==
        FUNCTION_SUMMARY_PREFIX) {
      auto fnPos = strlen(FUNCTION_SUMMARY_PREFIX);
      std::string fn = line.substr(fnPos, line.find(" ", fnPos) - fnPos);
      auto numCallsPos = line.find(FUNCTION_SUMMARY_CALLED_PREFIX) +
                         strlen(FUNCTION_SUMMARY_CALLED_PREFIX);
      long numCalls = SPA::strToNum<long>(
          line.substr(numCallsPos, line.find(" ", numCallsPos) - numCallsPos));
      gcovFunctionCoverage[fn] = (numCalls > 0);
    } else if (isdigit(line[0]) || line[0] == '-' || line[0] == '#') {
      auto countLineDelim = line.find(":");
      auto lineSrcDelim = line.find(":", countLineDelim + 1);
      long count =
          line[0] == '-' ? -1
                         : SPA::strToNum<long>(line.substr(0, countLineDelim));
      long lineNum = SPA::strToNum<long>(
          line.substr(countLineDelim + 1, lineSrcDelim - countLineDelim - 1));
      std::string src = line.substr(lineSrcDelim + 1);

      if (lineNum == 0) {
        if (src.substr(0, strlen(SOURCE_KEY_PREFIX)) == SOURCE_KEY_PREFIX) {
          srcFile = src.substr(strlen(SOURCE_KEY_PREFIX));
        }
      } else {
        if (count >= 0) {
          assert((!srcFile.empty()) &&
                 "Source file not specified in GCOV file.");
          gcovLineCoverage[srcFile][lineNum] = (count > 0);
        }
      }
    }
  }
  if (EnableDbg) {
    klee::klee_message(
        "Covered %ld lines in %s.",
        gcovLineCoverage.count(srcFile) ? gcovLineCoverage[srcFile].size() : 0,
        srcFile.c_str());
  }

  return FTW_CONTINUE;
}

void loadCoverage(SPA::Path *path) {
  klee::klee_message("Loading GCOV data.");
  gcovFiles = "";
  gcovLineCoverage.clear();
  gcovFunctionCoverage.clear();
  assert(ftw(gcovDir, gcovGcnaWalker, 10) == 0);

  auto cwd = getcwd(NULL, 0);
  std::string cmd = std::string() + "cd " + gcovDir + "; gcov -b -o " +
                    gcovDir + cwd + gcovFiles;
  if (EnableDbg) {
    klee::klee_message("Running: %s", cmd.c_str());
  } else {
    cmd += " >/dev/null 2>/dev/null";
  }
  assert(system(cmd.c_str()) == 0);
  free(cwd);

  assert(ftw(gcovDir, gcovGcovWalker, 10) == 0);
  if (EnableDbg) {
    klee::klee_message("Covered %ld functions.", gcovFunctionCoverage.size());
  }

  path->testLineCoverage = gcovLineCoverage;
  path->testFunctionCoverage = gcovFunctionCoverage;
}

int main(int argc, char **argv, char **envp) {
  // Fill up every global cl::opt object declared in the program
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Systematic Protocol Analysis - Validate");

  std::ifstream inFile(InFileName);
  assert(inFile.is_open() && "Unable to open input path-file.");

  std::ofstream outFile(OutFileName);
  assert(outFile.is_open() && "Unable to open output path-file.");

  std::ofstream fpOutFile;
  if (!FPOutFileName.empty()) {
    fpOutFile.open(FPOutFileName);
    assert(fpOutFile.is_open() &&
           "Unable to open false-positive output path-file.");
  }

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
    } else if (args[0] == "KILL") {
      if (args.size() == 1) {
        actions.push_back(new KillAction());
      } else if (args.size() == 2) {
        actions.push_back(new KillAction(atoi(args[1].c_str()) - 1));
      } else {
        assert(false && "KILL only accepts 0 or 1 arguments.");
      }
    } else if (args[0] == "WAIT") {
      Condition *condition;
      assert(args.size() > 1 && "Wrong number of arguments in WAIT.");
      if (args[1] == "LISTEN") {
        assert(args.size() == 4 && "Wrong number of arguments in WAIT LISTEN.");
        int port = atoi(args[3].c_str());

        if (args[2] == "TCP") {
          condition = new ListenCondition(TCP_CONN_TABLE, port, TCP_LISTEN);
        } else if (args[2] == "UDP") {
          condition = new ListenCondition(UDP_CONN_TABLE, port);
        } else {
          assert(false && "Unknown protocol to listen for.");
        }
      } else if (args[1] == "DONE") {
        if (args.size() == 2) {
          condition = new ProcsDoneCondition();
        } else if (args.size() == 3) {
          condition = new ProcsDoneCondition(atoi(args[2].c_str()) - 1);
        } else {
          assert(false && "WAIT DONE only accepts 0 or 1 arguments.");
        }
      } else if (args[1] == "TIME") {
        assert(args.size() == 3 && "Wrong number of arguments in WAIT TIME.");
        condition = new TimedCondition(SPA::strToNum<long>(args[1]));
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
      assert(args.size() == 2 && "Wrong number of arguments in TIMEMOUT.");
      timeout =
          std::chrono::duration<long, std::milli>(SPA::strToNum<long>(args[1]));
    } else {
      assert(false && "Unknown command.");
    }
  } while ((pos = Commands.find(";", pos)) != std::string::npos);

  if (!checkExpression) {
    klee::klee_message("No check expression specified. Accepting all paths.");
    checkExpression.reset(new SPA::ConstFE(true));
  }

  if (EnableDbg) {
    klee::klee_message("Final expression: %s",
                       checkExpression->dbg_str().c_str());
  }

  SPA::PathLoader pathLoader(inFile);
  std::set<std::map<std::string, std::vector<uint8_t> > > processedTestCases;
  llvm::OwningPtr<SPA::Path> path;
  while (path.reset(pathLoader.getPath()), path || Follow) {
    if (!path) {
      klee::klee_message("Waiting for new input paths.");
      usleep(FOLLOW_WAIT);
      continue;
    }

    klee::klee_message("Processing path.");

    if (processedTestCases.count(path->getTestInputs())) {
      klee::klee_message("Found repeated test-case. Dropping path.");
      continue;
    }
    processedTestCases.insert(path->getTestInputs());

    if (timeout.count()) {
      watchdog = std::chrono::system_clock::now() + timeout;
    }

    // Set up gcov working directory.
    strncpy(gcovDir, GCOV_DIR_TEMPLATE, sizeof(gcovDir));
    gcovDir[sizeof(gcovDir) - 1] = '\0';
    assert(mkdtemp(gcovDir));
    if (EnableDbg) {
      klee::klee_message("Storing GCOV data in: %s", gcovDir);
    }

    bool skipPath = false;
    for (auto action : actions) {
      if (timeout.count() && std::chrono::system_clock::now() > watchdog) {
        klee::klee_message("Path processing timed out.");
        break;
      }
      if (!action->run(path.get())) {
        klee::klee_message("Action failed. Dropping path.");
        skipPath = true;
        break;
      }
    }
    waitFinish(true, true);

    if (!skipPath) {
      loadCoverage(path.get());

      klee::klee_message("Checking validity condition.");
      if (checkExpression->checkPath(*path.get())) {
        klee::klee_message("Path valid. Outputting as true-positive.");
        outFile << *path;
      } else {
        if (fpOutFile.is_open()) {
          klee::klee_message("Path invalid. Outputting as false-positive.");
          fpOutFile << *path;
        } else {
          klee::klee_message("Path invalid. Dropping.");
        }
      }
    }

    klee::klee_message("Cleaning up.");
    assert(system((std::string("rm -rf ") + gcovDir).c_str()) == 0);

    assert(waitPIDs.empty());
    runPIDs.clear();
  }

  return 0;
}
