#include <cstdlib>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <spa/spaRuntimeImpl.h>

extern "C" {
	void spa_api_input_handler( va_list args ) {
		uint8_t *var = (uint8_t *) va_arg( args, void * );
		size_t size = va_arg( args, size_t );
		const char *name = va_arg( args, const char * );

		std::cerr << "Injecting value for " << name << "[" << size << "]." << std::endl;

		std::string varName = "SPA_";
		varName += name;

		char *varValue = getenv( varName.c_str() );
		assert( varValue && "Variable not defined in environment." );

		std::stringstream ss( varValue );

		while ( ss.good() && ! ss.bad() ) {
			assert( size-- > 0 && "Variable size mismatch." );
			unsigned int n;
			ss >> std::hex >> n;
			std::cerr << n << " ";
			*(var++) = n;
		}
		std::cerr << std::endl;
		assert( size == 0 && "Variable size mismatch." );
	}

	void spa_state_handler( va_list args ) { }
	void spa_api_output_handler( va_list args ) { }
	void spa_msg_input_handler( va_list args ) { }
	void spa_msg_input_size_handler( va_list args ) { }
	void spa_msg_output_handler( va_list args ) { }

	void spa_valid_path_handler( va_list args ) {
		std::cerr << "Valid path." << std::endl;
		exit( 200 );
	}

	void spa_invalid_path_handler( va_list args ) {
		std::cerr << "Invalid path." << std::endl;
		exit( 201 );
	}
}
