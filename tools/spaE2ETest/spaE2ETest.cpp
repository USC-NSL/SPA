#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <cstdlib>


int main(int argc, char **argv, char **envp) {
	assert( argc == 5 );
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

				pid_t serverPID = fork();
				assert( serverPID >= 0 && "Error forking server process." );
				if ( serverPID == 0 ) {
					setpgid( 0, 0 );
					std::cerr << "Launching server: " << serverCmd << std::endl;
					exit( system( serverCmd ) );
				}

				pid_t clientPID = fork();
				assert( clientPID >= 0 && "Error forking client process." );
				if ( clientPID == 0 ) {
					setpgid( 0, 0 );
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

				if ( (! WIFEXITED( serverStatus )) || WEXITSTATUS( serverStatus ) == 201 ) {
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
