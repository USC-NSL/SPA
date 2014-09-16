#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <stdint.h>
#include <map>
#include <vector>
#include <algorithm>
#include <chrono>

#define LOG() \
std::cerr << "[" << (std::chrono::duration_cast<std::chrono::milliseconds>( \
                      std::chrono::steady_clock::now() \
                        - programStartTime).count() / 1000.0) << "] "

auto programStartTime = std::chrono::steady_clock::now();

typedef bool (*TestClassifier)( std::map<std::string, std::vector<uint8_t> > );

bool spdylayDiffVersion( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_clientVersion" ) );
// 	assert( testCase.count( "spa_in_api_serverVersion" ) );
	if ( testCase.count( "spa_in_api_clientVersion" ) && testCase.count( "spa_in_api_serverVersion" ) )
		return testCase["spa_in_api_clientVersion"] != testCase["spa_in_api_serverVersion"];
	return false;
}

bool spdylayBadName( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_name" ) );
	const char *names[] = { "spa_in_api_name", "spa_in_api_name1", "spa_in_api_name2", "spa_in_api_name3", "spa_in_api_name4", "spa_in_api_name5", NULL };
	for ( int i = 0; names[i]; i++ ) {
		if ( testCase.count( names[i] ) > 0 ) {
			for ( std::vector<uint8_t>::iterator it = testCase[names[i]].begin(), ie = testCase[names[i]].end(); it != ie; it++ ) {
				if ( *it == '\0' )
					break;
				if ( *it < 0x20 )
					return true;
			}
		}
	}
	return false;
}

bool spdylayBadValue( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_value" ) );
	const char *values[] = { "spa_in_api_value", "spa_in_api_value1", "spa_in_api_value2", "spa_in_api_value3", "spa_in_api_value4", "spa_in_api_value5", NULL };
	for ( int i = 0; values[i]; i++ )
		if ( testCase.count( values[i] ) > 0 && testCase[values[i]][0] == '\0' )
			return true;
	return false;
}

bool spdylayNoDataLength( std::map<std::string, std::vector<uint8_t> > testCase ) {
	return testCase.count( "spa_in_api_dataLength" ) == 0;
}

bool nginxSpdy3(std::map<std::string, std::vector<uint8_t> > testCase) {
  if (testCase.count("spa_in_api_clientVersion"))
    return testCase["spa_in_api_clientVersion"] != std::vector<uint8_t>({2, 0});
  return false;
}

bool nginxHttp09(std::map<std::string, std::vector<uint8_t> > testCase) {
  if (testCase.count("spa_in_api_versionId"))
    return testCase["spa_in_api_versionId"] == std::vector<uint8_t>({2});
  return false;
}

bool nginxBadUrlPercent(std::map<std::string, std::vector<uint8_t> > testCase) {
  if (testCase.count("spa_in_api_path")) {
    for (unsigned i = 0; i < testCase["spa_in_api_path"].size(); i++) {
      if (testCase["spa_in_api_path"][i] == '%') {
        if (i > testCase["spa_in_api_path"].size() - 3)
          return true;
        if (! isxdigit(testCase["spa_in_api_path"][i+1]))
          return true;
        if (! isxdigit(testCase["spa_in_api_path"][i+2]))
          return true;
      }
    }
  }
  return false;
}

bool nginxPercent00(std::map<std::string, std::vector<uint8_t> > testCase) {
  if (testCase.count("spa_in_api_path")) {
    for (unsigned i = 0; i < testCase["spa_in_api_path"].size() - 2; i++) {
      if (testCase["spa_in_api_path"][i] == '%'
          && testCase["spa_in_api_path"][i + 1] == '0'
          && testCase["spa_in_api_path"][i + 2] == '0') {
        return true;
      }
    }
  }
  return false;
}

bool nginxValueCrLf(std::map<std::string, std::vector<uint8_t> > testCase) {
  const char *values[] = {"spa_in_api_value", "spa_in_api_value1", "spa_in_api_value2", "spa_in_api_value3", "spa_in_api_value4", "spa_in_api_value5", "spa_in_api_path", NULL};
  for (int i = 0; values[i]; i++) {
    if (testCase.count(values[i]) > 0
        && (std::find(testCase[values[i]].begin(), testCase[values[i]].end(), '\r') !=testCase[values[i]].end()
            || std::find(testCase[values[i]].begin(), testCase[values[i]].end(), '\n') != testCase[values[i]].end())) {
      return true;
    }
  }
  return false;
}

