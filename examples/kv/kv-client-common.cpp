#include <iostream>
#include <cassert>
#include <vector>

#include "kv.h"

int main(int argc, char *argv[]) {
  std::vector<std::string> args(argv + 1, argv + argc);

  for (std::string arg : args) {
    size_t d;
    if ((d = arg.find("==")) != std::string::npos) {
      std::string key = arg.substr(0, d);
      std::string expected_value = arg.substr(d + 2);

      std::string actual_value = kv_get(key);
      std::cout << key << " == " << expected_value << " / " << actual_value
                << std::endl;
      assert(expected_value == actual_value);
    } else if ((d = arg.find("=")) != std::string::npos) {
      std::string key = arg.substr(0, d);
      std::string value = arg.substr(d + 1);

      std::cout << key << " = " << value << std::endl;
      kv_set(key, value);
    } else {
      std::cout << arg << " == " << kv_get(arg) << std::endl;
    }
  }
  return 0;
}
