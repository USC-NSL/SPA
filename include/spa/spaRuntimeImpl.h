#include <spa/spaRuntime.h>

extern "C" {
void klee_make_symbolic(void *addr, size_t nbytes, const char *name) {}

void spa_runtime_call(SpaRuntimeHandler_t handler, ...) {
  va_list args;
  va_start(args, handler);
  handler(args);
  va_end(args);
}
}
