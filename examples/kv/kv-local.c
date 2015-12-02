#include <stdlib.h>
#include <string.h>

#include "kv.h"

struct store_t;
typedef struct store_t {
  key_t key;
  value_t value;
  struct store_t *next;
} store_t;

store_t *store = NULL;

value_t kv_get(key_t key) {
  store_t *s;
  for (s = store; s && s->key != key; s = s->next)
    ;
  return s ? s->value : NULL_VALUE;
}

void kv_set(key_t key, value_t value) {
  store_t *s;
  for (s = store; s && s->key != key; s = s->next) {
  }
  if (s) {
    s->value = value;
  } else {
    store_t *new_store = malloc(sizeof(store_t));
    new_store->key = key;
    new_store->value = value;
    new_store->next = store;
    store = new_store;
  }
}
