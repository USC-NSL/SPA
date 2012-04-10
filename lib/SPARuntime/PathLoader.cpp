/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <list>

#include "llvm/Support/MemoryBuffer.h"

#include "../../lib/Core/Memory.h"
#include "klee/ExprBuilder.h"
#include <expr/Parser.h>

#include <spa/PathLoader.h>

namespace {
	typedef enum {
		START,
		PATH,
		SYMBOLS,
		TAGS,
		KQUERY,
		PATH_DONE
	} LoadState_t;
}

namespace SPA {
	std::string cleanUpLine( std::string line ) {
		// Remove comments.
		line = line.substr( 0, line.find( SPA_PATH_COMMENT ) );
		// Remove trailing white space.
		line = line.substr( 0, line.find_last_not_of( SPA_PATH_WHITE_SPACE ) + 1 );
		// Remove leading white space.
		if ( line.find_first_not_of( SPA_PATH_WHITE_SPACE ) != line.npos )
			line = line.substr( line.find_first_not_of( SPA_PATH_WHITE_SPACE ), line.npos );
		
		return line;
	}

	std::vector<std::string> split( std::string str, std::string delimiter ) {
		std::vector<std::string> result;
		size_t p;
		while ( (p = str.find( delimiter )) != std::string::npos ) {
			result.push_back( str.substr( 0, p ) );
			str = str.substr( p + delimiter.size() );
		}
		result.push_back( str );

		return result;
	}

	#define changeState( from, to ) \
		if ( state != from ) { CLOUD9_ERROR( "Invalid path file. Error near line " << lineNumber << "." ); \
			assert( false && "Invalid path file." ); \
		} \
		state = to;

	Path *PathLoader::getPath() {
		LoadState_t state = START;
		Path *path = NULL;
		std::map<std::string, std::string> arrayToName;
		std::list<std::string> symbolsWithValue;
		std::string kQuery;
		while ( input.good() ) {
			std::string line;
			getline( input, line );
			lineNumber++;
			line = cleanUpLine( line );
			if ( line.empty() )
				continue;

			if ( line == SPA_PATH_START ) {
				changeState( START, PATH );
				path = new Path();
				arrayToName.clear();
				symbolsWithValue.clear();
				kQuery = "";
			} else if ( line == SPA_PATH_SYMBOLS_START ) {
				changeState( PATH, SYMBOLS );
			} else if ( line == SPA_PATH_SYMBOLS_END ) {
				changeState( SYMBOLS, PATH );
			} else if ( line == SPA_PATH_TAGS_START ) {
				changeState( PATH, TAGS );
			} else if ( line == SPA_PATH_TAGS_END ) {
				changeState( TAGS, PATH );
			} else if ( line == SPA_PATH_KQUERY_START ) {
				changeState( PATH, KQUERY );
			} else if ( line == SPA_PATH_KQUERY_END ) {
				changeState( KQUERY, PATH_DONE );

				llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer( kQuery );
				klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
				klee::expr::Parser *P = klee::expr::Parser::Create( "", MB, Builder );
				while ( klee::expr::Decl *D = P->ParseTopLevelDecl() ) {
					assert( ! P->GetNumErrors() && "Error parsing kquery in path file." );
					if ( klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>( D ) ) {
						path->symbols.insert( AD->Root );
						if ( arrayToName.count( AD->Root->name ) )
							path->symbolNames[arrayToName[AD->Root->name]] = AD->Root;
					} else if ( klee::expr::QueryCommand *QC = dyn_cast<klee::expr::QueryCommand>( D ) ) {
						path->constraints = klee::ConstraintManager( QC->Constraints );

						if ( ! symbolsWithValue.empty() ) {
							std::list<std::string>::iterator curSymbol = symbolsWithValue.begin();
							unsigned b = path->symbolNames[*curSymbol]->size;
							for ( std::vector<klee::ref<klee::Expr> >::const_iterator it = QC->Values.begin(), ie = QC->Values.end(); it != ie; it++, b-- ) {
								if ( b == 0 ) {
									assert( ++curSymbol != symbolsWithValue.end() && "Too many expressions in path file kquery." );
									b = path->symbolNames[*curSymbol]->size;
								}
								path->symbolValues[*curSymbol].push_back( *it );
							}
							assert( (b == 0) && (++curSymbol == symbolsWithValue.end()) && "Too few expressions in path file kquery." );
						}

						delete D;
						break;
					}
				}
				delete P;
				delete Builder;
				delete MB;
			} else if ( line == SPA_PATH_END ) {
				changeState( PATH_DONE, START );
				if ( ! filter || filter->checkPath( *path ) )
					return path;
				else
					delete path;
			} else {
				switch ( state ) {
					case SYMBOLS : {
						std::vector<std::string> s = split( line, SPA_PATH_SYMBOL_DELIMITER );
						assert( s.size() == 3 && "Invalid symbol specification in path file." );
						arrayToName[s[1]] = s[0];
						if ( s[2] == SPA_PATH_SYMBOL_HASVALUE )
							symbolsWithValue.push_back( s[0] );
						else
							assert( s[2] == SPA_PATH_SYMBOL_NOVALUE && "Invalid symbol specification in path file." );
					} break;
					case TAGS : {
						std::vector<std::string> s = split( line, SPA_PATH_TAG_DELIMITER );
						assert( s.size() == 2 && "Invalid tag specification in path file." );
						path->tags[s[0]] = s[1];
					} break;
					case KQUERY : {
						kQuery += " " + line;
					} break;
					default : {
						assert( false && "Invalid path file." );
					} break;
				}
			}
		}

		return NULL;
	}
}
