#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <unistd.h>

#include <spa/SPA.h>

#define RECHECK_WAIT    100000 // us
#define LISTEN_TIMEOUT 5000000 // us
#define FINISH_TIMEOUT 5000000 // us
#define LOGS_TIMEOUT   1000000 // us

#define LOG_FILE_VARIABLE	"SPA_LOG_FILE"

#define TCP_LISTEN 10

#define LOG() \
std::cerr << "[" << (std::chrono::duration_cast<std::chrono::milliseconds>( \
                      std::chrono::steady_clock::now() \
                        - programStartTime).count() / 1000.0) << "] "

auto programStartTime = std::chrono::steady_clock::now();

void waitListen(uint16_t port) {
  LOG() << "Waiting for server to listen on port " << port << "." << std::endl;

  auto start_time = std::chrono::steady_clock::now();

  do {
    std::ifstream tcp_table("/proc/net/tcp");
    std::string line;
    std::getline( tcp_table, line ); // Ignore table header.
    while (tcp_table.good()) {
      std::getline( tcp_table, line );
      if (! line.empty()) {
        uint16_t local_port, state;

        std::istringstream tokenizer(line);
        std::string token;
        tokenizer >> token >> token;
        std::istringstream hexconverter(token.substr(token.find(':') + 1));
        hexconverter >> std::hex >> local_port;
        tokenizer >> token >> std::hex >> state;

        if (local_port == port && state == TCP_LISTEN)
          return;
      }
    }

    usleep(RECHECK_WAIT);
  } while (std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now() - start_time).count()
               <= LISTEN_TIMEOUT);
}

void waitFinish(std::set<pid_t> pids) {
  LOG() << "Waiting for processes to finish." << std::endl;

  do {
    auto start_time = std::chrono::steady_clock::now();

    while ((! pids.empty()) && std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start_time).count()
              <= FINISH_TIMEOUT) {
      pid_t pid = *pids.begin();
      int status;
      assert(waitpid(pid, &status, WNOHANG) != -1);
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        pids.erase(pid);
      } else {
        usleep(RECHECK_WAIT);
      }
    }

    if (! pids.empty()) {
      LOG() << "Killing processes." << std::endl;

      for (pid_t pid : pids) {
        kill(- pid, SIGKILL);
      }
    }
  } while (! pids.empty());
}

void waitFiles(std::set<std::string> files) {
  auto start_time = std::chrono::steady_clock::now();

  while ((! files.empty()) && std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - start_time).count()
        <= LOGS_TIMEOUT) {
    std::string file = *files.begin();
    if (std::ifstream(file).is_open()) {
      files.erase(file);
    } else {
      usleep(RECHECK_WAIT);
    }
  }
}

bool processTestCase( char *clientCmd, char *serverCmd, uint16_t port ) {
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
  waitListen(port);

  std::string clientLog = tmpnam( NULL );
  LOG() << "Logging client results to: " << clientLog << std::endl;

  pid_t clientPID = fork();
  assert( clientPID >= 0 && "Error forking client process." );
  if ( clientPID == 0 ) {
    setpgid( 0, 0 );
    setenv( LOG_FILE_VARIABLE, clientLog.c_str(), 1 );
    LOG() << "Launching client: " << clientCmd << std::endl;
    exit( system( clientCmd ) );
  }
  waitFinish({clientPID, serverPID});
  waitFiles({clientLog, serverLog});

	LOG() << "Processing outputs." << std::endl;
	std::ifstream logFile( clientLog.c_str() );
  assert( logFile.is_open() && "Unable to open client log file." );
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
  bool serverReceived = false;
  while ( logFile.good() ) {
		std::string line;
		std::getline( logFile, line );
		if ( line.compare( 0, strlen( SPA_TAG_PREFIX SPA_VALIDPATH_TAG ), SPA_TAG_PREFIX SPA_VALIDPATH_TAG ) == 0 ) {
			size_t boundary = line.find( ' ' );
			assert( boundary != std::string::npos && boundary < line.size() - 1 && "Malformed tag." );
			serverValid = (line.substr( boundary + 1 ) == SPA_VALIDPATH_VALUE);
    } else if (line.compare(0, strlen(SPA_TAG_PREFIX SPA_MSGRECEIVED_TAG), SPA_TAG_PREFIX SPA_MSGRECEIVED_TAG) == 0) {
      size_t boundary = line.find( ' ' );
      assert( boundary != std::string::npos && boundary < line.size() - 1 && "Malformed tag." );
      serverReceived = (line.substr( boundary + 1 ) == SPA_MSGRECEIVED_VALUE);
    }
  }
  logFile.close();
  remove( serverLog.c_str() );
  LOG() << "Server received: " << serverReceived << std::endl;
  LOG() << "Server validity: " << serverValid << std::endl;
  assert(clientValid == serverReceived && "Message sent but not received.");

	return clientValid && (! serverValid);
}

