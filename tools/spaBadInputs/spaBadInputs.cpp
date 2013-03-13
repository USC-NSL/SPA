#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/wait.h>
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


#define DEFAULT_NUM_JOBS_PER_WORKER	100
#define DEFAULT_NUM_WORKERS			1
#define CHECKPOINT_TIME				60 // seconds

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

	llvm::cl::opt<int> NumWorkers("p", llvm::cl::desc(
		"Number of parallel workers to create."));

	llvm::cl::opt<int> NumJobsPerWorker("j", llvm::cl::desc(
		"Number of jobs to give to each worker."));
}

typedef struct {
	unsigned long solutions;
} GlobalData_t;

typedef struct {
	SPA::Path *cp;
	bool deletecp;
	SPA::Path *sp;
	bool deletesp;
} job_t;

std::ofstream output, debug;
unsigned int numWorkers, numJobsPerWorker, runningWorkers = 0;
volatile bool shutdown = false;
std::list<job_t *> queue;
GlobalData_t *gd = NULL;

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

std::string escapeChar( unsigned char c ) {
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

void processPaths( SPA::Path *cp, SPA::Path *sp ) {
	// Check if valid transaction.
	if ( cp->getTag( SPA_VALIDPATH_TAG ) == SPA_VALIDPATH_VALUE && sp->getTag( SPA_VALIDPATH_TAG ) == SPA_VALIDPATH_VALUE ) {
		std::cerr << "	Not invalid path pair." << std::endl;
		return;
	}

	klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();
	klee::Solver *solver =
		klee::createIndependentSolver(
			klee::createCachingSolver(
				klee::createCexCachingSolver(
					new klee::STPSolver( false, true ) ) ) );

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
		// Check if input.
		if ( it->first.compare( 0, strlen( SPA_INPUT_PREFIX ), SPA_INPUT_PREFIX ) == 0 ) {
			// Check if message.
			if ( it->first.compare( 0, strlen( SPA_MESSAGE_INPUT_PREFIX ), SPA_MESSAGE_INPUT_PREFIX ) == 0 ) {
				std::string type = it->first.substr( strlen( SPA_MESSAGE_INPUT_PREFIX ), std::string::npos );
				const klee::Array *clientIn = it->second;
				std::string serverOutName = std::string( SPA_MESSAGE_OUTPUT_PREFIX ) + type;
				if ( sp->hasOutput( serverOutName ) ) {
// 					std::cerr << "	Connecting server-to-client message type: " << type << std::endl;
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
							return;
						}
					}
				} else {
// 					std::cerr << "	Client consumes a message type not generated by server: " << it->first << ". Leaving as unknown."<< std::endl;
					objectNames.push_back( it->first );
					objects.push_back( it->second );
				}
			} else { // Not a message (API): solve as an unknown.
// 				std::cerr << "	Free client input: " << it->first << ". Leaving as unknown."<< std::endl;
				objectNames.push_back( it->first );
				objects.push_back( it->second );
			}
		}
		// Check if constraints were added at initialization.
		std::string initOutName = SPA_INIT_PREFIX + it->first.substr( strlen( SPA_PREFIX ), std::string::npos );
		if ( cp->hasOutput( initOutName ) ) {
// 			std::cerr << "	Adding user-defined client constraints for: " << it->first << std::endl;
			const klee::Array *clientIn = it->second;
			assert( clientIn->size == cp->getOutputSize( initOutName ) && "Constraint size mismatch." );
			// Add server output values = client input array constraint.
			for ( size_t offset = 0; offset < cp->getOutputSize( initOutName ); offset++ ) {
				klee::UpdateList ul( clientIn, 0 );
				klee::ref<klee::Expr> e = exprBuilder->Eq(
					exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
					cp->getOutputValue( initOutName, offset ) );
				if ( ! cm.addAndCheckConstraint( e ) ) {
					std::cerr << "	Constraints trivially UNSAT." << std::endl;
//					showConstraints( cm );
//					std::cerr << "		New constraint:" << std::endl << e << std::endl;
					return;
				}
			}
		}
	}
	// Process server inputs.
	for ( std::map<std::string, const klee::Array *>::const_iterator it = sp->beginSymbols(), ie = sp->endSymbols(); it != ie; it++ ) {
		// Check if input.
		if ( it->first.compare( 0, strlen( SPA_INPUT_PREFIX ), SPA_INPUT_PREFIX ) == 0 ) {
			// Check if message.
			if ( it->first.compare( 0, strlen( SPA_MESSAGE_INPUT_PREFIX ), SPA_MESSAGE_INPUT_PREFIX ) == 0 ) {
				std::string type = it->first.substr( strlen( SPA_MESSAGE_INPUT_PREFIX ), std::string::npos );
				const klee::Array *serverIn = it->second;
				std::string clientOutName = std::string( SPA_MESSAGE_OUTPUT_PREFIX ) + type;
				// Check if client outputs this message type.
				if ( cp->hasOutput( clientOutName ) ) {
// 					std::cerr << "	Connecting client-to-server message type: " << type << std::endl;
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
							return;
						}
						connected = true;
					}
				} else {
// 					std::cerr << "	Server consumes a message type not generated by client: " << it->first << ". Leaving as unknown."<< std::endl;
					objectNames.push_back( it->first );
					objects.push_back( it->second );
				}
			} else { // Not a message (API): solve as an unknown.
// 				std::cerr << "	Free server input: " << it->first << ". Leaving as unknown."<< std::endl;
				objectNames.push_back( it->first );
				objects.push_back( it->second );
			}
		}
		// Check if constraints were added at initialization.
		std::string initOutName = SPA_INIT_PREFIX + it->first.substr( strlen( SPA_PREFIX ), std::string::npos );
		if ( sp->hasOutput( initOutName ) ) {
// 			std::cerr << "	Adding user-defined server constraints for: " << it->first << std::endl;
			const klee::Array *serverIn = it->second;
			assert( serverIn->size == sp->getOutputSize( initOutName ) && "Constraint size mismatch." );
			// Add server output values = client input array constraint.
			for ( size_t offset = 0; offset < sp->getOutputSize( initOutName ); offset++ ) {
				klee::UpdateList ul( serverIn, 0 );
				klee::ref<klee::Expr> e = exprBuilder->Eq(
					exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
					sp->getOutputValue( initOutName, offset ) );
				if ( ! cm.addAndCheckConstraint( e ) ) {
					std::cerr << "	Constraints trivially UNSAT." << std::endl;
//					showConstraints( cm );
//					std::cerr << "		New constraint:" << std::endl << e << std::endl;
					return;
				}
			}
		}
	}

	if ( ! connected ) {
		std::cerr << "	Client and server not connected." << std::endl;
		return;
	}

