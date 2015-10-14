#include <stdlib.h>
#include <string.h>

#include "kv.h"

struct store_t;
typedef struct store_t {
  char *key;
  char *value;
  struct store_t *next;
} store_t;

store_t *store = NULL;

char *kv_get(char *key) {
  store_t *s;
  for (s = store; s && strcmp(s->key, key) != 0; s = s->next)
    ;
  return s ? s->value : "";
}

void kv_set(char *key, char *value) {
  store_t *s;
  for (s = store; s && strcmp(s->key, key) != 0; s = s->next) {
  }
  if (s) {
    free(s->value);
    s->value = value;
  } else {
    store_t *new_store = malloc(sizeof(store_t));
    new_store->key = malloc(strlen(key) + 1);
    new_store->value = malloc(strlen(value) + 1);

    strcpy(new_store->key, key);
    strcpy(new_store->value, value);
  }
}
