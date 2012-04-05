/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __PATHLOADER_H__
#define __PATHLOADER_H__

#include <spa/Path.h>
#include <spa/PathFilter.h>

namespace SPA {
	class PathLoader {
	private:
		std::istream &input;
		int lineNumber;
		PathFilter *filter;

	public:
		PathLoader( std::istream &_input ) : input( _input ), lineNumber( 0 ) {}
		void setFilter( PathFilter *_filter ) { filter = _filter; }
		void restart() { input.seekg( 0 ); lineNumber = 0; }

		Path *getPath();
	};
}

#endif // #ifndef __PATHLOADER_H__
