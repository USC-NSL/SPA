/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __DbgLineIF_H__
#define __DbgLineIF_H__

#include <spa/WhitelistIF.h>
#include <spa/CFG.h>

namespace SPA {
class DbgLineIF : public WhitelistIF {
private:
  DbgLineIF(
      std::set<std::pair<llvm::Instruction *, llvm::Instruction *> > whitelist)
      : WhitelistIF(whitelist) {}

public:
  static DbgLineIF *parse(llvm::Module *module, std::string dbgPoint);
};
}

#endif // #ifndef __DbgLineIF_H__
