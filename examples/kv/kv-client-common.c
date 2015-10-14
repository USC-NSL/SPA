#include <spa/spaRuntime.h>

#include "kv.h"

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    char *d;
    if ((d = strstr(argv[i], "==")) != NULL) {
      *d = '\0';
      char *key = argv[i];
      char *expected_value = d + 2;

      char *actual_value = kv_get(key);
      printf("%s == %s / %s\n", key, expected_value, actual_value);
      assert(strcmp(expected_value, actual_value) == 0);
    } else if ((d = strstr(argv[i], "=")) != NULL) {
      *d = '\0';
      char *key = argv[i];
      char *value = d + 1;

      printf("%s = %s\n", key, value);
      kv_set(key, value);
    } else {
      printf("%s == %s\n", argv[i], kv_get(argv[i]));
    }
  }
  return 0;
}

#define SPA_KEY "k"
typedef enum {
  GET,
  SET
} op_t;
typedef struct {
  char op;
  char value;
} operation_t;

void __attribute__((noinline, used)) spa_entry() {
  operation_t operations[5];

  spa_api_input_var(operations);

  for (int i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
    switch (operations[i].op) {
    case GET: {
      kv_get(SPA_KEY);
    } break;
    case SET: {
      char value[2];
      value[0] = operations[i].value;
      value[1] = '\0';
      kv_set(SPA_KEY, value);
    } break;
    default:
      return;
      break;
    }
  }
}
