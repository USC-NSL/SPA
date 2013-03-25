#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <stdint.h>
#include <map>
#include <vector>

typedef bool (*TestClassifier)( std::map<std::string, std::vector<uint8_t> > );

TestClassifier classifiers[] = {
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
		assert( arg <= argc && "Insufficient output files specified." );
		outputFiles.push_back( new std::ofstream( argv[arg++] ) );
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
			int i;
			for ( i = 0; classifiers[i]; i++ ) {
				if ( classifiers[i]( testCase ) ) {
					std::cerr << "Test-case just before line " << lineNo << "classified as " << i << std::endl;
					*outputFiles[i] << testToStr( testCase );
					resultCounts[i]++;
					displayStats();
					break;
				}
			}
			if ( ! classifiers[i] ) {
				std::cerr << "Test-case just before line " << lineNo << "classified as default." << std::endl;
				*outputFiles[i] << testToStr( testCase );
				resultCounts[i]++;
				displayStats();
			}
		}
	}
}