bool nginxDotDotPastRoot(std::map<std::string, std::vector<uint8_t> > testCase) {
  if (testCase.count("spa_in_api_path")) {
    int depth = 0;

    enum { SLASH, DIR, DOT, DOTDOT } state = SLASH;
    for (auto c : testCase["spa_in_api_path"]) {
      switch (state) {
        case SLASH:
          if (c == '/') {
            state = SLASH;
          } else if (c == '.') {
            state = DOT;
          } else {
            state = DIR;
            depth++;
          }
        break;
        case DIR:
          if (c == '/') {
            state = SLASH;
          } else {
            state = DIR;
          }
        break;
        case DOT:
          if (c == '/') {
            state = SLASH;
          } else if (c == '.') {
            state = DOTDOT;
          } else {
            state = DIR;
          }
        break;
        case DOTDOT:
          if (c == '/') {
            state = SLASH;
            depth--;
          } else {
            state = DIR;
          }
        break;
        default:
          assert(0 && "Bad state.");
      }
      if (depth < 0)
        return true;
    }
  }
  return false;
}

bool nginxTrace(std::map<std::string, std::vector<uint8_t> > testCase) {
  if (testCase.count("spa_in_api_methodId"))
    return testCase["spa_in_api_methodId"] == std::vector<uint8_t>({6});
  return false;
}

bool sipFromBadChar( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_from" ) );
	if ( testCase.count( "spa_in_api_from" ) ) {
		for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_from"].begin(), ie = testCase["spa_in_api_from"].end(); it != ie; it++ ) {
			if ( *it == '\0' )
				break;
			if ( ! isprint( *it ) )
				return true;
		}
	}
	return false;
}

bool sipFromNoScheme( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_from" ) );
	if ( testCase.count( "spa_in_api_from" )) {
		std::string from;
		for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_from"].begin(), ie = testCase["spa_in_api_from"].end(); it != ie; it++ ) {
			if ( *it == '\0' )
				break;
			from += (char) *it;
		}
		return from.find( "sip:" ) == std::string::npos;
	}
	return false;
}

// bool sipFromBadQuote( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_from" ) );
// 	unsigned long count = 0;
// 	for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_from"].begin(), ie = testCase["spa_in_api_from"].end(); it != ie; it++ ) {
// 		if ( *it == '\0' )
// 			break;
// 		if ( *it == '\"' )
// 			count++;
// 	}
// 	return count != 0 && count != 2;
// }

bool sipToConfusedScheme( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_to" ) );
	if ( testCase.count( "spa_in_api_to" ) ) {
		std::string to;
		for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_to"].begin(), ie = testCase["spa_in_api_to"].end(); it != ie; it++ ) {
			if ( *it == '\0' )
				break;
			to += (char) tolower( *it );
		}
		if ( to.compare( 0, 3, "sip" ) == 0 && to[4] == ':' )
			return true;
		if ( to.compare( 0, 4, "<sip" ) == 0 && to[5] == ':' )
			return true;
	}
	return false;
}

bool sipEventBadChar( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_event" ) );
	if ( testCase.count( "spa_in_api_event" ) ) {
		for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_event"].begin(), ie = testCase["spa_in_api_event"].end(); it != ie; it++ ) {
			if ( *it == '\0' )
				break;
			if ( ! isprint( *it ) )
				return true;
		}
	}
	return false;
}

