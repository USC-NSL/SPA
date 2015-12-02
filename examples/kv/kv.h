typedef char key_t;
typedef char value_t;

#define NULL_VALUE '-'

value_t kv_get(key_t key);
void kv_set(key_t key, value_t value);
