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
#include <klee/Solver.h>
#include <expr/Parser.h>
#include <spa/maxRuntime.h>
#include <spa/max.h>

std::map<std::string, std::set< std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> > > *paths = NULL;
std::map<std::string, std::pair<void *, size_t> > fixedInputs;
std::map<std::string, std::pair<void *, size_t> > varInputs;
klee::Solver *solver;

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
	paths = new std::map<std::string, std::set< std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> > >();
	int numPaths = 0;

	std::cerr << "Loading path data from file: " << MAX_PATH_FILE << std::endl;
	std::ifstream pathFile( MAX_PATH_FILE );
	assert( pathFile.is_open() && "Unable to open path file." );

	LoadState_t state = PATH_DONE;
	std::string handlerName = "";
	std::string kQuery = "";
	std::map<std::string, std::string> symbols;
	std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> pathData;
	while ( pathFile.good() ) {
		std::string line;
		getline( pathFile, line );
		line = cleanUpLine( line );
		if ( line.empty() )
			continue;

		if ( line == MAX_PATH_START ) {
			changeState( PATH_DONE, PATH_STARTED );
			handlerName.clear();
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
					pathData.second = new klee::ConstraintManager( QC->Constraints );
					delete D;
					break;
				} else if ( klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>( D ) ) {
					pathData.first[symbols[AD->Name->Name]] = AD->Root;
				}
			}
			delete P;
			delete Builder;
			delete MB;
		} else if ( line == MAX_PATH_END ) {
			changeState( PATH_STARTED, PATH_DONE );
			(*paths)[handlerName].insert( pathData );
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

void initAdversarialModule() {
	if ( ! paths )
		loadPaths();
	if ( ! solver ) {
		solver = new klee::STPSolver( false, true );
		solver = klee::createCexCachingSolver(solver);
		solver = klee::createCachingSolver(solver);
		solver = klee::createIndependentSolver(solver);
	}
}

void solvePath( char *name ) {
	std::cerr << "Finding interesting paths for " << name << "." << std::endl;

	// Check if handler has known paths.
	if ( paths->count( name ) ) {
// 		std::cerr << "Fixed inputs:" << std::endl;
// 		for ( std::map<std::string, std::pair<void *, size_t> >::iterator iit = fixedInputs.begin(), iie = fixedInputs.end(); iit != iie; iit++ ) {
// 			std::cerr << "	" << iit->first << " =";
// 			for ( unsigned i = 0; i < iit->second.second; i++ )
// 				std::cerr << " " << (int) ((uint8_t *) iit->second.first)[i];
// 			std::cerr << std::endl;
// 		}
		unsigned int numPaths = 1;
		// Check each path.
		for ( std::set< std::pair<std::map<std::string, const klee::Array *>, klee::ConstraintManager *> >::iterator pit = (*paths)[name].begin(), pie = (*paths)[name].end(); pit != pie; pit++, numPaths++ ) {
			klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
			// Load path constraints.
			klee::ConstraintManager cm( *pit->second );
			// And all relevant fixed input constraints.
			for ( std::map<std::string, std::pair<void *, size_t> >::iterator iit = fixedInputs.begin(), iie = fixedInputs.end(); iit != iie; iit++ ) {
				// Check if input is relevant.
				if ( pit->first.count( iit->first ) ) {
					// Add the comparison of each byte.
					klee::UpdateList ul( pit->first.find( iit->first )->second, 0 );
					for ( size_t p = 0; p < iit->second.second; p++ )
						cm.addConstraint( exprBuilder->Eq(
							klee::ReadExpr::create( ul, klee::ConstantExpr::alloc( p, klee::Expr::Int32 ) ),
							exprBuilder->Constant( llvm::APInt( 8, ((uint8_t *) iit->second.first)[p] ) ) ) );
				}
			}

			// Created list of variable inputs.
			std::vector<const klee::Array*> objects;
			for ( std::map<std::string, std::pair<void *, size_t> >::iterator iit = varInputs.begin(), iie = varInputs.end(); iit != iie; iit++ )
				objects.push_back( pit->first.find( iit->first )->second );

// 			std::cerr << "Constraints:" << std::endl;
// 			for ( klee::ConstraintManager::const_iterator cit = cm.begin(), cie = cm.end(); cit != cie; cit++ )
// 				std::cerr << *cit << std::endl;

			std::vector< std::vector<unsigned char> > result;
			if ( solver->getInitialValues( klee::Query( cm, exprBuilder->False() ), objects, result ) ) {
				std::cerr << "Found interesting assignment. Searched " << numPaths << "/" << (*paths)[name].size() << " paths." << std::endl;
				std::cerr << "Variable inputs:" << std::endl;

				std::map<std::string, std::pair<void *, size_t> >::iterator iit, iie;
				std::vector< std::vector<unsigned char> >::iterator rit, rie;
				for ( iit = varInputs.begin(), iie = varInputs.end(), rit = result.begin(), rie = result.end(); iit != iie && rit != rie; iit++, rit++ ) {
					assert( iit->second.second == rit->size() && "Result size doesn't match declared size." );

					std::cerr << "	" << iit->first << ":";
					for ( size_t i = 0; i < iit->second.second; i++ )
						std::cerr << " " << (int) ((uint8_t *) iit->second.first)[i];

					for ( size_t i = 0; i < rit->size(); i++ )
						((unsigned char *) iit->second.first)[i] = (*rit)[i];

					std::cerr << " ->";
					for ( size_t i = 0; i < iit->second.second; i++ )
						std::cerr << " " << (int) ((uint8_t *) iit->second.first)[i];
					std::cerr << std::endl;
				}
				return;
			} else {
// 				std::cerr << "Path does not match." << std::endl;
			}
		}
	}
	std::cerr << "No compatible paths found." << std::endl;
}

extern "C" {
	void klee_make_symbolic(void *addr, size_t nbytes, const char *name) {}

	void max_make_symbolic( void *var, size_t size, const char *name, int fixed ) {
		if ( fixed ) {
			fixedInputs[std::string() + MAX_FIXED_INPUT_PREFIX + name].first = var;
			fixedInputs[std::string() + MAX_FIXED_INPUT_PREFIX + name].second = size;
		} else {
			varInputs[std::string() + MAX_VAR_INPUT_PREFIX + name].first = var;
			varInputs[std::string() + MAX_VAR_INPUT_PREFIX + name].second = size;
		}
	}

	void max_solve_symbolic( char *name  ) {
		initAdversarialModule();
		solvePath( name );
		fixedInputs.clear();
		varInputs.clear();
	}
}
