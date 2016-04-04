/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>
#include <sstream>
#include <fstream>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <semaphore.h>

#include <llvm/IR/Instruction.h>
#include <llvm/DebugInfo.h>

#include "../../lib/Core/Common.h"
#include <spa/Path.h>

namespace SPA {
std::string __attribute__((weak)) numToStr(long long n) {
  std::stringstream ss;
  ss << n;
  return ss.str();
}

std::string __attribute__((weak)) numToStrHex(long long n) {
  std::stringstream ss;
  ss << std::hex << n;
  return ss.str();
}

template <typename T> T strToNum(const std::string &s) {
  std::stringstream ss(s);
  T result;
  return ss >> result ? result : 0;
}

std::string __attribute__((weak)) debugLocation(const llvm::Instruction *inst) {
  if (llvm::MDNode *node = inst->getMetadata("dbg")) {
    llvm::DILocation loc(node);
    return loc.getDirectory().str() + "/" + loc.getFilename().str() + ":" +
           numToStr(loc.getLineNumber());
  } else {
    return "(unknown)";
  }
}

std::string __attribute__((weak))
    strJoin(std::vector<std::string> strings, std::string delimiter) {
  std::string result;
  for (auto it = strings.begin(), ie = strings.end(); it != ie; it++) {
    result += (it == strings.begin() ? "" : delimiter) + *it;
  }
  return result;
}

std::string __attribute__((weak))
    strSplitJoin(std::string input, std::string delimiter, int lidx,
                 unsigned int rcut) {
  if (lidx > 0) {
    while (lidx-- > 0) {
      auto pos = input.find(delimiter);
      assert(pos != std::string::npos && "Index out of bounds.");
      input = input.substr(pos + delimiter.length());
    }
  } else if (lidx < 0) {
    auto pos = input.length();
    while (lidx++ < 0) {
      pos = input.rfind(delimiter, pos - 1);
      assert(pos != std::string::npos && "Index out of bounds.");
    }
    input = input.substr(pos + delimiter.length());
  }
  while (rcut-- > 0) {
    auto pos = input.rfind(delimiter);
    assert(pos != std::string::npos && "Index out of bounds.");
    input = input.substr(0, pos);
  }
  return input;
}

bool __attribute__((weak))
    pathPrefixMatch(std::string longPath, std::string shortPath) {
  auto delim = longPath.rfind("/");
  std::string longDir =
      delim == std::string::npos ? "" : longPath.substr(0, delim);
  std::string longFile =
      delim == std::string::npos ? longPath : longPath.substr(delim + 1);
  delim = shortPath.rfind("/");
  std::string shortDir =
      delim == std::string::npos ? "" : shortPath.substr(0, delim);
  std::string shortFile =
      delim == std::string::npos ? shortPath : shortPath.substr(delim + 1);

  // Exact match file.
  // shortDir must be a suffix of longDir, with care that it comes right
  // after a / or matches exactly.
  return shortFile == longFile && longDir.length() >= shortDir.length() &&
         longDir.substr(longDir.length() - shortDir.length()) == shortDir &&
         (shortDir == "" || shortDir.length() == longDir.length() ||
          longDir[longDir.length() - shortDir.length() - 1] == '/');
}

std::string __attribute__((weak)) generateUUID() {
  std::ifstream ifs("/proc/sys/kernel/random/uuid");
  assert(ifs.good() && "Unable to open UUID generation file.");

  std::string uuid;
  std::getline(ifs, uuid);
  return uuid;
}

bool __attribute__((weak))
    checkMessageCompatibility(std::shared_ptr<Symbol> senderSymbol,
                              std::string receiverLocalName) {
  std::string receiverPrefix =
      strSplitJoin(receiverLocalName, SPA_SYMBOL_DELIMITER, 2, 1);
  std::string receiver5Tuple =
      strSplitJoin(receiverLocalName, SPA_SYMBOL_DELIMITER, -1, 0);
  std::string receiverConnectIP = strSplitJoin(receiver5Tuple, ".", 0, 7);
  std::string receiverConnectPort = strSplitJoin(receiver5Tuple, ".", 4, 6);
  std::string receiverProtocol = strSplitJoin(receiver5Tuple, ".", 5, 5);
  std::string receiverBindIP = strSplitJoin(receiver5Tuple, ".", -5, 1);
  std::string receiverBindPort = strSplitJoin(receiver5Tuple, ".", -1, 0);

  if (receiverPrefix == senderSymbol->getPrefix() &&
      receiverBindIP == senderSymbol->getMessageDestinationIP() &&
      receiverProtocol == senderSymbol->getMessageProtocol() &&
      receiverBindPort == senderSymbol->getMessageDestinationPort()) {
    if (receiverConnectIP == SPA_ANY_IP &&
        receiverConnectPort == SPA_ANY_PORT) {
      return true;
    } else {
      return receiverConnectIP == senderSymbol->getMessageSourceIP() &&
             receiverConnectPort == senderSymbol->getMessageSourcePort();
    }
  } else {
    return false;
  }
}

std::string __attribute__((weak))
    runCommand(std::string command, std::string input) {
  int pipe_to_cmd[2], pipe_from_cmd[2];
  assert(pipe(pipe_to_cmd) == 0);
  assert(pipe(pipe_from_cmd) == 0);

  int id = shmget(IPC_PRIVATE, sizeof(sem_t), (S_IRUSR | S_IWUSR));
  assert(id != -1 && "Failed to create shared memory for semaphore.");
  sem_t *sem = (sem_t *)shmat(id, NULL, 0);
  assert(sem != (void *)- 1 &&
         "Failed to attach memory segment for semaphore.");
  assert(sem_init(sem, 1 /*shared across processes*/, 0) != -1 &&
         "Failed to initialize semaphore");

  pid_t pid = fork();
  assert(pid != -1 && "Unable to fork child process.");
  if (pid == 0) {
    assert(close(pipe_to_cmd[1]) == 0);
    assert(dup2(pipe_to_cmd[0], 0) >= 0);
    assert(close(pipe_from_cmd[0]) == 0);
    assert(dup2(pipe_from_cmd[1], 1) >= 0);

    assert(sem_post(sem) != -1 && "Failed to unlock semaphore.");

    assert(execl("/bin/sh", "sh", "-c", command.c_str(), NULL) == 0);
    exit(-1);
  }

  // Wait for child process to start.
  while (sem_wait(sem) == -1) {
    assert(errno == EINTR && "Failed to lock semaphore.");
  }
  assert(shmdt((void *)sem) == 0 &&
         "Failed to destroy semaphore shared memory.");

  assert(close(pipe_to_cmd[0]) == 0);
  assert(close(pipe_from_cmd[1]) == 0);
  assert(write(pipe_to_cmd[1], input.c_str(), input.length()) == (long)
         input.length());
  assert(close(pipe_to_cmd[1]) == 0);

  char c;
  std::string output;
  while (read(pipe_from_cmd[0], &c, 1) == 1) {
    output += c;
  }
  assert(close(pipe_from_cmd[0]) == 0);
  int status;
  assert(waitpid(pid, &status, 0) == pid);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

  return output;
}

pid_t __attribute__((weak)) forkWorker(unsigned long maxWorkers) {
  static unsigned long numWorkers = 0;
  static bool isWorker = false;

  assert((!isWorker) && "Workers can not launch other workers.");

  if (maxWorkers <= 1) {
    return 0;
  }

  while (numWorkers >= maxWorkers) {
    int status;
    assert(waitpid(0, &status, 0) > 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    numWorkers--;
  }

  pid_t pid = fork();
  assert(pid >= 0);
  if (pid == 0) {
    isWorker = true;
  } else {
    numWorkers++;
  }

  return pid;
}

}

#endif // #ifndef __UTIL_H__
