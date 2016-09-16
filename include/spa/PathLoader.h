/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PATHLOADER_H__
#define __PATHLOADER_H__

#include <spa/Path.h>
#include <spa/PathFilter.h>

#include <fstream>

namespace SPA {
typedef struct PathLoaderPosition {
  bool loadEmpty;
  unsigned long pathNumber;
  unsigned long lineNumber;
  std::streampos filePosition;
} PathLoaderPosition;

class PathLoader {
private:
  std::ifstream &input;
  bool loadEmptyFirst;
  bool loadEmpty;
  unsigned long pathNumber;
  unsigned long lineNumber;
  PathFilter *filter;

public:
  PathLoader(std::ifstream &input, bool loadEmptyFirst = false)
      : input(input), loadEmptyFirst(loadEmptyFirst), loadEmpty(loadEmptyFirst),
        pathNumber(0), lineNumber(0), filter(NULL) {}
  void setFilter(PathFilter *_filter) { filter = _filter; }
  void restart() {
    input.clear();
    input.seekg(0, std::ios::beg);
    loadEmpty = loadEmptyFirst;
    pathNumber = 0;
    lineNumber = 0;
  }
  PathLoaderPosition save() {
    PathLoaderPosition p;

    p.loadEmpty = loadEmpty;
    p.filePosition = input.tellg();
    p.pathNumber = pathNumber;
    p.lineNumber = lineNumber;

    return p;
  }
  void load(PathLoaderPosition p) {
    input.clear();
    input.seekg(p.filePosition, std::ios::beg);
    loadEmpty = p.loadEmpty;
    pathNumber = p.pathNumber;
    lineNumber = p.lineNumber;
  }

  Path *getPath();
  Path *getPath(uint64_t pathID);
  std::string getPathText();
  std::string getPathText(uint64_t pathID);
  bool skipPath();
  bool gotoPath(uint64_t pathID);
};
}

#endif // #ifndef __PATHLOADER_H__
