#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include <spa/SPA.h>

#define LOG_FILE_VARIABLE	"SPA_LOG_FILE"


int main(int argc, char **argv, char **envp) {
	if ( argc != 5 ) {
		std::cerr << "Usage: " << argv[0] << " <input-file> <output-file> <client-cmd> <server-cmd>" << std::endl;

		return -1;
	}
	char *inputFileName		= argv[1];
	char *outputFileName	= argv[2];
	char *clientCmd			= argv[3];
	char *serverCmd			= argv[4];

	std::ifstream inputFile( inputFileName );
	std::ofstream outputFile( outputFileName );

	std::string bundle;
	while ( inputFile.good() ) {
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
				std::cerr << "Processing test case." << std::endl;

				std::string serverLog = tmpnam( NULL );
				std::cerr << "Logging server results to: " << serverLog << std::endl;

				pid_t serverPID = fork();
				assert( serverPID >= 0 && "Error forking server process." );
				if ( serverPID == 0 ) {
					setpgid( 0, 0 );
					setenv( LOG_FILE_VARIABLE, serverLog.c_str(), 1 );
					std::cerr << "Launching server: " << serverCmd << std::endl;
					exit( system( serverCmd ) );
				}

				std::string clientLog = tmpnam( NULL );
				std::cerr << "Logging client results to: " << clientLog << std::endl;

				pid_t clientPID = fork();
				assert( clientPID >= 0 && "Error forking client process." );
				if ( clientPID == 0 ) {
					setpgid( 0, 0 );
					setenv( LOG_FILE_VARIABLE, clientLog.c_str(), 1 );
					sleep( 1 );
					std::cerr << "Launching client: " << clientCmd << std::endl;
					exit( system( clientCmd ) );
				}

				sleep( 2 );
				std::cerr << "Killing processes." << std::endl;
				kill( - serverPID, SIGTERM );
				kill( - clientPID, SIGTERM );

				int clientStatus = 0, serverStatus = 0;
// 				std::cerr << "Waiting for server." << std::endl;
				assert( waitpid( serverPID, &serverStatus, 0 ) != -1 );
// 				std::cerr << "Waiting for client." << std::endl;
				assert( waitpid( clientPID, &clientStatus, 0 ) != -1 );

				std::cerr << "Processing outputs." << std::endl;
				std::ifstream logFile( clientLog.c_str() );
				assert( logFile.is_open() && "Unable to open client log file." );
				bool clientValid = false;
				while ( logFile.good() ) {
					std::getline( logFile, line );
					if ( line.compare( 0, strlen( SPA_TAG_PREFIX SPA_VALIDPATH_TAG ), SPA_TAG_PREFIX SPA_VALIDPATH_TAG ) == 0 ) {
						size_t boundary = line.find( ' ' );
						assert( boundary != std::string::npos && boundary < line.size() - 1 && "Malformed tag." );
						clientValid = (line.substr( boundary + 1 ) == SPA_VALIDPATH_VALUE);
					}
				}
				logFile.close();
				remove( clientLog.c_str() );
				std::cerr << "Client validity: " << clientValid << std::endl;

				logFile.open( serverLog.c_str() );
				assert( logFile.is_open() && "Unable to open server log file." );
				bool serverValid = false;
				while ( logFile.good() ) {
					std::getline( logFile, line );
					if ( line.compare( 0, strlen( SPA_TAG_PREFIX SPA_VALIDPATH_TAG ), SPA_TAG_PREFIX SPA_VALIDPATH_TAG ) == 0 ) {
						size_t boundary = line.find( ' ' );
						assert( boundary != std::string::npos && boundary < line.size() - 1 && "Malformed tag." );
						serverValid = (line.substr( boundary + 1 ) == SPA_VALIDPATH_VALUE);
					}
				}
				logFile.close();
				remove( serverLog.c_str() );
				std::cerr << "Server validity: " << serverValid << std::endl;

				if ( (! clientValid) /*|| (! serverValid)*/ ) {
					std::cerr << "Found true positive. Outputting" << std::endl;
					outputFile << bundle << std::endl;
				} else {
					std::cerr << "Found false positive. Filtering." << std::endl;
				}
				bundle.clear();
				std::cerr << "--------------------------------------------------" << std::endl;
			}
		}
	}
}
