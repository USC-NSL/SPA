/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __BlacklistIF_H__
#define __BlacklistIF_H__

#include <set>

#include <spa/NegatedIF.h>
#include <spa/WhitelistIF.h>

namespace SPA {
class BlacklistIF : public NegatedIF {
public:
  BlacklistIF(std::set<llvm::Instruction *> blacklist)
      : NegatedIF(new WhitelistIF(blacklist)) {}
};
}

#endif // #ifndef __BlacklistIF_H__