// 	std::cerr << "	Path pair constraints:" << std::endl;
// 	showConstraints( cm );

	// Add client handler id, to reproduce test later.
	objectNames.push_back( SPA_HANDLERID_VARIABLE );
	objects.push_back( cp->getSymbol( SPA_HANDLERID_VARIABLE ) );

	std::vector< std::vector<unsigned char> > result;
	if ( solver->getInitialValues( klee::Query( cm, exprBuilder->False() ), objects, result ) ) {
		int fd = open( OutputFile.getValue().c_str(), O_APPEND );
		assert( fd >= 0 && "Unable to open output file for locking." );
		assert( flock( fd, LOCK_EX ) == 0 && "Unable to lock output file." );
		std::cerr << "	Found solution." << std::endl;
		gd->solutions++;

		std::ostream &o = output.is_open() ? output : std::cout;
		std::ostream &d = debug.is_open() ? debug : std::cout;

		for ( size_t i = 0; i < result.size(); i++ ) {
// 				std::cerr << "	Outputting solution for: " << objectNames[i] << std::endl;
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

		assert( flock( fd, LOCK_UN ) == 0 && "Unable to unlock output file." );
		assert( close( fd ) == 0 && "Unable to close output file for locking." );
	} else {
		std::cerr << "	Could not solve constraint." << std::endl;
	}
}

void handleSignal( int n ) {
	std::cerr << "Finishing current jobs." << std::endl;
	shutdown = true;
}

