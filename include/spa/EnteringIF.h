/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __EnteringIF_H__
#define __EnteringIF_H__

#include <spa/WhitelistIF.h>
#include <spa/CFG.h>
#include <spa/CG.h>

namespace SPA {
class EnteringIF : public WhitelistIF {
public:
  EnteringIF(CFG &cfg, CG &cg, InstructionFilter *filter);
};
}

#endif // #ifndef __EnteringIF_H__
