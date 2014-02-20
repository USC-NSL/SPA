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


#define DEFAULT_NUM_JOBS_PER_WORKER		1000
#define DEFAULT_NUM_WORKERS				1
#define DEFAULT_EXPANSION_FRONT_WIDTH	1
#define CHECKPOINT_TIME				600 // seconds

#define LOG() \
	std::cerr << "[" << difftime( time( NULL ), programStartTime ) << "] "

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

	llvm::cl::opt<int> ExpansionFrontWidth("w", llvm::cl::desc(
		"The width of the expansion front."));
}

typedef struct {
	unsigned long solutions;
	unsigned long cpuTime;
} GlobalData_t;

typedef struct {
	SPA::Path *cp;
	bool deletecp;
	SPA::Path *sp;
	bool deletesp;
} job_t;

std::ofstream output, debug;
unsigned int numWorkers, numJobsPerWorker, expansionFrontWidth, runningWorkers = 0;
volatile bool shutdown = false;
std::list<job_t *> queue;
GlobalData_t *gd = NULL;
time_t programStartTime;

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
// 		LOG() << *it << std::endl;
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
// 		LOG() << "	Not invalid path pair." << std::endl;
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
// 					LOG() << "	Connecting server-to-client message type: " << type << std::endl;
					assert( clientIn->size >= sp->getOutputSize( serverOutName ) && "Client and server message size mismatch." );
					// Add server output values = client input array constraint.
					for ( size_t offset = 0; offset < sp->getOutputSize( serverOutName ); offset++ ) {
						klee::UpdateList ul( clientIn, 0 );
						klee::ref<klee::Expr> e = exprBuilder->Eq(
							exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
							sp->getOutputValue( serverOutName, offset ) );
						if ( ! cm.addAndCheckConstraint( e ) ) {
// 							LOG() << "	Constraints trivially UNSAT." << std::endl;
// 							showConstraints( cm );
// 							LOG() << "		New constraint:" << std::endl << e << std::endl;
							return;
						}
					}
				} else {
// 					LOG() << "	Client consumes a message type not generated by server: " << it->first << ". Leaving as unknown."<< std::endl;
					objectNames.push_back( it->first );
					objects.push_back( it->second );
				}
			} else { // Not a message (API): solve as an unknown.
// 				LOG() << "	Free client input: " << it->first << ". Leaving as unknown."<< std::endl;
				objectNames.push_back( it->first );
				objects.push_back( it->second );
			}
		}
		// Check if constraints were added at initialization.
		std::string initOutName = SPA_INIT_PREFIX + it->first.substr( strlen( SPA_PREFIX ), std::string::npos );
		if ( cp->hasOutput( initOutName ) ) {
// 			LOG() << "	Adding user-defined client constraints for: " << it->first << std::endl;
			const klee::Array *clientIn = it->second;
			assert( clientIn->size == cp->getOutputSize( initOutName ) && "Constraint size mismatch." );
			// Add server output values = client input array constraint.
			for ( size_t offset = 0; offset < cp->getOutputSize( initOutName ); offset++ ) {
				klee::UpdateList ul( clientIn, 0 );
				klee::ref<klee::Expr> e = exprBuilder->Eq(
					exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
					cp->getOutputValue( initOutName, offset ) );
				if ( ! cm.addAndCheckConstraint( e ) ) {
// 					LOG() << "	Constraints trivially UNSAT." << std::endl;
//					showConstraints( cm );
//					LOG() << "		New constraint:" << std::endl << e << std::endl;
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
// 					LOG() << "	Connecting client-to-server message type: " << type << std::endl;
					assert( serverIn->size >= cp->getOutputSize( clientOutName ) && "Client and server message size mismatch." );
					// Add client output values = server input array constraint.
					for ( size_t offset = 0; offset < cp->getOutputSize( clientOutName ); offset++ ) {
						klee::UpdateList ul( serverIn, 0 );
						klee::ref<klee::Expr> e = exprBuilder->Eq(
							exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
							cp->getOutputValue( clientOutName, offset ) );
						if ( ! cm.addAndCheckConstraint( e ) ) {
// 							LOG() << "	Constraints trivially UNSAT." << std::endl;
// 							showConstraints( cm );
// 							LOG() << "		New constraint:" << std::endl << e << std::endl;
							return;
						}
						connected = true;
					}
				} else {
// 					LOG() << "	Server consumes a message type not generated by client: " << it->first << ". Leaving as unknown."<< std::endl;
					objectNames.push_back( it->first );
					objects.push_back( it->second );
				}
			} else { // Not a message (API): solve as an unknown.
// 				LOG() << "	Free server input: " << it->first << ". Leaving as unknown."<< std::endl;
				objectNames.push_back( it->first );
				objects.push_back( it->second );
			}
		}
		// Check if constraints were added at initialization.
		std::string initOutName = SPA_INIT_PREFIX + it->first.substr( strlen( SPA_PREFIX ), std::string::npos );
		if ( sp->hasOutput( initOutName ) ) {
// 			LOG() << "	Adding user-defined server constraints for: " << it->first << std::endl;
			const klee::Array *serverIn = it->second;
			assert( serverIn->size == sp->getOutputSize( initOutName ) && "Constraint size mismatch." );
			// Add server output values = client input array constraint.
			for ( size_t offset = 0; offset < sp->getOutputSize( initOutName ); offset++ ) {
				klee::UpdateList ul( serverIn, 0 );
				klee::ref<klee::Expr> e = exprBuilder->Eq(
					exprBuilder->Read( ul, exprBuilder->Constant( offset, klee::Expr::Int32 ) ),
					sp->getOutputValue( initOutName, offset ) );
				if ( ! cm.addAndCheckConstraint( e ) ) {
// 					LOG() << "	Constraints trivially UNSAT." << std::endl;
//					showConstraints( cm );
//					LOG() << "		New constraint:" << std::endl << e << std::endl;
					return;
				}
			}
		}
	}

	if ( ! connected ) {
// 		LOG() << "	Client and server not connected." << std::endl;
		return;
	}

