/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <set>

#include <spa/NegatedIF.h>
#include <spa/WhitelistIF.h>

#ifndef __BlacklistIF_H__
#define __BlacklistIF_H__

namespace SPA {
	class BlacklistIF : public InstructionFilter, private NegatedIF {
	public:
		BlacklistIF( std::set<Instruction *> blacklist ) : NegatedIF( new WhitelistIF( blacklist ) ) { }
	};
}

#endif // #ifndef __BlacklistIF_H__
