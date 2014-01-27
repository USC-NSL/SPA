#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include <spa/SPA.h>

#define LOG_FILE_VARIABLE	"SPA_LOG_FILE"

#define LOG() \
	std::cerr << "[" << difftime( time( NULL ), programStartTime ) << "] "

time_t programStartTime;

bool processTestCase( char *clientCmd, char *serverCmd ) {
	std::string serverLog = tmpnam( NULL );
	LOG() << "Logging server results to: " << serverLog << std::endl;

	pid_t serverPID = fork();
	assert( serverPID >= 0 && "Error forking server process." );
	if ( serverPID == 0 ) {
		setpgid( 0, 0 );
		setenv( LOG_FILE_VARIABLE, serverLog.c_str(), 1 );
		LOG() << "Launching server: " << serverCmd << std::endl;
		exit( system( serverCmd ) );
	}
	
	std::string clientLog = tmpnam( NULL );
	LOG() << "Logging client results to: " << clientLog << std::endl;
	
	pid_t clientPID = fork();
	assert( clientPID >= 0 && "Error forking client process." );
	if ( clientPID == 0 ) {
		setpgid( 0, 0 );
		setenv( LOG_FILE_VARIABLE, clientLog.c_str(), 1 );
		sleep( 1 );
		LOG() << "Launching client: " << clientCmd << std::endl;
		exit( system( clientCmd ) );
	}
	
	sleep( 2 );
	LOG() << "Killing processes." << std::endl;
	kill( - serverPID, SIGTERM );
	kill( - clientPID, SIGTERM );
	
	int clientStatus = 0, serverStatus = 0;
	// 					LOG() << "Waiting for server." << std::endl;
	assert( waitpid( serverPID, &serverStatus, 0 ) != -1 );
	// 					LOG() << "Waiting for client." << std::endl;
	assert( waitpid( clientPID, &clientStatus, 0 ) != -1 );
	
	LOG() << "Processing outputs." << std::endl;
	std::ifstream logFile( clientLog.c_str() );
	// 					assert( logFile.is_open() && "Unable to open client log file." );
	bool clientValid = false;
	while ( logFile.good() ) {
		std::string line;
		std::getline( logFile, line );
		if ( line.compare( 0, strlen( SPA_TAG_PREFIX SPA_OUTPUT_TAG ), SPA_TAG_PREFIX SPA_OUTPUT_TAG ) == 0 ) {
			size_t boundary = line.find( ' ' );
			assert( boundary != std::string::npos && boundary < line.size() - 1 && "Malformed tag." );
			clientValid = (line.substr( boundary + 1 ) == SPA_OUTPUT_VALUE);
		}
	}
	logFile.close();
	remove( clientLog.c_str() );
	LOG() << "Client validity: " << clientValid << std::endl;
	
	logFile.open( serverLog.c_str() );
	assert( logFile.is_open() && "Unable to open server log file." );
	bool serverValid = false;
	while ( logFile.good() ) {
		std::string line;
		std::getline( logFile, line );
		if ( line.compare( 0, strlen( SPA_TAG_PREFIX SPA_VALIDPATH_TAG ), SPA_TAG_PREFIX SPA_VALIDPATH_TAG ) == 0 ) {
			size_t boundary = line.find( ' ' );
			assert( boundary != std::string::npos && boundary < line.size() - 1 && "Malformed tag." );
			serverValid = (line.substr( boundary + 1 ) == SPA_VALIDPATH_VALUE);
		}
	}
	logFile.close();
	remove( serverLog.c_str() );
	LOG() << "Server validity: " << serverValid << std::endl;
	
	return clientValid && (! serverValid);
}

int main(int argc, char **argv, char **envp) {
	if ( argc < 5 || argc > 7  ) {
		std::cerr << "Usage: " << argv[0] << " [-f] <input-file> <output-file> <client-cmd> <server-cmd> [reference-server-cmd]" << std::endl;

		return -1;
	}

	programStartTime = time( NULL );

	bool follow = false;

	unsigned int arg = 1;
	if ( std::string( "-f" ) == argv[arg] ) {
		follow = true;
		arg++;
	}
	char *inputFileName		= argv[arg++];
	char *outputFileName	= argv[arg++];
	char *clientCmd			= argv[arg++];
	char *serverCmd			= argv[arg++];
	char *refServerCmd		= NULL;
	if ( arg < (unsigned int) argc )
		refServerCmd = argv[arg++];

	// Load previously confirmed results to prevent redundant results.
	std::ifstream inputFile( outputFileName );
	std::string bundle;
	std::set<std::string> testedBundles;
	LOG() << "Loading previous results." << std::endl;
	while ( inputFile.good() ) {
		std::string line;
		std::getline( inputFile, line );

		if ( ! line.empty() ) {
			bundle += line + '\n';
		} else {
			if ( ! bundle.empty() ) {
				testedBundles.insert( bundle );
				bundle.clear();
			}
		}
	}
	inputFile.close();

	unsigned long numTestCases = 0;
	unsigned long truePositives = testedBundles.size();
	unsigned long falsePositives = 0;
	inputFile.open( inputFileName );
	std::ofstream outputFile( outputFileName, std::ios_base::app );

	while ( inputFile.good() || follow ) {
		if ( follow && ! inputFile.good() ) {
			LOG() << "Reached end of input. Sleeping." << std::endl;
			sleep( 1 );
			inputFile.clear();
			continue;
		}
		std::string line;
		std::getline( inputFile, line );

		if ( ! line.empty() ) {
			bundle += line + '\n';

			size_t d = line.find( ' ' );
			std::string name = line.substr( 0, d );
			assert( ! name.empty() && "Empty variable name." );
			std::string value;
			if ( d != std::string::npos )
				value = line.substr( d + 1, std::string::npos );
			setenv( name.c_str(), value.c_str(), 1 );
		} else {
			if ( ! bundle.empty() ) {
				numTestCases++;
				if ( testedBundles.count( bundle ) ) {
					LOG() << "Redundant test case. Ignoring." << std::endl;
				} else {
					LOG() << "Processing test case." << std::endl;
					testedBundles.insert( bundle );

					LOG() << "Testing cross-interoperability." << std::endl;
					bool crossInterop = processTestCase( clientCmd, serverCmd );
					LOG() << "Testing reference-interoperability." << std::endl;
					bool refInterop = refServerCmd != NULL ? processTestCase( clientCmd, refServerCmd ) : false;
					// Only consider interoperability bugs where the client and reference server assert valid but test server doesn't.
					if ( crossInterop && (! refInterop) ) {
						LOG() << "Found true positive. Outputting" << std::endl;
						truePositives++;
						outputFile << bundle << std::endl;
					} else {
						LOG() << "Found false positive. Filtering." << std::endl;
						falsePositives++;
					}
					LOG() << "--------------------------------------------------" << std::endl;
				}
				bundle.clear();
				LOG() << "Processed " << numTestCases << " test cases: " << truePositives << " true positives, " << falsePositives << " false positives." << std::endl;
			}
		}
	}
}
