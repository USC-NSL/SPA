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

	llvm::cl::opt<std::string> OutputFile("o", llvm::cl::desc(
		"Specifies the file to write output data to."));

	llvm::cl::opt<std::string> DebugFile("d", llvm::cl::desc(
		"Specifies the file to write debug data to."));
	
	llvm::cl::opt<bool> Follow("f", llvm::cl::desc(
		"Follows input files as they are generated."));
}

std::ofstream output, debug;
unsigned long solutions = 0;

class ClientPathFilter : public SPA::PathFilter {
public:
	bool checkPath( SPA::Path &path ) {
		return /*path.getTag( SPA_HANDLERTYPE_TAG ) == SPA_APIHANDLER_VALUE &&*/
			! path.getTag( SPA_OUTPUT_TAG ).empty();
	}
};

class ServerPathFilter : public SPA::PathFilter {
public:
	bool checkPath( SPA::Path &path ) {
		return path.getTag( SPA_HANDLERTYPE_TAG ) == SPA_MESSAGEHANDLER_VALUE &&
			(path.getTag( SPA_VALIDPATH_TAG ) != SPA_VALIDPATH_VALUE || 
				! path.getTag( SPA_OUTPUT_TAG ).empty());
	}
};

// void showConstraints( klee::ConstraintManager &cm ) {
// 	for ( klee::ConstraintManager::const_iterator it = cm.begin(), ie = cm.end(); it != ie; it++ )
// 		std::cerr << *it << std::endl;
// }

std::string escapeChar( char c ) {
	if ( c == '\n' )
		return "\\n";
	if ( c == '\r' )
		return "\\r";
	if ( c == '\\' )
		return "\\\\";
	if ( c == '\n' )
		return "\\n";
	if ( c == '\'' )
		return "\\'";
	if ( c == '\"' )
		return "\\\"";
	if ( isprint( c ) )
		return std::string( 1, c );

	char s[5];
	snprintf( s, sizeof( s ), "\\x%x", c );
	return s;
}

