#include <assert.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <map>

#include "llvm/Support/CommandLine.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include <llvm/Support/MemoryBuffer.h>
#include <klee/Constraints.h>
#include <klee/ExprBuilder.h>
#include <klee/Solver.h>
#include <klee/Init.h>
#include <klee/Expr.h>
#include <klee/ExprBuilder.h>
#include <klee/Solver.h>
#include <../../lib/Core/Memory.h>

#include "spa/SPA.h"
#include "spa/PathLoader.h"

//TODO:Generalize
#define SPA_VALID_TAG	"QueryValid"

namespace {
	llvm::cl::opt<std::string> ClientPathFile("client", llvm::cl::desc(
		"Specifies the client path file."));

	llvm::cl::opt<std::string> ServerPathFile("server", llvm::cl::desc(
		"Specifies the server path file."));
}

class InvalidPathFilter : public SPA::PathFilter {
public:
	bool checkPath( SPA::Path &path ) {
		return path.getTag( SPA_VALID_TAG ).empty();
	}
};

void showResult( std::vector<unsigned char> result ) {
	std::cout << "[" << result.size() << "]";
	for ( std::vector<unsigned char>::iterator it = result.begin(), ie = result.end(); it != ie; it++ )
		std::cout << " " << (int) *it;
	std::cout << std::endl;
}

void showConstraints( klee::ConstraintManager &cm ) {
	for ( klee::ConstraintManager::const_iterator it = cm.begin(), ie = cm.end(); it != ie; it++ )
		std::cerr << *it << std::endl;
}

int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Bad Input Generator" );

	assert( ClientPathFile.size() > 0 && "No client path file specified." );
	std::cerr << "Loading client path data." << std::endl;
	std::ifstream pathFile( ClientPathFile.getValue().c_str() );
	assert( pathFile.is_open() && "Unable to open path file." );
	SPA::PathLoader clientPathLoader( pathFile );
	std::set<SPA::Path *> clientPaths;
	while ( SPA::Path *path = clientPathLoader.getPath() )
		clientPaths.insert( path );
	pathFile.close();

	assert( ServerPathFile.size() > 0 && "No server path file specified." );
	std::cerr << "Loading server path data." << std::endl;
	pathFile.open( ServerPathFile.getValue().c_str() );
	assert( pathFile.is_open() && "Unable to open path file." );
	SPA::PathLoader serverPathLoader( pathFile );
	serverPathLoader.setFilter( new InvalidPathFilter() );
	std::set<SPA::Path *> serverPaths;
	while ( SPA::Path *path = serverPathLoader.getPath() )
		serverPaths.insert( path );
	pathFile.close();

	std::cerr << "Solving path pairs." << std::endl;
	klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
	klee::Solver *solver = new klee::STPSolver( false, true );
	solver = klee::createCexCachingSolver(solver);
	solver = klee::createCachingSolver(solver);
	solver = klee::createIndependentSolver(solver);

	unsigned long numClientPaths = 0;
	for ( std::set<SPA::Path *>::iterator cit = clientPaths.begin(), cie = clientPaths.end(); cit != cie; cit++, numClientPaths++ ) {
		unsigned long numServerPaths = 0;
		for ( std::set<SPA::Path *>::iterator sit = serverPaths.begin(), sie = serverPaths.end(); sit != sie; sit++, numServerPaths++ ) {
			std::cerr << "Processing client path " << numClientPaths << "/" << clientPaths.size()
				<< " with server path " << numServerPaths << "/" << serverPaths.size() << "." << std::endl;
			klee::ConstraintManager cm;
			klee::ref<klee::Expr> badInputs = exprBuilder->True();
			// Add client path constraints.
			for ( klee::ConstraintManager::const_iterator it = (*cit)->getConstraints().begin(), ie = (*cit)->getConstraints().end(); it != ie; it++ )
// 				badInputs = exprBuilder->And( *it, badInputs );
				cm.addConstraint( *it );
			// Add server path constraints.
			for ( klee::ConstraintManager::const_iterator it = (*sit)->getConstraints().begin(), ie = (*sit)->getConstraints().end(); it != ie; it++ )
// 				badInputs = exprBuilder->And( *it, badInputs );
				cm.addConstraint( *it );
			// Add client output values = server input array constraint.
			//TODO: Generalize
			for ( int offset = 0; offset < (*cit)->getSymbolValueSize( "spa_output_query" ); offset++ ) {
				klee::UpdateList ul( (*sit)->getSymbol( "spa_input_query" ), 0 );
				badInputs = exprBuilder->And(
					exprBuilder->Eq(
						exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
						(*cit)->getSymbolValue( "spa_output_query", offset ) ),
					badInputs );
			}
			std::cerr << "Path pair constraints:" << std::endl;
			showConstraints( cm );
			std::cerr << badInputs << std::endl;

			std::vector<const klee::Array*> objects;
			//TODO: Generalize
			objects.push_back( (*cit)->getSymbol( "spa_input_op" ) );
			objects.push_back( (*cit)->getSymbol( "spa_input_arg1" ) );
			objects.push_back( (*cit)->getSymbol( "spa_input_arg2" ) );

			std::cerr << "Solving constraint for:" << std::endl;
			for ( std::vector<const klee::Array*>::iterator it = objects.begin(), ie = objects.end(); it != ie; it++ )
				std::cerr << (*it)->name << std::endl;
			std::vector< std::vector<unsigned char> > result;
			if ( solver->getInitialValues( klee::Query( cm, badInputs ), objects, result ) ) {
				std::cerr << "Found solution:" << std::endl;
				//TODO: Generalize
				std::cout << "	op = ";
				showResult( result[0] );
				std::cout << "	arg1 = ";
				showResult( result[1] );
				std::cout << "	arg2 = ";
				showResult( result[2] );
			} else {
				std::cerr << "Could not solve constraint." << std::endl;
			}
		}
	}
}
