#include <cstdlib>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <spa/spaRuntimeImpl.h>
#include <signal.h>

void handleSignal(int signum) {
  std::cerr << "Flushing GCOV data." << std::endl;
  exit(-1);
}

bool signalRegistered = (signal(SIGUSR1, handleSignal) != SIG_ERR);

void handle_input(uint8_t *var, size_t size, const char *name) {
  char *value = getenv(name);
  // 	assert( varValue && "Variable not defined in environment." );
  if (!value) {
    std::cerr << "Variable " << name << "[" << size
              << "] not defined; leaving value unmodified." << std::endl;
    return;
  }

  std::cerr << "Injecting value for "
            << name /*<< "[" << size << "] = " << value*/ << std::endl;

  std::stringstream ss(value);

  while (ss.good() && !ss.bad()) {
    assert(size-- > 0 && "Variable size mismatch.");
    unsigned int n;
    ss >> std::hex >> n;
    // 		std::cerr << n << " ";
    *(var++) = n;
  }
  // 	std::cerr << std::endl;
  assert(size == 0 && "Variable size mismatch.");
}

std::ostream &log() {
  assert(signalRegistered && "Unable to register signal to flush GCOV data.");

  static std::ofstream logFile;

  if (!logFile.is_open()) {
    const char *filename = getenv("SPA_LOG_FILE");
    // 		assert( filename && "SPA_LOG_FILE not defined in environment." );
    if (filename == NULL) {
      std::cerr
          << "SPA_LOG_FILE not defined in environment. Logging to /dev/null."
          << std::endl;
      filename = "/dev/null";
    }

    logFile.open(filename, std::ios_base::out | std::ios_base::app);
    assert(logFile.is_open() && "Unable to open SPA log file.");
  }

  return logFile;
}

void handle_output(uint8_t *var, size_t size, const char *name) {
  std::cerr << "Logging value for " << name << std::endl;

  log() << name;
  for (; size > 0; var++, size--)
    log() << " " << std::hex << (int) * var;
  log() << std::endl;
}

extern "C" {
void spa_api_input_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  handle_input(var, size, name);
}

void spa_state_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  handle_input(var, size, name);
}

void spa_api_output_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  size_t maxSize = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  handle_output(var, size, name);
}

void spa_msg_input_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  handle_input(var, size, name);
}

void spa_msg_input_size_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  handle_input(var, size, name);
}

void spa_msg_output_handler(va_list args) {
  uint8_t *var = (uint8_t *)va_arg(args, void *);
  size_t size = va_arg(args, size_t);
  size_t maxSize = va_arg(args, size_t);
  const char *name = va_arg(args, const char *);

  handle_output(var, size, name);
}

void spa_tag_handler(va_list args) {
  const char *key = va_arg(args, const char *);
  const char *value = va_arg(args, const char *);

  std::cerr << "Logging tag " << key << " = " << value << std::endl;
  log() << key << " " << value << std::endl;
}

void spa_valid_path_handler(va_list args) {
  std::cerr << "Valid path." << std::endl;
//   exit(200);
}

void spa_invalid_path_handler(va_list args) {
  std::cerr << "Invalid path." << std::endl;
//   exit(201);
}
}