bool processPaths( SPA::Path *cp, SPA::Path *sp ) {
	// Check if valid transaction.
	if ( cp->getTag( SPA_VALIDPATH_TAG ) == SPA_VALIDPATH_VALUE && sp->getTag( SPA_VALIDPATH_TAG ) == SPA_VALIDPATH_VALUE ) {
		std::cerr << "	Not invalid path pair." << std::endl;
		return false;
	}

	static klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
	static klee::Solver *solver =
		klee::createIndependentSolver(
			klee::createCachingSolver(
				klee::createCexCachingSolver(
					new klee::STPSolver( false, true ) ) ) );
	static std::set<std::vector< std::vector<unsigned char> > > results;

	klee::ConstraintManager cm;
	// Add client path constraints.
	for ( klee::ConstraintManager::const_iterator it = cp->getConstraints().begin(), ie = cp->getConstraints().end(); it != ie; it++ )
		cm.addConstraint( *it );
	// Add server path constraints.
	for ( klee::ConstraintManager::const_iterator it = sp->getConstraints().begin(), ie = sp->getConstraints().end(); it != ie; it++ )
		cm.addConstraint( *it );

	// Unknowns to solve for,
	std::vector<std::string> objectNames;
	std::vector<const klee::Array*> objects;
	// Flag indicating whether client and server were connected at some point.
	bool connected = false;
	// Process client inputs.
	for ( std::map<std::string, const klee::Array *>::const_iterator it = cp->beginSymbols(), ie = cp->endSymbols(); it != ie; it++ ) {
		// Check if input without a fully concrete initial value.
		if ( it->first.compare( 0, strlen( SPA_INPUT_PREFIX ), SPA_INPUT_PREFIX ) == 0 && (! cp->hasOutput( SPA_INIT_PREFIX + it->first.substr( strlen( SPA_PREFIX ), std::string::npos ) ) ) ) {
			// Check if message.
			if ( it->first.compare( 0, strlen( SPA_MESSAGE_INPUT_PREFIX ), SPA_MESSAGE_INPUT_PREFIX ) == 0 ) {
				std::string type = it->first.substr( strlen( SPA_MESSAGE_INPUT_PREFIX ), std::string::npos );
				const klee::Array *clientIn = it->second;
				std::string serverOutName = std::string( SPA_MESSAGE_OUTPUT_PREFIX ) + type;
				if ( sp->hasOutput( serverOutName ) ) {
					std::cerr << "	Connecting message type: " << type << std::endl;
					assert( clientIn->size >= sp->getOutputSize( serverOutName ) && "Client and server message size mismatch." );
					// Add server output values = client input array constraint.
					for ( size_t offset = 0; offset < sp->getOutputSize( serverOutName ); offset++ ) {
						klee::UpdateList ul( clientIn, 0 );
						klee::ref<klee::Expr> e = exprBuilder->Eq(
							exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
							sp->getOutputValue( serverOutName, offset ) );
						if ( ! cm.addAndCheckConstraint( e ) ) {
							std::cerr << "	Constraints trivially UNSAT." << std::endl;
// 							showConstraints( cm );
// 							std::cerr << "		New constraint:" << std::endl << e << std::endl;
							return false;
						}
					}
				} else {
					std::cerr << "	Client consumes a message type not generated by server: " << it->first << ". Leaving as unknown."<< std::endl;
					objectNames.push_back( it->first );
					objects.push_back( it->second );
				}
			} else { // Not a message (API): solve as an unknown.
				std::cerr << "	Free client input: " << it->first << ". Leaving as unknown."<< std::endl;
				objectNames.push_back( it->first );
				objects.push_back( it->second );
			}
		}
	}
	// Process server inputs.
	for ( std::map<std::string, const klee::Array *>::const_iterator it = sp->beginSymbols(), ie = sp->endSymbols(); it != ie; it++ ) {
		// Check if input without a fully concrete initial value.
		if ( it->first.compare( 0, strlen( SPA_INPUT_PREFIX ), SPA_INPUT_PREFIX ) == 0 && (! sp->hasOutput( SPA_INIT_PREFIX + it->first.substr( strlen( SPA_PREFIX ), std::string::npos ) ) ) ) {
			// Check if message.
			if ( it->first.compare( 0, strlen( SPA_MESSAGE_INPUT_PREFIX ), SPA_MESSAGE_INPUT_PREFIX ) == 0 ) {
				std::string type = it->first.substr( strlen( SPA_MESSAGE_INPUT_PREFIX ), std::string::npos );
				const klee::Array *serverIn = it->second;
				std::string clientOutName = std::string( SPA_MESSAGE_OUTPUT_PREFIX ) + type;
				// Check if client outputs this message type.
				if ( cp->hasOutput( clientOutName ) ) {
					std::cerr << "	Connecting message type: " << type << std::endl;
					assert( serverIn->size >= cp->getOutputSize( clientOutName ) && "Client and server message size mismatch." );
					// Add client output values = server input array constraint.
					for ( size_t offset = 0; offset < cp->getOutputSize( clientOutName ); offset++ ) {
						klee::UpdateList ul( serverIn, 0 );
						klee::ref<klee::Expr> e = exprBuilder->Eq(
							exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
							cp->getOutputValue( clientOutName, offset ) );
						if ( ! cm.addAndCheckConstraint( e ) ) {
							std::cerr << "	Constraints trivially UNSAT." << std::endl;
// 							showConstraints( cm );
// 							std::cerr << "		New constraint:" << std::endl << e << std::endl;
							return false;
						}
						connected = true;
					}
				} else {
					std::cerr << "	Server consumes a message type not generated by client: " << it->first << ". Leaving as unknown."<< std::endl;
					objectNames.push_back( it->first );
					objects.push_back( it->second );
				}
			} else { // Not a message (API): solve as an unknown.
				std::cerr << "	Free server input: " << it->first << ". Leaving as unknown."<< std::endl;
				objectNames.push_back( it->first );
				objects.push_back( it->second );
			}
		}
	}

	if ( ! connected ) {
		std::cerr << "	Client and server not connected." << std::endl;
		return false;
	}

// 	std::cerr << "	Path pair constraints:" << std::endl;
// 	showConstraints( cm );

	std::vector< std::vector<unsigned char> > result;
	if ( solver->getInitialValues( klee::Query( cm, exprBuilder->False() ), objects, result ) ) {
		std::cerr << "	Found solution." << std::endl;

		if ( ! results.count( result ) ) {
			results.insert( result );

			std::ostream &o = output.is_open() ? output : std::cout;
			std::ostream &d = debug.is_open() ? debug : std::cout;

			for ( std::map<std::string, std::vector<klee::ref<klee::Expr> > >::const_iterator oit = cp->beginOutputs(), oie = cp->endOutputs(); oit != oie; oit++ ) {
				if ( oit->first.compare( 0, strlen( SPA_INIT_PREFIX ), SPA_INIT_PREFIX ) == 0 ) {
					std::string name = SPA_PREFIX + oit->first.substr( strlen( SPA_INIT_PREFIX ) );
					std::cerr << "	Outputting initial value for: " << name << std::endl;
					o << name;
					for ( std::vector<klee::ref<klee::Expr> >::const_iterator bit = oit->second.begin(), bie = oit->second.end(); bit != bie; bit++ ) {
						klee::ConstantExpr *ce;
						assert( (ce = dyn_cast<klee::ConstantExpr>( bit->get() )) && "Initial value is not constant." );
						o << " " << std::hex << (int) ce->getZExtValue( 8 );
					}
					o << std::endl;
				}
			}
			for ( std::map<std::string, std::vector<klee::ref<klee::Expr> > >::const_iterator oit = sp->beginOutputs(), oie = sp->endOutputs(); oit != oie; oit++ ) {
				if ( oit->first.compare( 0, strlen( SPA_INIT_PREFIX ), SPA_INIT_PREFIX ) == 0 ) {
					std::string name = SPA_PREFIX + oit->first.substr( strlen( SPA_INIT_PREFIX ) );
					std::cerr << "	Outputting initial value for: " << name << std::endl;
					o << name;
					for ( std::vector<klee::ref<klee::Expr> >::const_iterator bit = oit->second.begin(), bie = oit->second.end(); bit != bie; bit++ ) {
						klee::ConstantExpr *ce;
						assert( (ce = dyn_cast<klee::ConstantExpr>( bit->get() )) && "Initial value is not constant." );
						o << " " << std::hex << (int) ce->getZExtValue( 8 );
					}
					o << std::endl;
				}
			}
			for ( size_t i = 0; i < result.size(); i++ ) {
				std::cerr << "	Outputting solution for: " << objectNames[i] << std::endl;
				o << objectNames[i];
				for ( std::vector<unsigned char>::iterator it = result[i].begin(), ie = result[i].end(); it != ie; it++ )
					o << " " << std::hex << (int) *it;
				o << std::endl;
			}
			o << std::endl;

			d << "/*********" << std::endl;
			d << "Client Path:" << std::endl;
			d << std::endl << *cp << std::endl << std::endl;
			d << "Server Path:" << std::endl;
			d << std::endl << *sp << std::endl << std::endl;
			d << "*********/" << std::endl << std::endl;
			for ( size_t i = 0; i < result.size(); i++ ) {
				d << "uint8_t " << objectNames[i] << "[" << result[i].size() << "] = {";
				for ( std::vector<unsigned char>::iterator it = result[i].begin(), ie = result[i].end(); it != ie; it++ )
					d << " " << (int) *it << (it != result[i].end() ? "," : "");
				d << " };" << std::endl;
				d << "char " << objectNames[i] << "[" << result[i].size() << "] = \"";
				for ( std::vector<unsigned char>::iterator it = result[i].begin(), ie = result[i].end(); it != ie && *it != '\0'; it++ )
					d << escapeChar( *it );
				d << "\";" << std::endl;
				d << std::endl;
			}
			d << "// -----------------------------------------------------------------------------" << std::endl;
			solutions++;
			return true;
		} else {
			std::cerr << "	Redundant result." << std::endl;
			return false;
		}
	} else {
		std::cerr << "	Could not solve constraint." << std::endl;
		return false;
	}
}