static struct {
	TestClassifier classifier;
	const char *outFileName;
} classifiers[] = {
  { spdylayDiffVersion, "BadInputs.spdylayDiffVersion" },
  { nginxSpdy3, "BadInputs.nginxSpdy3" },
  { nginxHttp09, "BadInputs.nginxHttp09" },
  { nginxBadUrlPercent, "BadInputs.nginxBadUrlPercent" },
  { nginxPercent00, "BadInputs.nginxPercent00" },
  { nginxValueCrLf, "BadInputs.nginxValueCrLf" },
  { nginxDotDotPastRoot, "BadInputs.nginxDotDotPastRoot" },
  { nginxTrace, "BadInputs.nginxTrace" },
  { spdylayBadName, "BadInputs.spdylayBadName" },
  { spdylayBadValue, "BadInputs.spdylayBadValue" },
//   { spdylayNoDataLength, "BadInputs.spdylayNoDataLength" },
  { sipFromBadChar, "BadInputs.sipFromBadChar" },
  { sipFromNoScheme, "BadInputs.sipFromNoScheme" },
  { sipToConfusedScheme, "BadInputs.sipToConfusedScheme" },
  { sipEventBadChar, "BadInputs.sipEventBadChar" },
	{ NULL, "BadInputs.default" },
};

std::vector<unsigned long> resultCounts;

std::string testToStr( std::map<std::string, std::vector<uint8_t> > testCase ) {
	std::stringstream result;
	for ( std::map<std::string, std::vector<uint8_t> >::iterator vit = testCase.begin(), vie = testCase.end(); vit != vie; vit++ ) {
		result << vit->first;
		for ( std::vector<uint8_t>::iterator bit = vit->second.begin(), bie = vit->second.end(); bit != bie; bit++ )
			result << " " << std::hex << (int) *bit;
		result << std::endl;
	}
	result << std::endl;
	return result.str();
}

void displayStats() {
  LOG() << "Breakdown:" << std::endl;
  unsigned long total = 0;
  for (unsigned i = 0; i < resultCounts.size(); i++) {
    LOG() << "  " << classifiers[i].outFileName << ": " << resultCounts[i] << std::endl;
    total += resultCounts[i];
  }
  LOG() << "  Total: " << total << std::endl;
}

int main(int argc, char **argv, char **envp) {
	if ( argc < 2 || argc > 3 ) {
		std::cerr << "Usage: " << argv[0] << " [-f] <input-file>" << std::endl;

		return -1;
	}

	bool follow = false;
	char *inputFileName;

	if ( std::string( "-f" ) == argv[1] ) {
		follow = true;
		inputFileName = argv[2];
	} else {
		inputFileName = argv[1];
	}

	std::ifstream inputFile( inputFileName );
	unsigned long lineNo = 0;

	int i = 0;
	std::vector<std::ofstream *> outputFiles;
	do {
		if ( classifiers[i].classifier )
			LOG() << "Class "<< i << " outputted to: " << classifiers[i].outFileName << std::endl;
		else
			LOG() << "Default class outputted to: " << classifiers[i].outFileName << std::endl;
		outputFiles.push_back( new std::ofstream( classifiers[i].outFileName ) );
		assert( outputFiles[i]->is_open() );
		resultCounts.push_back( 0 );
	} while ( classifiers[i++].classifier );

	std::map<std::string, std::vector<uint8_t> > testCase;
	while ( inputFile.good() || follow ) {
		if ( follow && ! inputFile.good() ) {
      displayStats();
			LOG() << "Reached end of input. Sleeping." << std::endl;
			sleep( 1 );
			inputFile.clear();
			continue;
		}
		std::string line;
		std::getline( inputFile, line );
		lineNo++;

		if ( ! line.empty() ) {
			size_t d = line.find( ' ' );
			std::string name = line.substr( 0, d );
			assert( ! name.empty() && "Empty variable name." );

			std::vector<uint8_t> value;
			if ( d != std::string::npos ) {
				std::stringstream ss( line.substr( d + 1, std::string::npos ) );
				ss << std::hex;

				while ( ss.good() ) {
					int c;
					ss >> c;
					value.push_back( c );
				}
			}
			testCase[name] = value;
		} else {
      if ( ! testCase.empty() ) {
        for ( i = 0; classifiers[i].classifier; i++ ) {
          if ((! classifiers[i].classifier) || classifiers[i].classifier(testCase)) {
            LOG() << "Test-case just before line " << lineNo
                  << " classified as " << classifiers[i].outFileName << std::endl;
            *outputFiles[i] << testToStr( testCase );
            outputFiles[i]->flush();
            resultCounts[i]++;
//             displayStats();
            break;
          }
        }
        testCase.clear();
      }
		}
	}
  displayStats();
}
