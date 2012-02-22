#include <assert.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <utility>

#include <map>

#include <llvm/Support/MemoryBuffer.h>
#include <klee/Constraints.h>
#include <klee/ExprBuilder.h>
#include <expr/Parser.h>
#include <spa/maxRuntime.h>
#include <spa/max.h>

std::map<std::string, std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> > *paths = NULL;
std::map<std::string, std::pair<void *, size_t> > fixedInputs;
std::map<std::string, std::pair<void *, size_t> > varInputs;

std::string cleanUpLine( std::string line ) {
	// Remove comments.
	line = line.substr( 0, line.find( MAX_PATH_COMMENT ) );
	// Remove trailing white space.
	line = line.substr( 0, line.find_last_not_of( MAX_PATH_WHITE_SPACE ) + 1 );
	// Remove leading white space.
	if ( line.find_first_not_of( MAX_PATH_WHITE_SPACE ) != line.npos )
		line = line.substr( line.find_first_not_of( MAX_PATH_WHITE_SPACE ), line.npos );
	
	return line;
}

typedef enum {
	PATH_STARTED,
	SYMBOLS,
	CONSTRAINTS,
	PATH_DONE,
} LoadState_t;

#define changeState( from, to ) \
	assert( state == from && "Invalid path file." ); \
	state = to;

void loadPaths() {
	if ( ! paths ) {
		paths = new std::map<std::string, std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> >();
		int numPaths = 0;

		std::cerr << "Loading path data from file: " << MAX_PATH_FILE << std::endl;
		std::ifstream pathFile( MAX_PATH_FILE );
		assert( pathFile.is_open() && "Unable to open path file." );

		LoadState_t state = PATH_DONE;
		std::string handlerName = "";
		std::string kQuery = "";
		std::map<std::string, std::string> symbols;
		while ( pathFile.good() ) {
			std::string line;
			getline( pathFile, line );
			line = cleanUpLine( line );
			if ( line.empty() )
				continue;

			if ( line == MAX_PATH_START ) {
				changeState( PATH_DONE, PATH_STARTED );
			} else if ( line == MAX_PATH_SYMBOLS_START ) {
				changeState( PATH_STARTED, SYMBOLS );
				assert( (! handlerName.empty()) && "No handler name found." );
			} else if ( line == MAX_PATH_SYMBOLS_END ) {
				changeState( SYMBOLS, PATH_STARTED );
			} else if ( line == MAX_PATH_CONSTRAINTS_START ) {
				changeState( PATH_STARTED, CONSTRAINTS );
				kQuery = "";
			} else if ( line == MAX_PATH_CONSTRAINTS_END ) {
				changeState( CONSTRAINTS, PATH_STARTED );

				llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer( kQuery );
				klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
				klee::expr::Parser *P = klee::expr::Parser::Create( MAX_PATH_FILE, MB, Builder );
				while ( klee::expr::Decl *D = P->ParseTopLevelDecl() ) {
					assert( ! P->GetNumErrors() && "Error parsing constraints in path file." );
					if ( klee::expr::QueryCommand *QC = dyn_cast<klee::expr::QueryCommand>( D ) ) {
						(*paths)[handlerName].second = new klee::ConstraintManager( QC->Constraints );
						delete D;
						break;
					} else if ( klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>( D ) ) {
						(*paths)[handlerName].first[symbols[AD->Name->Name]] = AD->Root;
					}
				}
				delete P;
				delete Builder;
				delete MB;
			} else if ( line == MAX_PATH_END ) {
				changeState( PATH_STARTED, PATH_DONE );
				numPaths++;
			} else {
				switch ( state ) {
					case PATH_STARTED : {
						assert( handlerName.empty() && "Handler name multiply defined." );
						handlerName = line;
					} break;
					case SYMBOLS : {
						assert( line.find( MAX_PATH_SYMBOLS_DELIMITER ) != line.npos && line.find( MAX_PATH_SYMBOLS_DELIMITER ) + strlen( MAX_PATH_SYMBOLS_DELIMITER ) < line.size() && "Invalid symbol definition." );
						std::string name = line.substr( 0, line.find( MAX_PATH_SYMBOLS_DELIMITER ) );
						std::string symbol = line.substr( line.find( MAX_PATH_SYMBOLS_DELIMITER ) + strlen( MAX_PATH_SYMBOLS_DELIMITER ), line.npos );
						symbols[symbol] = name;
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
		pathFile.close();

		std::cerr << "Loaded " << numPaths << " paths." << std::endl;
	}
}

bool checkPath() {
	return true;
}

void solvePath() {
	
}

extern "C" {
	void klee_make_symbolic(void *addr, size_t nbytes, const char *name) {}

	void max_make_symbolic( void *var, size_t size, const char *name, int fixed ) {
		if ( fixed ) {
			fixedInputs[name].first = var;
			fixedInputs[name].second = size;
		} else {
			varInputs[name].first = var;
			varInputs[name].second = size;
		}
	}

	void max_solve_symbolic( char *name  ) {
		loadPaths();
		if ( checkPath() )
			solvePath();
	}
}