void processJob( SPA::Path *cp, SPA::Path *sp, bool deletecp, bool deletesp ) {
	if ( numWorkers > 1 ) {
		// If no job specified, then just drain the queue.
		if ( cp && sp ) {
			job_t *j = new job_t();
			j->cp = cp;
			j->deletecp = deletecp;
			j->sp = sp;
			j->deletesp = deletesp;
			queue.push_back( j );

			if ( queue.size() < numJobsPerWorker )
				return;
		}

		if ( queue.size() == 0 )
			return;

		while ( runningWorkers >= numWorkers ) {
			int status = 0;
			assert ( wait( &status ) > 0 );
			runningWorkers--;
		}

		int pid;
		if ( (pid = fork()) < 0 ) {
			// Unable to fork: wait for remaining workers.
			std::cerr << "Error forking worker process. Waiting on running jobs." << std::endl;
			while ( runningWorkers > 0 ) {
				int status = 0;
				assert( wait( &status ) > 0 );
				runningWorkers--;
			}

			for ( std::list<job_t *>::iterator it = queue.begin(), ie = queue.end(); it != ie; it++ ) {
				processPaths( (*it)->cp, (*it)->sp );
				if ( (*it)->deletecp )
					delete (*it)->cp;
				if ( (*it)->deletesp )
					delete (*it)->sp;
				delete *it;
			}
			queue.clear();
		} else if ( pid == 0 ) {
			signal( SIGINT, SIG_IGN );
			for ( std::list<job_t *>::iterator it = queue.begin(), ie = queue.end(); it != ie; it++ )
				processPaths( (*it)->cp, (*it)->sp );
			exit( 0 );
		} else {
			signal( SIGINT, handleSignal );

			runningWorkers++;

			for ( std::list<job_t *>::iterator it = queue.begin(), ie = queue.end(); it != ie; it++ ) {
				if ( (*it)->deletecp )
					delete (*it)->cp;
				if ( (*it)->deletesp )
					delete (*it)->sp;
				delete *it;
			}

			queue.clear();
		}
	} else {
		if ( cp && sp )
			processPaths( cp, sp );
	}
}

void flushJobs() {
	processJob( NULL, NULL, false, false );

	while ( runningWorkers > 0 ) {
		int status = 0;
		assert( wait( &status ) > 0 );
		runningWorkers--;
	}
}

bool checkpoint( std::string cpFileName, unsigned long &clientPath, unsigned long &serverPath, unsigned long &processed ) {
	static time_t last = 0;
	time_t now = time( NULL );

	if ( shutdown || difftime( now, last ) > CHECKPOINT_TIME ) {
		flushJobs();
		last = time( NULL );
		{
			std::ifstream cpFile( cpFileName.c_str() );
			unsigned long c = 0, s = 0, p = 0;
			cpFile >> c >> s >> p;
			if ( cpFile.is_open() && (c > clientPath || s > serverPath || p > processed) ) {
				std::cerr << "Restarting from checkpoint. Expect some redundant results." << std::endl;
				clientPath = c;
				serverPath = s;
				processed = p;
				return true;
			}
		}
		{
			std::ofstream cpFile( cpFileName.c_str() );
			assert( cpFile.is_open() && "Unable to open checkpoint file." );
			cpFile << clientPath << " " << serverPath << " " << processed << std::endl;
			cpFile.flush();
			cpFile.close();
			return false;
		}
	}

	return false;
}

