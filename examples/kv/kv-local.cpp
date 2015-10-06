#include <map>

#include "kv.h"

static std::map<std::string, std::string> store;

std::string kv_get(std::string key) { return store[key]; }

void kv_set(std::string key, std::string value) { store[key] = value; }
