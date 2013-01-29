#include <spa/spaRuntime.h>

int cmp( char a, char b ) {
// 	klee_print_expr( "a", a );
// 	klee_print_expr( "b", b );

	if ( a > b ) {
		return 0;
	} else if ( a < b ) {
		return 0;
	} else {
		spa_return();
		return 1;
	}
}

void __attribute__((noinline,used)) spaHandler() {
	spa_api_entry();

	char expected[16] = "0123456789ABCDEF";
	char input[16];
	char result[sizeof(expected)];
	size_t i;
	for ( i = 0; i < sizeof(result) / sizeof(input); i++ ) {
		spa_api_input_var( input );
		memcpy( result + i * sizeof(result) / sizeof(input), input, sizeof(input) );
	}

	size_t c = 0;
	for ( i = 0; i < sizeof( result ); i++ ) {
		c += cmp( result[i], expected[i] );
		klee_print_expr( "COUNT", c );
	}
	assert( c == sizeof(expected) );

	spa_msg_output_var( result );
	spa_valid_path();
}

int main( int argc, char **argv ) {
	spaHandler();
	return 0;
}