int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Bad Input Generator" );

	SPA::Path *cp, *sp;

	assert( ClientPathFile.size() > 0 && "No client path file specified." );
	std::cerr << "Loading client path data." << std::endl;
	std::ifstream clientFile( ClientPathFile.getValue().c_str() );
	assert( clientFile.is_open() && "Unable to open path file." );
	SPA::PathLoader clientPathLoader( clientFile );
	clientPathLoader.setFilter( new ClientPathFilter() );
	unsigned long totalClientPaths = 0;
	while ( (cp = clientPathLoader.getPath()) ) {
		delete cp;
		totalClientPaths++;
	}
	clientPathLoader.restart();
	std::cerr << "Found " << totalClientPaths << " initial paths." << std::endl;

	assert( ServerPathFile.size() > 0 && "No server path file specified." );
	std::cerr << "Loading server path data." << std::endl;
	std::ifstream serverFile( ServerPathFile.getValue().c_str() );
	assert( serverFile.is_open() && "Unable to open path file." );
	SPA::PathLoader serverPathLoader( serverFile );
	serverPathLoader.setFilter( new ServerPathFilter() );
	unsigned long totalServerPaths = 0;
	while ( (sp = serverPathLoader.getPath()) ) {
		delete sp;
		totalServerPaths++;
	}
	serverPathLoader.restart();
	std::cerr << "Found " << totalServerPaths << " initial paths." << std::endl;

	assert( OutputFile.size() > 0 && "No output file specified." );
	output.open( OutputFile.getValue().c_str(), std::ios_base::out | std::ios_base::app );
	assert( output.is_open() && "Unable to open output file." );
	std::string cpFileName = OutputFile.getValue() + ".checkpoint";

	if ( DebugFile.size() > 0 ) {
		debug.open( DebugFile.getValue().c_str() );
	}

	gd = (GlobalData_t *) mmap( NULL, sizeof( GlobalData_t ), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0 );
	gd->solutions = 0;

	assert( NumWorkers >= 0 && "Invalid number of parallel workers." );
	numWorkers = NumWorkers > 0 ? NumWorkers : DEFAULT_NUM_WORKERS;
	if ( numWorkers > 1 )
		std::cerr << "Using " << NumWorkers << " worker processes." << std::endl;
	else
		std::cerr << "Not using worker processes." << std::endl;

	assert( NumJobsPerWorker >= 0 && "Invalid number of jobs per workers." );
	numJobsPerWorker = NumJobsPerWorker > 0 ? NumJobsPerWorker : DEFAULT_NUM_JOBS_PER_WORKER;

	while ( (! (cp = clientPathLoader.getPath())) && Follow ) {
		std::cerr << "Client path file is empty. Sleeping." << std::endl;
		sleep( 1 );
	}

	while ( (! (sp = serverPathLoader.getPath())) && Follow ) {
		std::cerr << "Server path file is empty. Sleeping." << std::endl;
		sleep( 1 );
	}

	assert( cp && sp );
	std::cerr << "Processing client path 1/" << totalClientPaths << " with server path 1/" << totalServerPaths << "." << std::endl;
	processJob( cp, sp, true, true );

	signal( SIGINT, handleSignal );

	unsigned long clientPath = 1;
	unsigned long serverPath = 1;
	unsigned long processed = 1;
	bool processing;
	do {
		processing = false;
		// Save a checkpoint to recover from failure.
		if ( checkpoint( cpFileName, clientPath, serverPath, processed ) ) {
			clientPathLoader.restart();
			for ( unsigned long i = 1; i <= clientPath; i++ )
				assert( (cp = clientPathLoader.getPath()) && "Unable to load client path file to checkpoint position.");
			for ( unsigned long i = 1; i <= serverPath; i++ )
				assert( (sp = serverPathLoader.getPath()) && "Unable to load server path file to checkpoint position.");
		}
		if ( shutdown ) {
			std::cerr << "Interrupted cleanly." << std::endl;
			exit( 0 );
		}

		if ( (cp = clientPathLoader.getPath()) ) {
			clientPath++;
			serverPathLoader.restart();
			for ( unsigned long i = 1; i <= serverPath; i++ ) {
				assert( (sp = serverPathLoader.getPath()) );
				std::cerr << "Processing client path " << clientPath << "/" << totalClientPaths << " with server path " << i << "/" << totalServerPaths << " (pair " << processed << "/" << (totalClientPaths * totalServerPaths) << " - " << (processed * 100 / totalClientPaths / totalServerPaths) << "%). Found " << gd->solutions << " solutions." << std::endl;
				processJob( cp, sp, i == serverPath, true );
				processed++;
			}
			processing = true;
		}

		if ( (sp = serverPathLoader.getPath()) ) {
			serverPath++;
			clientPathLoader.restart();
			for ( unsigned long i = 1; i <= clientPath; i++ ) {
				assert( (cp = clientPathLoader.getPath()) );
				std::cerr << "Processing client path " << i << "/" << totalClientPaths << " with server path " << serverPath << "/" << totalServerPaths << " (pair " << processed << "/" << (totalClientPaths * totalServerPaths) << " - " << (processed * 100 / totalClientPaths / totalServerPaths) << "%). Found " << gd->solutions << " solutions." << std::endl;
				processJob( cp, sp, true, i == clientPath );
				processed++;
			}
			processing = true;
		}

		if ( ! processing && Follow ) {
			processJob( NULL, NULL, false, false );
			std::cerr << "Reached end of path files, sleeping." << std::endl;
			sleep( 1 );
		}
	} while ( processing || Follow );
	flushJobs();
	remove( cpFileName.c_str() );
	std::cerr << "Done. Found " << gd->solutions << " solutions." << std::endl;
}
