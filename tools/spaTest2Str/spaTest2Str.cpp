#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>

std::string escapeChar(unsigned char c) {
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

int main(int argc, char **argv, char **envp) {
  if (argc < 3 || argc > 4) {
    std::cerr << "Usage: " << argv[0] << " [-f] <input-file> <output-file>"
              << std::endl;

    return -1;
  }

  bool follow = false;

  unsigned int arg = 1;
  if (std::string("-f") == argv[arg]) {
    follow = true;
    arg++;
  }
  char *inputFileName = argv[arg++];
  char *outputFileName = argv[arg++];

  std::ifstream inputFile(inputFileName);
  std::ofstream outputFile(outputFileName);

  while (inputFile.good() || follow) {
    if (follow && !inputFile.good()) {
      std::cerr << "Reached end of input. Sleeping." << std::endl;
      sleep(1);
      inputFile.clear();
      continue;
    }
    std::string line;
    std::getline(inputFile, line);

    if (!line.empty()) {
      size_t d = line.find(' ');
      std::string name = line.substr(0, d);
      assert(!name.empty() && "Empty variable name.");

      if (d == std::string::npos) {
        outputFile << "char " << name << "[] = \"\";" << std::endl;
      } else {
        std::stringstream ss(line.substr(d + 1, std::string::npos));
        ss << std::hex;

        outputFile << "char " << name << "[] = \"";

        while (ss.good()) {
          int c;
          ss >> c;
          if (c == '\0')
            break;
          outputFile << escapeChar(c);
        }
        outputFile << "\";" << std::endl;
      }
    } else {
      outputFile << std::endl;
    }
  }
}
