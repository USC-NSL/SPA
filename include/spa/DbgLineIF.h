/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __DbgLineIF_H__
#define __DbgLineIF_H__

#include <spa/WhitelistIF.h>
#include <spa/CFG.h>

namespace SPA {
class DbgLineIF : public WhitelistIF {
public:
  DbgLineIF(llvm::Module *module, std::string dbgPoint);
};
}

#endif // #ifndef __DbgLineIF_H__
