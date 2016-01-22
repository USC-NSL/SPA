/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>
#include <sstream>
#include <fstream>

#include <llvm/IR/Instruction.h>
#include <llvm/DebugInfo.h>

#include <spa/Path.h>

namespace SPA {
template <typename T> std::string numToStr(T n) {
  std::stringstream ss;
  ss << n;
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
           numToStr<long>(loc.getLineNumber());
  } else {
    return "(unknown)";
  }
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

}

#endif // #ifndef __UTIL_H__
