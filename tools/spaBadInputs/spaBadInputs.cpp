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

namespace {
	llvm::cl::opt<std::string> ClientPathFile("client", llvm::cl::desc(
		"Specifies the client path file."));

	llvm::cl::opt<std::string> ServerPathFile("server", llvm::cl::desc(
		"Specifies the server path file."));
}

void showResult( std::vector<unsigned char> result ) {
	std::cout << "[" << result.size() << "]";
	for ( std::vector<unsigned char>::iterator it = result.begin(), ie = result.end(); it != ie; it++ )
		std::cout << " " << (int) *it;
	std::cout << std::endl;
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
	std::set<SPA::Path *> serverPaths;
	while ( SPA::Path *path = serverPathLoader.getPath() )
		serverPaths.insert( path );
	pathFile.close();

	std::cerr << "Building global constraint." << std::endl;
	klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();

	klee::ref<klee::Expr> generated = exprBuilder->False();
	// OR data from each client path.
	for ( std::set<SPA::Path *>::iterator it1 = clientPaths.begin(), ie1 = clientPaths.end(); it1 != ie1; it1++ ) {
		klee::ref<klee::Expr> pc = exprBuilder->True();
		// AND path constraints.
		for ( klee::ConstraintManager::const_iterator it2 = (*it1)->getConstraints().begin(), ie2 = (*it1)->getConstraints().end(); it2 != ie2; it2++ )
			pc = exprBuilder->And( *it2, pc );
		// AND client output values = server input array.
		//TODO: Generalize
		for ( int offset = 0; offset < (*it1)->getSymbolValueSize( "spa_output_query" ); offset++ ) {
			klee::UpdateList ul( (*serverPaths.begin())->getSymbol( "spa_input_query" ), 0 );
			pc = exprBuilder->And(
				exprBuilder->Eq(
					(*it1)->getSymbolValue( "spa_output_query", offset ),
					klee::ReadExpr::create( ul, klee::ConstantExpr::alloc( offset, klee::Expr::Int32 ) ) ),
				pc );
		}
		generated = exprBuilder->Or( pc, generated );
	}

	klee::ref<klee::Expr> accepted = exprBuilder->False();
	// OR data from each server path.
	for ( std::set<SPA::Path *>::iterator it1 = serverPaths.begin(), ie1 = serverPaths.end(); it1 != ie1; it1++ ) {
		klee::ref<klee::Expr> pc = exprBuilder->True();
		// AND path constraints
		for ( klee::ConstraintManager::const_iterator it2 = (*it1)->getConstraints().begin(), ie2 = (*it1)->getConstraints().end(); it2 != ie2; it2++ )
			pc = exprBuilder->And( *it2, pc );
		accepted = exprBuilder->Or( pc, accepted );
	}

	klee::ref<klee::Expr> badInputs = exprBuilder->And( generated, exprBuilder->Not( accepted ) );
// 	std::cout << "Constraint on bad inputs:" << std::endl << badInputs << std::endl;

	klee::ConstraintManager cm;
	cm.addConstraint( badInputs );

	klee::Solver *solver = new klee::STPSolver( false, true );
	solver = klee::createCexCachingSolver(solver);
	solver = klee::createCachingSolver(solver);
	solver = klee::createIndependentSolver(solver);

	std::vector<const klee::Array*> objects;
	//TODO: Generalize
	objects.push_back( (*clientPaths.begin())->getSymbol( "spa_input_op" ) );
	objects.push_back( (*clientPaths.begin())->getSymbol( "spa_input_arg1" ) );
	objects.push_back( (*clientPaths.begin())->getSymbol( "spa_input_arg2" ) );

	std::vector< std::vector<unsigned char> > result;
	std::cerr << "Solving constraint." << std::endl;
	if ( solver->getInitialValues( klee::Query( cm, exprBuilder->False() ), objects, result ) ) {
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
