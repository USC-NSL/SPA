#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <stdint.h>
#include <map>
#include <vector>

typedef bool (*TestClassifier)( std::map<std::string, std::vector<uint8_t> > );

bool spdylayDiffVersion( std::map<std::string, std::vector<uint8_t> > testCase ) {
	assert( testCase.count( "spa_in_api_clientVersion" ) );
	assert( testCase.count( "spa_in_api_serverVersion" ) );
	return testCase["spa_in_api_clientVersion"] != testCase["spa_in_api_serverVersion"];
}

bool spdylayBadName( std::map<std::string, std::vector<uint8_t> > testCase ) {
	assert( testCase.count( "spa_in_api_name" ) );
	for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_name"].begin(), ie = testCase["spa_in_api_name"].end(); it != ie; it++ ) {
		if ( *it == '\0' )
			break;
		if ( *it < 0x20 )
			return true;
	}
	return false;
}

bool spdylayBadValue( std::map<std::string, std::vector<uint8_t> > testCase ) {
	assert( testCase.count( "spa_in_api_value" ) );
	return testCase["spa_in_api_value"][0] == '\0';
}

bool spdylayNoDataLength( std::map<std::string, std::vector<uint8_t> > testCase ) {
	return testCase.count( "spa_in_api_dataLength" ) == 0;
}

bool sipFromBadChar( std::map<std::string, std::vector<uint8_t> > testCase ) {
	assert( testCase.count( "spa_in_api_from" ) );
	for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_from"].begin(), ie = testCase["spa_in_api_from"].end(); it != ie; it++ ) {
		if ( *it == '\0' )
			break;
		if ( ! isprint( *it ) )
			return true;
	}
	return false;
}

bool sipFromNoScheme( std::map<std::string, std::vector<uint8_t> > testCase ) {
	assert( testCase.count( "spa_in_api_from" ) );
	std::string from;
	for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_from"].begin(), ie = testCase["spa_in_api_from"].end(); it != ie; it++ ) {
		if ( *it == '\0' )
			break;
		from += (char) *it;
	}
	return from.find( "sip:" ) == std::string::npos;
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
	assert( testCase.count( "spa_in_api_to" ) );
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
	return false;
}

bool sipEventBadChar( std::map<std::string, std::vector<uint8_t> > testCase ) {
	assert( testCase.count( "spa_in_api_event" ) );
	for ( std::vector<uint8_t>::iterator it = testCase["spa_in_api_event"].begin(), ie = testCase["spa_in_api_event"].end(); it != ie; it++ ) {
		if ( *it == '\0' )
			break;
		if ( ! isprint( *it ) )
			return true;
	}
	return false;
}

TestClassifier classifiers[] = {
// 	spdylayDiffVersion,
	spdylayBadName,
	spdylayBadValue,
// 	spdylayNoDataLength,
// 	sipFromBadChar,
// 	sipFromNoScheme,
// 	sipToConfusedScheme,
// 	sipEventBadChar,
	NULL
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
	std::cerr << "Breakdown:";
	for( std::vector<unsigned long>::iterator it = resultCounts.begin(), ie = resultCounts.end(); it != ie; it++ )
		std::cerr << " " << *it;
	std::cerr << std::endl;
}

int main(int argc, char **argv, char **envp) {
	if ( argc < 2 ) {
		std::cerr << "Usage: " << argv[0] << " [-f] <input-file> <output-files>..." << std::endl;

		return -1;
	}

	bool follow = false;

	int arg = 1;
	if ( std::string( "-f" ) == argv[arg] ) {
		follow = true;
		arg++;
	}

	char *inputFileName = argv[arg++];
	std::ifstream inputFile( inputFileName );
	unsigned long lineNo = 0;

	std::vector<std::ofstream *> outputFiles;
	int i = 0;
	do {
		assert( arg < argc && "Insufficient output files specified." );
		if ( classifiers[i] )
			std::cerr << "Class "<< i << " outputted to: " << argv[arg] << std::endl;
		else
			std::cerr << "Default class outputted to: " << argv[arg] << std::endl;
		outputFiles.push_back( new std::ofstream( argv[arg++] ) );
		assert( outputFiles[i]->is_open() );
		resultCounts.push_back( 0 );
	} while ( classifiers[i++] );
	assert( arg == argc && "Too many output files specified." );

	std::map<std::string, std::vector<uint8_t> > testCase;
	while ( inputFile.good() || follow ) {
		if ( follow && ! inputFile.good() ) {
			std::cerr << "Reached end of input. Sleeping." << std::endl;
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
				for ( i = 0; classifiers[i]; i++ ) {
					if ( classifiers[i]( testCase ) ) {
						std::cerr << "Test-case just before line " << lineNo << " classified as " << i << std::endl;
						*outputFiles[i] << testToStr( testCase );
						outputFiles[i]->flush();
						resultCounts[i]++;
						displayStats();
						break;
					}
				}
				if ( ! classifiers[i] ) {
					std::cerr << "Test-case just before line " << lineNo << " classified as default." << std::endl;
					*outputFiles[i] << testToStr( testCase );
					outputFiles[i]->flush();
					resultCounts[i]++;
					displayStats();
				}
				testCase.clear();
			}
		}
	}
}
