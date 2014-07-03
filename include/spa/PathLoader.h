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
		unsigned long lineNumber;
		PathFilter *filter;

		unsigned long savedLN;
		std::streampos savedPos;

	public:
		PathLoader( std::ifstream &_input ) : input( _input ), lineNumber( 0 ), filter( NULL ), savedLN( 0 ), savedPos( 0 ) {}
		void setFilter( PathFilter *_filter ) { filter = _filter; }
		void restart() { input.clear(); input.seekg( 0, std::ios::beg ); lineNumber = 0; }
		void save() { savedPos = input.tellg(); savedLN = lineNumber; }
		void load() { input.clear(); input.seekg( savedPos, std::ios::beg ); lineNumber = savedLN; }

		Path *getPath();
    Path *getPath( uint64_t pathID );
    bool skipPath();
	};
}

#endif // #ifndef __PATHLOADER_H__
