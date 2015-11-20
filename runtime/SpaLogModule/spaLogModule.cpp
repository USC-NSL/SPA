#include <cstdlib>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <spa/spaRuntimeImpl.h>

unsigned long eventID = 0;
std::map<std::string, std::pair<void *, size_t> > stateVars;
std::map<std::string, std::string> tags;

std::ostream &log() {
  static std::ofstream logFile;

  if (!logFile.is_open()) {
    char *filename = getenv("SPA_LOG_FILE");
    assert(filename && "SPA_LOG_FILE not defined in environment.");

    logFile.open(filename, std::ios_base::out | std::ios_base::app);
    assert(logFile.is_open() && "Unable to open SPA log file.");
  }

  return logFile;
}

std::string escapeChar(char c) {
  if (c == '\n')
    return "\\n";
  if (c == '\r')
    return "\\r";
  if (c == '\\')
    return "\\\\";
  if (c == '\n')
    return "\\n";
  if (c == '\'')
    return "\\'";
  if (c == '\"')
    return "\\\"";
  if (isprint(c))
    return std::string(1, c);

  char s[5];
  snprintf(s, sizeof(s), "\\x%x", c);
  return s;
}

void outputVar(std::string name, uint8_t *var, size_t size) {
  log() << name;
  for (size_t i = 0; i < size; i++)
    log() << " " << std::hex << (int) var[i];
  log() << std::endl;
  log() << name << " = \"";
  for (size_t i = 0; i < size; i++)
    log() << escapeChar(var[i]);
  log() << "\"" << std::endl;
}

void dumpState() {
  for (std::map<std::string, std::pair<void *, size_t> >::iterator
           it = stateVars.begin(),
           ie = stateVars.end();
       it != ie; it++) {
    std::string name = it->first;
    uint8_t *var = (uint8_t *)it->second.first;
    size_t size = it->second.second;

    outputVar(name, var, size);
  }

  for (std::map<std::string, std::string>::iterator it = tags.begin(),
                                                    ie = tags.end();
       it != ie; it++)
    log() << it->first << " = " << it->second << std::endl;
}

std::string curTime() {
  time_t t;
  time(&t);
  std::string s(ctime(&t));
  s.resize(s.length() - 1);
  return s;
}

extern "C" {
void spa_input_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  log() << "Event " << eventID++ << " - Input on " << curTime()
        << std::endl;
  outputVar(name, var, size);
  dumpState();
}

void spa_api_input_handler(va_list args) {
//   uint8_t *var = (uint8_t *)va_arg(args, void *);
//   size_t size = va_arg(args, size_t);
//   const char *name = va_arg(args, const char *);
}

void spa_state_handler(va_list args) {
  void *var = va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  assert(var && "Declared NULL state variable.");

  stateVars[name] = std::pair<void *, size_t>(var, size);
}

void spa_api_output_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  //   size_t maxSize = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  log() << "Event " << eventID++ << " - API Output on " << curTime()
        << std::endl;
  outputVar(name, var, size);
  dumpState();
}

void spa_msg_input_handler(va_list args) {
//   uint8_t *var = (uint8_t *)va_arg(args, void *);
//   size_t size = va_arg(args, size_t);
//   const char *name = va_arg(args, const char *);
}

void spa_msg_input_size_handler(va_list args) {
//   uint8_t *var = (uint8_t *)va_arg(args, void *);
//   size_t size = va_arg(args, size_t);
//   const char *name = va_arg(args, const char *);
}

void spa_msg_output_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  //   size_t maxSize = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  log() << "Event " << eventID++ << " - Message Output on " << curTime()
        << std::endl;
  outputVar(name, var, size);
  dumpState();
}

void spa_tag_handler(va_list args) {
  const char *key = va_arg(args, const char *);
  const char *value = va_arg(args, const char *);

  tags[key] = value;

  log() << "Event " << eventID++ << " - Tag " << key << " set on " << curTime()
        << std::endl;
  dumpState();
}

void spa_valid_path_handler(va_list args) {
  log() << "Event " << eventID++ << " - Valid Path on " << curTime()
        << std::endl;
  dumpState();
}

void spa_invalid_path_handler(va_list args) {
  log() << "Event " << eventID++ << " - Invalid Path on " << curTime()
        << std::endl;
  dumpState();
}
}
