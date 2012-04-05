/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include "llvm/Support/MemoryBuffer.h"

#include "../../lib/Core/Memory.h"
#include "klee/ExprBuilder.h"
#include <expr/Parser.h>

#include <spa/PathLoader.h>

namespace {
	typedef enum {
		START,
		PATH,
		SYMBOL_NAME,
		SYMBOL_ARRAY,
		SYMBOL_VALUE,
		TAG_KEY,
		TAG_VALUE,
		CONSTRAINTS,
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

	#define changeState( from, to ) \
		if ( state != from ) { CLOUD9_ERROR( "Invalid path file. Error near line " << lineNumber << "." ); \
			assert( false && "Invalid path file." ); \
		} \
		state = to;

	Path *PathLoader::getPath() {

		LoadState_t state = START;
		Path *path = NULL;
		std::string symbolName, symbolArray, tagKey, tagValue, kQuery;
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
			} else if ( line == SPA_PATH_SYMBOL_START ) {
				changeState( PATH, SYMBOL_NAME );
				symbolName = "";
				symbolArray = "";
				kQuery = "";
			} else if ( line == SPA_PATH_SYMBOL_END ) {
				changeState( SYMBOL_VALUE, PATH );
				assert( (! symbolName.empty()) && (! symbolArray.empty()) && "Invalid path file." );
				llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer( kQuery );
				klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
				klee::expr::Parser *P = klee::expr::Parser::Create( "", MB, Builder );
				while ( klee::expr::Decl *D = P->ParseTopLevelDecl() ) {
					assert( ! P->GetNumErrors() && "Error parsing symbol value in path file." );
					if ( klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>( D ) ) {
						path->symbols.insert( AD->Root );
						if ( symbolArray == AD->Root->name )
							path->symbolNames[symbolName] = AD->Root;
					} else if ( klee::expr::QueryCommand *QC = dyn_cast<klee::expr::QueryCommand>( D ) ) {
						path->symbolValues[symbolName] = QC->Values;
						delete D;
						break;
					}
				}
				delete P;
				delete Builder;
				delete MB;
			} else if ( line == SPA_PATH_TAG_START ) {
				changeState( PATH, TAG_KEY );
				tagKey = "";
				tagValue = "";
			} else if ( line == SPA_PATH_TAG_END ) {
				changeState( TAG_VALUE, PATH );
				assert( (! tagKey.empty()) && (! tagValue.empty()) && "Invalid path file." );
				path->tags[tagKey] = tagValue;
			} else if ( line == SPA_PATH_CONSTRAINTS_START ) {
				changeState( PATH, CONSTRAINTS );
				kQuery = "";
			} else if ( line == SPA_PATH_CONSTRAINTS_END ) {
				changeState( CONSTRAINTS, PATH_DONE );

				llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer( kQuery );
				klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
				klee::expr::Parser *P = klee::expr::Parser::Create( "", MB, Builder );
				while ( klee::expr::Decl *D = P->ParseTopLevelDecl() ) {
					assert( ! P->GetNumErrors() && "Error parsing constraints in path file." );
					if ( klee::expr::QueryCommand *QC = dyn_cast<klee::expr::QueryCommand>( D ) ) {
						path->constraints = klee::ConstraintManager( QC->Constraints );
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
					case SYMBOL_NAME : {
						assert( symbolName.empty() && "Invalid path file." );
						symbolName = line;
						changeState( SYMBOL_NAME, SYMBOL_ARRAY );
					} break;
					case SYMBOL_ARRAY : {
						assert( symbolArray.empty() && "Invalid path file." );
						symbolArray = line;
						changeState( SYMBOL_ARRAY, SYMBOL_VALUE );
					} break;
					case SYMBOL_VALUE : {
						kQuery += " " + line;
					} break;
					case TAG_KEY : {
						assert( tagKey.empty() && "Invalid path file." );
						tagKey = line;
						changeState( TAG_KEY, TAG_VALUE );
					} break;
					case TAG_VALUE : {
						assert( tagValue.empty() && "Invalid path file." );
						tagValue = line;
					} break;
					case CONSTRAINTS : {
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