// 	LOG() << "	Path pair constraints:" << std::endl;
// 	showConstraints( cm );

	// Add client handler id, to reproduce test later.
	objectNames.push_back( SPA_HANDLERID_VARIABLE );
	objects.push_back( cp->getSymbol( SPA_HANDLERID_VARIABLE ) );

	std::vector< std::vector<unsigned char> > result;
	if ( solver->getInitialValues( klee::Query( cm, exprBuilder->False() ), objects, result ) ) {
		int fd = open( OutputFile.getValue().c_str(), O_APPEND );
		assert( fd >= 0 && "Unable to open output file for locking." );
		assert( flock( fd, LOCK_EX ) == 0 && "Unable to lock output file." );
// 		LOG() << "	Found solution." << std::endl;
		gd->solutions++;

		std::ostream &o = output.is_open() ? output : std::cout;
		std::ostream &d = debug.is_open() ? debug : std::cout;

		for ( size_t i = 0; i < result.size(); i++ ) {
// 				LOG() << "	Outputting solution for: " << objectNames[i] << std::endl;
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
// 		LOG() << "	Could not solve constraint." << std::endl;
	}
}

void handleSignal( int n ) {
	LOG() << "Finishing current jobs." << std::endl;
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
		assert( (pid = fork()) >= 0 && "Error spawning worker process." );
		if ( pid == 0 ) {
			signal( SIGINT, SIG_IGN );
			time_t startTime = time( NULL );
			for ( std::list<job_t *>::iterator it = queue.begin(), ie = queue.end(); it != ie; it++ )
				processPaths( (*it)->cp, (*it)->sp );
			time_t now = time( NULL );

			int fd = open( OutputFile.getValue().c_str(), O_APPEND );
			assert( fd >= 0 && "Unable to open output file for locking." );
			assert( flock( fd, LOCK_EX ) == 0 && "Unable to lock output file." );
			gd->cpuTime += difftime( now, startTime );
			assert( flock( fd, LOCK_UN ) == 0 && "Unable to unlock output file." );
			assert( close( fd ) == 0 && "Unable to close output file for locking." );

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

	LOG() << "Flushing jobs." << std::endl;
	while ( runningWorkers > 0 ) {
		int status = 0;
		assert( wait( &status ) > 0 );
		runningWorkers--;
	}
}

bool saveCheckpoint( std::string cpFileName,
		unsigned long clientPath, unsigned long serverPath,
		unsigned long solutions,
		unsigned long totalClientPaths, unsigned long totalServerPaths,
		unsigned long cpuTime ) {
	static time_t last = 0;
	time_t now = time( NULL );

	if ( shutdown || difftime( now, last ) > CHECKPOINT_TIME ) {
		LOG() << "Waiting to save checkpoint." << std::endl;
		flushJobs();
		last = time( NULL );
		std::ofstream cpFile( cpFileName.c_str() );
		assert( cpFile.is_open() && "Unable to open checkpoint file." );
		cpFile << clientPath << " " << serverPath << " " << totalClientPaths << " " << totalServerPaths << " " << solutions << " " << cpuTime << std::endl;
		cpFile.flush();
		cpFile.close();
		LOG() << "Checkpoint saved." << std::endl;
		return true;
	}

	return false;
}

bool restoreCheckpoint( std::string cpFileName,
		unsigned long &clientPath, unsigned long &serverPath,
		unsigned long &processed, unsigned long &solutions,
		unsigned long &totalClientPaths, unsigned long &totalServerPaths,
		unsigned long &cpuTime ) {
	std::ifstream cpFile( cpFileName.c_str() );
	if ( cpFile.is_open() ) {
		LOG() << "Restarting from checkpoint. Expect some redundant results." << std::endl;
		cpFile >> clientPath >> serverPath >> totalClientPaths >> totalServerPaths >> solutions >> cpuTime;
		processed = clientPath * serverPath;
		return true;
	}

	return false;
}

int main(int argc, char **argv, char **envp) {
	// Fill up every global cl::opt object declared in the program
	cl::ParseCommandLineOptions( argc, argv, "Systematic Protocol Analyzer - Bad Input Generator" );

	assert( ClientPathFile.size() > 0 && "No client path file specified." );
	std::ifstream clientFile( ClientPathFile.getValue().c_str() );
	assert( clientFile.is_open() && "Unable to open path file." );

	assert( ServerPathFile.size() > 0 && "No server path file specified." );
	std::ifstream serverFile( ServerPathFile.getValue().c_str() );
	assert( serverFile.is_open() && "Unable to open path file." );

	assert( OutputFile.size() > 0 && "No output file specified." );
	output.open( OutputFile.getValue().c_str(), std::ios_base::out | std::ios_base::app );
	assert( output.is_open() && "Unable to open output file." );
	std::string cpFileName = OutputFile.getValue() + ".checkpoint";

	if ( DebugFile.size() > 0 ) {
		debug.open( DebugFile.getValue().c_str() );
	}

	programStartTime = time( NULL );

	gd = (GlobalData_t *) mmap( NULL, sizeof( GlobalData_t ), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0 );
	gd->solutions = 0;
	gd->cpuTime = 0;

	assert( NumWorkers >= 0 && "Invalid number of parallel workers." );
	numWorkers = NumWorkers > 0 ? NumWorkers : DEFAULT_NUM_WORKERS;
	if ( numWorkers > 1 )
		LOG() << "Using " << NumWorkers << " worker processes." << std::endl;
	else
		LOG() << "Not using worker processes." << std::endl;

	assert( NumJobsPerWorker >= 0 && "Invalid number of jobs per workers." );
	numJobsPerWorker = NumJobsPerWorker > 0 ? NumJobsPerWorker : DEFAULT_NUM_JOBS_PER_WORKER;

	assert( ExpansionFrontWidth >= 0 && "Invalid expansion front width." );
	expansionFrontWidth = ExpansionFrontWidth > 0 ? ExpansionFrontWidth : DEFAULT_EXPANSION_FRONT_WIDTH;

	signal( SIGINT, handleSignal );

	while ( true ) {
		int pid;
		assert( (pid = fork()) >= 0 && "Error spawning work coordinator process." );
		if ( pid == 0 ) { // Child process: work coordinator.
			SPA::PathLoader clientPathLoader( clientFile );
			clientPathLoader.setFilter( new ClientPathFilter() );
			clientPathLoader.restart();
			SPA::PathLoader serverPathLoader( serverFile );
			serverPathLoader.setFilter( new ServerPathFilter() );
			serverPathLoader.restart();

			SPA::Path *cp, *sp;
			unsigned long clientPath = 0, serverPath = 0;
			unsigned long totalClientPaths = 0, totalServerPaths;
			unsigned long processed;
			if ( restoreCheckpoint( cpFileName, clientPath, serverPath, processed, gd->solutions, totalClientPaths, totalServerPaths, gd->cpuTime ) ) {
				LOG() << "Restoring client path." << std::endl;
				for ( unsigned long i = 1; i <= clientPath; i++ )
					assert( (cp = clientPathLoader.getPath()) && "Unable to load client path file to checkpoint position.");
				LOG() << "Restoring server path." << std::endl;
				for ( unsigned long i = 1; i <= serverPath; i++ )
					assert( (sp = serverPathLoader.getPath()) && "Unable to load server path file to checkpoint position.");
			} else {
				while ( (! (cp = clientPathLoader.getPath())) && Follow ) {
					LOG() << "Client path file is empty. Sleeping." << std::endl;
					sleep( 1 );
				}
				while ( (! (sp = serverPathLoader.getPath())) && Follow ) {
					LOG() << "Server path file is empty. Sleeping." << std::endl;
					sleep( 1 );
				}
				assert( cp && sp && "Empty path file.");

				totalClientPaths = 1;
				while ( (cp = clientPathLoader.getPath()) ) {
					delete cp;
					totalClientPaths++;
				}
				clientPathLoader.restart();
				LOG() << "Found " << totalClientPaths << " initial client paths." << std::endl;

				totalServerPaths = 1;
				while ( (sp = serverPathLoader.getPath()) ) {
					delete sp;
					totalServerPaths++;
				}
				serverPathLoader.restart();
				LOG() << "Found " << totalServerPaths << " initial server paths." << std::endl;

				LOG() << "Processing client path 1/" << totalClientPaths << " with server path 1/" << totalServerPaths << "." << std::endl;
				cp = clientPathLoader.getPath();
				sp = serverPathLoader.getPath();
				assert( cp && sp && "Empty path file.");
				clientPath = 1;
				serverPath = 1;
				processJob( cp, sp, true, true );
				processed = 1;
			}

			bool processing;
			do {
				processing = false;
				// Save a checkpoint to recover from failure.
				saveCheckpoint( cpFileName, clientPath, serverPath, gd->solutions, totalClientPaths, totalServerPaths, gd->cpuTime );

				std::vector<SPA::Path *> expansionFrontPaths;
				while ( expansionFrontPaths.size() < expansionFrontWidth && (cp = clientPathLoader.getPath()) ) {
					expansionFrontPaths.push_back( cp );
					clientPath++;
					if ( clientPath > totalClientPaths )
						totalClientPaths = clientPath;
				}

				if ( ! expansionFrontPaths.empty() ) {
					serverPathLoader.restart();
					for ( unsigned long i = 1; i <= serverPath; i++ ) {
						assert( (sp = serverPathLoader.getPath()) );
						for ( unsigned long j = 0; j < expansionFrontPaths.size(); j++ ) {
							cp = expansionFrontPaths[j];
							LOG() << "Processing client path " << (clientPath - expansionFrontPaths.size() + j + 1) << "/" << totalClientPaths << " with server path " << i << "/" << totalServerPaths << " (pair " << processed << "/" << (totalClientPaths * totalServerPaths) << " - " << (processed * 100 / totalClientPaths / totalServerPaths) << "%). Found " << gd->solutions << " solutions." << std::endl;
							processJob( cp, sp, i == serverPath, j == expansionFrontPaths.size() - 1 );
							processed++;
						}
					}
					processing = true;
				}

				expansionFrontPaths.clear();
				while ( expansionFrontPaths.size() < expansionFrontWidth && (sp = serverPathLoader.getPath()) ) {
					expansionFrontPaths.push_back( sp );
					serverPath++;
					if ( serverPath > totalServerPaths )
						totalServerPaths = serverPath;
				}

				if ( ! expansionFrontPaths.empty() ) {
					clientPathLoader.restart();
					for ( unsigned long i = 1; i <= clientPath; i++ ) {
						assert( (cp = clientPathLoader.getPath()) );
						for ( unsigned long j = 0; j < expansionFrontPaths.size(); j++ ) {
							sp = expansionFrontPaths[j];
							LOG() << "Processing client path " << i << "/" << totalClientPaths << " with server path " << (serverPath - expansionFrontPaths.size() + j + 1) << "/" << totalServerPaths << " (pair " << processed << "/" << (totalClientPaths * totalServerPaths) << " - " << (processed * 100 / totalClientPaths / totalServerPaths) << "%). Found " << gd->solutions << " solutions." << std::endl;
							processJob( cp, sp, j == expansionFrontPaths.size(), i == clientPath );
							processed++;
						}
					}
					processing = true;
				}

				if ( ! processing && Follow ) {
					processJob( NULL, NULL, false, false );
					LOG() << "Reached end of path files, sleeping." << std::endl;
					sleep( 1 );
				}
			} while ( processing || Follow );
			flushJobs();
			remove( cpFileName.c_str() );
			exit( 0 );
		} else { // Parent process: re-spawner.
			int status = 0;
			time_t startTime = time( NULL );
			assert ( waitpid( pid, &status, 0 ) > 0 );
			if ( WIFEXITED( status ) && WEXITSTATUS( status ) == 0 ) {
				time_t now = time( NULL );
				LOG() << "Done. Found " << gd->solutions << " solutions." << std::endl;
				LOG() << "Elapsed Time: " << difftime( now, startTime ) << ", CPU Time:" << gd->cpuTime << std::endl;
				break;
			}
			if ( shutdown ) {
				LOG() << "Interrupted cleanly." << std::endl;
				break;
			}
			LOG() << "Coordinator process died. Re-spawning." << std::endl;
		}
	}
}
