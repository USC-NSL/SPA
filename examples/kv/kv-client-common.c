#include <spa/spaRuntime.h>

#include "kv.h"

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    char *d;
    if ((d = strstr(argv[i], "==")) != NULL) {
      key_t key = argv[i][0];
      value_t expected_value = *(d + 2);

      value_t actual_value = kv_get(key);
      printf("%c == %c / %c\n", key, expected_value, actual_value);
      assert(expected_value == actual_value);
    } else if ((d = strstr(argv[i], "=")) != NULL) {
      *d = '\0';
      key_t key = argv[i][0];
      value_t value = *(d + 1);

      printf("%c = %c\n", key, value);
      kv_set(key, value);
    } else {
      printf("%c == %c\n", argv[i][0], kv_get(argv[i][0]));
    }
  }
  return 0;
}

#define SPA_KEY 'k'
typedef enum {
  GET,
  SET
} op_t;
typedef struct {
  char op;
  value_t value;
} operation_t;

void __attribute__((noinline, weak)) kv_done() {
  // Complicated NOP to prevent inlining.
  static uint8_t i = 0;
  i++;
}

void __attribute__((noinline, used)) spa_entry() {
  operation_t operations[1];

  spa_api_input_var(operations);

  for (int i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
    switch (operations[i].op) {
    case GET: {
      kv_get(SPA_KEY);
    } break;
    case SET: {
      value_t value;
      value = operations[i].value;
      kv_set(SPA_KEY, value);
    } break;
    default:
      return;
      break;
    }
  }

  kv_done();
}
