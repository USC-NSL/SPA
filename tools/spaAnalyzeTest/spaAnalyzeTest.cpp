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
	if ( testCase.count( "spa_in_api_clientVersion" ) && testCase.count( "spa_in_api_serverVersion" ) ) {
    return (testCase["spa_in_api_clientVersion"] == std::vector<uint8_t>({2, 0}))
           != (testCase["spa_in_api_serverVersion"] == std::vector<uint8_t>({2, 0}));
  }
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

bool spdylayEmptyValue( std::map<std::string, std::vector<uint8_t> > testCase ) {
// 	assert( testCase.count( "spa_in_api_value" ) );
	const char *values[] = { "spa_in_api_value", "spa_in_api_value1", "spa_in_api_value2", "spa_in_api_value3", "spa_in_api_value4", "spa_in_api_value5", NULL };
	for ( int i = 0; values[i]; i++ )
		if ( testCase.count( values[i] ) > 0 && testCase[values[i]][0] == '\0' )
			return true;
	return false;
}

// From spdylay_frame.c:
static int VALID_HD_VALUE_CHARS[] = {
  1 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */,
  0 /* EOT  */, 0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */,
  0 /* BS   */, 1 /* HT   */, 0 /* LF   */, 0 /* VT   */,
  0 /* FF   */, 0 /* CR   */, 0 /* SO   */, 0 /* SI   */,
  0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
  0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */,
  0 /* CAN  */, 0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */,
  0 /* FS   */, 0 /* GS   */, 0 /* RS   */, 0 /* US   */,
  1 /* SPC  */, 1 /* !    */, 1 /* "    */, 1 /* #    */,
  1 /* $    */, 1 /* %    */, 1 /* &    */, 1 /* '    */,
  1 /* (    */, 1 /* )    */, 1 /* *    */, 1 /* +    */,
  1 /* ,    */, 1 /* -    */, 1 /* .    */, 1 /* /    */,
  1 /* 0    */, 1 /* 1    */, 1 /* 2    */, 1 /* 3    */,
  1 /* 4    */, 1 /* 5    */, 1 /* 6    */, 1 /* 7    */,
  1 /* 8    */, 1 /* 9    */, 1 /* :    */, 1 /* ;    */,
  1 /* <    */, 1 /* =    */, 1 /* >    */, 1 /* ?    */,
  1 /* @    */, 1 /* A    */, 1 /* B    */, 1 /* C    */,
  1 /* D    */, 1 /* E    */, 1 /* F    */, 1 /* G    */,
  1 /* H    */, 1 /* I    */, 1 /* J    */, 1 /* K    */,
  1 /* L    */, 1 /* M    */, 1 /* N    */, 1 /* O    */,
  1 /* P    */, 1 /* Q    */, 1 /* R    */, 1 /* S    */,
  1 /* T    */, 1 /* U    */, 1 /* V    */, 1 /* W    */,
  1 /* X    */, 1 /* Y    */, 1 /* Z    */, 1 /* [    */,
  1 /* \    */, 1 /* ]    */, 1 /* ^    */, 1 /* _    */,
  1 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
  1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */,
  1 /* h    */, 1 /* i    */, 1 /* j    */, 1 /* k    */,
  1 /* l    */, 1 /* m    */, 1 /* n    */, 1 /* o    */,
  1 /* p    */, 1 /* q    */, 1 /* r    */, 1 /* s    */,
  1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
  1 /* x    */, 1 /* y    */, 1 /* z    */, 1 /* {    */,
  1 /* |    */, 1 /* }    */, 1 /* ~    */, 0 /* DEL  */,
  1 /* 0x80 */, 1 /* 0x81 */, 1 /* 0x82 */, 1 /* 0x83 */,
  1 /* 0x84 */, 1 /* 0x85 */, 1 /* 0x86 */, 1 /* 0x87 */,
  1 /* 0x88 */, 1 /* 0x89 */, 1 /* 0x8a */, 1 /* 0x8b */,
  1 /* 0x8c */, 1 /* 0x8d */, 1 /* 0x8e */, 1 /* 0x8f */,
  1 /* 0x90 */, 1 /* 0x91 */, 1 /* 0x92 */, 1 /* 0x93 */,
  1 /* 0x94 */, 1 /* 0x95 */, 1 /* 0x96 */, 1 /* 0x97 */,
  1 /* 0x98 */, 1 /* 0x99 */, 1 /* 0x9a */, 1 /* 0x9b */,
  1 /* 0x9c */, 1 /* 0x9d */, 1 /* 0x9e */, 1 /* 0x9f */,
  1 /* 0xa0 */, 1 /* 0xa1 */, 1 /* 0xa2 */, 1 /* 0xa3 */,
  1 /* 0xa4 */, 1 /* 0xa5 */, 1 /* 0xa6 */, 1 /* 0xa7 */,
  1 /* 0xa8 */, 1 /* 0xa9 */, 1 /* 0xaa */, 1 /* 0xab */,
  1 /* 0xac */, 1 /* 0xad */, 1 /* 0xae */, 1 /* 0xaf */,
  1 /* 0xb0 */, 1 /* 0xb1 */, 1 /* 0xb2 */, 1 /* 0xb3 */,
  1 /* 0xb4 */, 1 /* 0xb5 */, 1 /* 0xb6 */, 1 /* 0xb7 */,
  1 /* 0xb8 */, 1 /* 0xb9 */, 1 /* 0xba */, 1 /* 0xbb */,
  1 /* 0xbc */, 1 /* 0xbd */, 1 /* 0xbe */, 1 /* 0xbf */,
  1 /* 0xc0 */, 1 /* 0xc1 */, 1 /* 0xc2 */, 1 /* 0xc3 */,
  1 /* 0xc4 */, 1 /* 0xc5 */, 1 /* 0xc6 */, 1 /* 0xc7 */,
  1 /* 0xc8 */, 1 /* 0xc9 */, 1 /* 0xca */, 1 /* 0xcb */,
  1 /* 0xcc */, 1 /* 0xcd */, 1 /* 0xce */, 1 /* 0xcf */,
  1 /* 0xd0 */, 1 /* 0xd1 */, 1 /* 0xd2 */, 1 /* 0xd3 */,
  1 /* 0xd4 */, 1 /* 0xd5 */, 1 /* 0xd6 */, 1 /* 0xd7 */,
  1 /* 0xd8 */, 1 /* 0xd9 */, 1 /* 0xda */, 1 /* 0xdb */,
  1 /* 0xdc */, 1 /* 0xdd */, 1 /* 0xde */, 1 /* 0xdf */,
  1 /* 0xe0 */, 1 /* 0xe1 */, 1 /* 0xe2 */, 1 /* 0xe3 */,
  1 /* 0xe4 */, 1 /* 0xe5 */, 1 /* 0xe6 */, 1 /* 0xe7 */,
  1 /* 0xe8 */, 1 /* 0xe9 */, 1 /* 0xea */, 1 /* 0xeb */,
  1 /* 0xec */, 1 /* 0xed */, 1 /* 0xee */, 1 /* 0xef */,
  1 /* 0xf0 */, 1 /* 0xf1 */, 1 /* 0xf2 */, 1 /* 0xf3 */,
  1 /* 0xf4 */, 1 /* 0xf5 */, 1 /* 0xf6 */, 1 /* 0xf7 */,
  1 /* 0xf8 */, 1 /* 0xf9 */, 1 /* 0xfa */, 1 /* 0xfb */,
  1 /* 0xfc */, 1 /* 0xfd */, 1 /* 0xfe */, 1 /* 0xff */
};
bool spdylayBadValueChar( std::map<std::string, std::vector<uint8_t> > testCase ) {
  //  assert( testCase.count( "spa_in_api_value" ) );
  const char *values[] = { "spa_in_api_path", "spa_in_api_value", "spa_in_api_value1", "spa_in_api_value2", "spa_in_api_value3", "spa_in_api_value4", "spa_in_api_value5", NULL };
  for (int i = 0; values[i]; i++) {
    if (testCase.count(values[i])) {
      for (uint8_t c : testCase[values[i]]) {
        if (! VALID_HD_VALUE_CHARS[c])
          return true;
      }
    }
  }
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
  { spdylayEmptyValue, "BadInputs.spdylayEmptyValue" },
  { spdylayBadValueChar, "BadInputs.spdylayBadValueChar" },
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
        for ( i = 0; ; i++ ) {
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
