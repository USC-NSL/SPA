/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PathFilter_H__
#define __PathFilter_H__

namespace SPA {
class Path;

class PathFilter {
public:
  /**
   * Checks if a path should be saved or filtered out.
   *
   * @param path The path to check.
   * @return true if the path should be saved;
   *         false if it should be filtered out.
   */
  virtual bool checkPath(Path &path) = 0;

protected:
  virtual ~PathFilter() {}
};
}

#endif // #ifndef __PathFilter_H__