int main(int argc, char **argv, char **envp) {
	if ( argc < 7 || argc > 9  ) {
		std::cerr << "Usage: " << argv[0] << " [-f] <input-file> <true-positive-file> <false-positive-file> <client-cmd> <server-cmd> <port> [reference-server-cmd]" << std::endl;

		return -1;
	}

	bool follow = false;

	unsigned int arg = 1;
	if ( std::string( "-f" ) == argv[arg] ) {
		follow = true;
		arg++;
	}
  char *inputFileName          = argv[arg++];
  char *truePositivesFileName  = argv[arg++];
  char *falsePositivesFileName = argv[arg++];
  uint16_t port                = atoi(argv[arg++]);
  char *clientCmd              = argv[arg++];
  char *serverCmd              = argv[arg++];
  char *refServerCmd          = NULL;
	if ( arg < (unsigned int) argc )
		refServerCmd = argv[arg++];

  std::set<std::string> testedBundles;

  // Load previously confirmed results to prevent redundant results.
  LOG() << "Loading previous results." << std::endl;
  std::ifstream inputFile( truePositivesFileName );
	std::string bundle;
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
  unsigned long truePositives = testedBundles.size();

  inputFile.open( falsePositivesFileName );
  bundle.clear();
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
  unsigned long falsePositives = testedBundles.size() - truePositives;

  unsigned long numTestCases = 0, numRepeats = 0;

  inputFile.open( inputFileName );
  assert(inputFile.is_open());
  std::ofstream truePositivesFile(truePositivesFileName, std::ios_base::app);
  assert(truePositivesFile.is_open());
  std::ofstream falsePositivesFile(falsePositivesFileName, std::ios_base::app);
  assert(falsePositivesFile.is_open());

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
          numRepeats++;
				} else {
					LOG() << "Processing test case." << std::endl;
					testedBundles.insert( bundle );

          LOG() << "Testing cross-interoperability." << std::endl;
          bool crossInterop = processTestCase(clientCmd, serverCmd, port);
          bool refInterop;
          if (refServerCmd != NULL) {
            LOG() << "Testing reference-interoperability." << std::endl;
            refInterop = processTestCase(clientCmd, refServerCmd, port);
          } else {
            refInterop = false;
          }
					// Only consider interoperability bugs where the client and reference server assert valid but test server doesn't.
					if ( crossInterop && (! refInterop) ) {
						LOG() << "Found true positive. Outputting" << std::endl;
						truePositives++;
						truePositivesFile << bundle << std::endl;
					} else {
						LOG() << "Found false positive. Filtering." << std::endl;
						falsePositives++;
            falsePositivesFile << bundle << std::endl;
          }
					LOG() << "--------------------------------------------------" << std::endl;
				}
				bundle.clear();
				LOG() << "Processed " << numTestCases << " test cases: "
              << truePositives << " true positives, "
              << falsePositives << " false positives, "
              << numRepeats << " repeats."
              << std::endl;
			}
		}
	}
}