int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Bad Input Generator" );

	assert( ClientPathFile.size() > 0 && "No client path file specified." );
	std::cerr << "Loading client path data." << std::endl;
	std::ifstream clientFile( ClientPathFile.getValue().c_str() );
	assert( clientFile.is_open() && "Unable to open path file." );
	SPA::PathLoader clientPathLoader( clientFile );
	clientPathLoader.setFilter( new ClientPathFilter() );
	unsigned long totalClientPaths = 0;
	while ( clientPathLoader.getPath() )
		totalClientPaths++;
	clientPathLoader.restart();
	std::cerr << "Found " << totalClientPaths << " initial paths." << std::endl;

	assert( ServerPathFile.size() > 0 && "No server path file specified." );
	std::cerr << "Loading server path data." << std::endl;
	std::ifstream serverFile( ServerPathFile.getValue().c_str() );
	assert( serverFile.is_open() && "Unable to open path file." );
	SPA::PathLoader serverPathLoader( serverFile );
	serverPathLoader.setFilter( new ServerPathFilter() );
	unsigned long totalServerPaths = 0;
	while ( serverPathLoader.getPath() )
		totalServerPaths++;
	serverPathLoader.restart();
	std::cerr << "Found " << totalServerPaths << " initial paths." << std::endl;

	if ( OutputFile.size() > 0 ) {
		output.open( OutputFile.getValue().c_str() );
	}

	if ( DebugFile.size() > 0 ) {
		debug.open( DebugFile.getValue().c_str() );
	}

	SPA::Path *cp = clientPathLoader.getPath(), *sp = serverPathLoader.getPath();
	assert( cp && sp );
	std::cerr << "Processing client path 1/" << totalClientPaths << " with server path 1/" << totalServerPaths << "." << std::endl;
	processPaths( cp, sp );

	unsigned long clientPath = 1;
	unsigned long serverPath = 1;
	unsigned long processed = 1;
	bool processing;
	do {
		processing = false;

		if ( (cp = clientPathLoader.getPath()) ) {
			clientPath++;
			serverPathLoader.restart();
			for ( unsigned long i = 1; i <= serverPath; i++ ) {
				assert( (sp = serverPathLoader.getPath()) );
				std::cerr << "Processing client path " << clientPath << "/" << totalClientPaths << " with server path " << i << "/" << totalServerPaths << " (pair " << processed << "/" << (totalClientPaths * totalServerPaths) << " - " << (processed * 100 / totalClientPaths / totalServerPaths) << "%). Found " << solutions << " solutions." << std::endl;
				processPaths( cp, sp );
				processed++;
			}
			processing = true;
		}

		if ( (sp = serverPathLoader.getPath()) ) {
			serverPath++;
			clientPathLoader.restart();
			for ( unsigned long i = 1; i <= clientPath; i++ ) {
				assert( (cp = clientPathLoader.getPath()) );
				std::cerr << "Processing client path " << i << "/" << totalClientPaths << " with server path " << serverPath << "/" << totalServerPaths << " (pair " << processed << "/" << (totalClientPaths * totalServerPaths) << " - " << (processed * 100 / totalClientPaths / totalServerPaths) << "%). Found " << solutions << " solutions." << std::endl;
				processPaths( cp, sp );
				processed++;
			}
			processing = true;
		}
	} while ( processing );
}
