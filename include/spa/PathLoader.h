/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PATHLOADER_H__
#define __PATHLOADER_H__

#include <spa/Path.h>
#include <spa/PathFilter.h>

#include <fstream>

namespace SPA {
class PathLoader {
private:
  std::ifstream &input;
  bool loadEmptyFirst;
  bool loadEmpty;
  unsigned long lineNumber;
  PathFilter *filter;

  bool savedLoadEmpty;
  unsigned long savedLN;
  std::streampos savedPos;

public:
  PathLoader(std::ifstream &input, bool loadEmptyFirst = false)
      : input(input), loadEmptyFirst(loadEmptyFirst), loadEmpty(loadEmptyFirst),
        lineNumber(0), filter(NULL), savedLN(0), savedPos(0) {}
  void setFilter(PathFilter *_filter) { filter = _filter; }
  void restart() {
    input.clear();
    input.seekg(0, std::ios::beg);
    loadEmpty = loadEmptyFirst;
    lineNumber = 0;
  }
  void save() {
    savedLoadEmpty = loadEmpty;
    savedPos = input.tellg();
    savedLN = lineNumber;
  }
  void load() {
    input.clear();
    input.seekg(savedPos, std::ios::beg);
    loadEmpty = savedLoadEmpty;
    lineNumber = savedLN;
  }

  Path *getPath();
  Path *getPath(uint64_t pathID);
  std::string getPathText();
  std::string getPathText(uint64_t pathID);
  bool skipPath();
  bool skipPaths(uint64_t pathID);
};
}

#endif // #ifndef __PATHLOADER_H__
