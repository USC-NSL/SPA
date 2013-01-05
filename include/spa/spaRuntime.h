#ifndef __SPARUNTIME_H__
#define __SPARUNTIME_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <klee/klee.h>

#define SPA_MAX_WAYPOINTS 8

typedef const char *SpaTag_t;
typedef void (*SpaRuntimeHandler_t)( va_list );


#ifdef __cplusplus
extern "C" {
#endif// #ifdef __cplusplus
	void __attribute__((noinline,weak)) spa_checkpoint() { static uint8_t i = 0; i++; } // Complicated NOP to prevent inlining.
	void __attribute__((noinline)) spa_runtime_call( SpaRuntimeHandler_t handler, ... );

	void spa_api_input_handler( va_list args );
	void spa_state_handler( va_list args );
	void spa_api_output_handler( va_list args );
	void spa_msg_input_handler( va_list args );
	void spa_msg_input_size_handler( va_list args );
	void spa_msg_output_handler( va_list args );
	void spa_tag_handler( va_list args );
	void spa_valid_path_handler( va_list args );
	void spa_invalid_path_handler( va_list args );
#ifdef __cplusplus
}
#endif// #ifdef __cplusplus


#ifdef ENABLE_KLEE

#define spa_assume( x ) klee_assume( x )

#else // #ifdef ENABLE_KLEE

#define spa_assume( x ) assert( x )

#endif // #ifdef ENABLE_KLEE #else


#ifdef ENABLE_SPA

#define spa_tag( var, value ) do { __spa_tag( &var, "spa_tag_" #var, value ); spa_runtime_call( spa_tag_handler, "spa_tag_" #var, value ); } while ( 0 )
void __attribute__((weak)) __spa_tag( SpaTag_t *var, const char *varName, SpaTag_t value ) {
	klee_make_symbolic( var, sizeof( char * ), varName );
	*var = value;
}


SpaTag_t ValidPath;
#define spa_default_invalid() spa_tag( ValidPath, "0" )
#define spa_default_valid() spa_tag( ValidPath, "1" )
#define spa_invalid_path() do { spa_tag( ValidPath, "0" ); spa_runtime_call( spa_invalid_path_handler ); spa_waypoint( SPA_MAX_WAYPOINTS - 2 ); spa_checkpoint(); } while ( 0 )
#define spa_valid_path() do { spa_tag( ValidPath, "1" ); spa_runtime_call( spa_valid_path_handler ); spa_waypoint( SPA_MAX_WAYPOINTS - 2 ); spa_checkpoint(); } while ( 0 )

#define spa_api_input( var, size, name ) do { \
	static uint8_t *initialValue[2] = { NULL, NULL }; \
	spa_input( var, size, "spa_in_api_" name, initialValue, "spa_init_in_api_" name ); \
	spa_runtime_call( spa_api_input_handler, var, size, "spa_in_api_" name ); \
} while ( 0 )
#define spa_api_input_var( var ) spa_api_input( &var, sizeof( var ), #var )

#define spa_state( var, size, name ) do { \
	static uint8_t *initialValue[2] = { NULL, NULL }; \
	spa_input( var, size, "spa_state_" name, initialValue, "spa_init_state_" name ); \
	spa_runtime_call( spa_state_handler, var, size, "spa_state_" name ); \
} while ( 0 )
#define spa_state_var( var ) spa_state( &var, sizeof( var ), #var )

#define spa_api_output( var, size, name ) do { __spa_output( (void *) var, size, "spa_out_api_" name ); spa_runtime_call( spa_api_output_handler, var, size, "spa_out_api_" name ); while ( 0 )
#define spa_api_output_var( var ) spa_api_output( var, sizeof( var ), #var )

SpaTag_t MsgReceived;
#define spa_msg_input( var, size, name ) do { \
	static uint8_t *initialValue[2] = { NULL, NULL }; \
	spa_input( var, size, "spa_in_msg_" name, initialValue, "spa_init_in_msg_" name ); \
	spa_tag( MsgReceived, "1" ); \
	spa_runtime_call( spa_msg_input_handler, var, size, "spa_in_msg_" name ); \
} while ( 0 )
#define spa_msg_input_size( var, name ) do { \
	static uint8_t *initialValue[2] = { NULL, NULL }; \
	spa_input( &var, sizeof( var ), "spa_in_msg_size_" name, initialValue, "spa_init_in_msg_size_" name ); \
	spa_runtime_call( spa_msg_input_size_handler, &var, "spa_in_msg_size_" name ); \
} while ( 0 )
#define spa_msg_input_var( var ) spa_msg_input( &var, sizeof( var ), #var )
#define spa_msg_output( var, size, name ) do { __spa_output( (void *) var, size, "spa_out_msg_" name, "spa_out_msg_size_" name ); spa_runtime_call( spa_msg_output_handler, var, size, "spa_out_msg_" name ); } while ( 0 )
#define spa_msg_output_var( var ) spa_msg_output( &var, sizeof( var ), #var )

#define spa_seed_var( id, var, value ) do { \
	typeof( var ) __v = value; \
	spa_seed( id, &var, sizeof( var ), &__v ); \
} while ( 0 )

#define spa_seed_file( id, var, fileName ) do { \
	if ( spa_internal_SeedID == id ) { \
		struct stat st; \
		klee_assert( stat( fileName, &st ) == 0 && "Unable to stat seed file." ); \
		char seedValue[st.st_size]; \
		FILE *file = fopen( fileName, "r" ); \
		klee_assert( file && "Unable to open seed file." ); \
		for ( int i = 0; feof( file ); seedValue[i++] = getc( file ) ); \
		fclose( file ); \
		spa_seed( id, var, st.st_size, seedValue ); \
	} \
} while ( 0 )

#define spa_seed_file_size( id, var, fileName ) do { \
	if ( spa_internal_SeedID == id ) { \
		struct stat st; \
		klee_assert( stat( fileName, &st ) == 0 && "Unable to stat seed file." ); \
		var = st.st_size; \
	} \
} while ( 0 )

#ifdef __cplusplus
extern "C" {
#endif// #ifdef __cplusplus
	void __attribute__((noinline,weak)) spa_input( void *var, size_t size, const char varName[], uint8_t *initialValue[], const char initialValueName[] ) {
		klee_make_symbolic( var, size, varName );

		// Check if initial assumptions are specified.
		if ( initialValue[0] && initialValue[1] ) {
			uint8_t concrete = 1;
			// For each byte, check symbolic mask and specified assumption.
			size_t i;
			for ( i = 0; i < size; i++ ) {
				if ( initialValue[1][i] ) { // Concrete byte.
					spa_assume( ((uint8_t *) var)[i] == initialValue[0][i] );
				} else { // Symbolic byte.
					concrete = 0;
				}
			}
			// Output initial value if it is fully concrete.
			if ( concrete ) {
				void *buffer = malloc( size );
				klee_make_symbolic( buffer, size, initialValueName );
				memcpy( buffer, initialValue[0], size );
			}
		}
	}

	void __attribute__((noinline,weak)) spa_waypoint( unsigned int id ) {
		static uint8_t init = 0;
		static uint8_t waypoints[SPA_MAX_WAYPOINTS / 8 + 1];
		static uint8_t *waypointsPtr;
		if ( ! init ) {
			bzero( waypoints, sizeof( waypoints ) );
			klee_make_symbolic( &waypointsPtr, sizeof( waypointsPtr ), "spa_waypoints" );
			waypointsPtr = waypoints;
			init = 1;
		}
		klee_assert( id < SPA_MAX_WAYPOINTS );
		waypoints[id>>3] |= 1<<(id & 0x7);
	}

	unsigned int spa_internal_SeedID;
	void __attribute__((noinline,weak)) spa_seed( const unsigned int id, void *var, size_t size, const void *value ) {
		if ( spa_internal_SeedID == id )
			memcpy( var, value, size );
	}
#ifdef __cplusplus
}
#endif// #ifdef __cplusplus

void __attribute__((weak)) __spa_output( void *var, size_t size, const char *varName, const char *sizeName ) {
	static SpaTag_t Output;
	void *buffer = malloc( size );
	klee_make_symbolic( buffer, size, varName );
	memcpy( buffer, var, size );
	size_t *bufSize = (size_t *) malloc( sizeof( size_t ) );
	klee_make_symbolic( bufSize, sizeof( size_t ), sizeName );
	*bufSize = size;
	spa_tag( Output, "1" );
	// 	spa_checkpoint();
	spa_waypoint( SPA_MAX_WAYPOINTS - 1 );
}

#else // #ifdef ENABLE_SPA

#define spa_default_invalid()
#define spa_default_valid()
#define spa_invalid_path() printf( "[SPA] Invalid Path.\n" )
#define spa_valid_path() printf( "[SPA] Valid Path.\n" )

#define spa_tag( var, value )

#define spa_api_input( var, size, name )
#define spa_api_input_var( var )
#define spa_state( var, size, name )
#define spa_state_var( var )
#define spa_api_output( var, size, name )
#define spa_api_output_var( var )
#define spa_msg_input( var, size, name )
#define spa_msg_input_size( var, name )
#define spa_msg_input_var( var )
#define spa_msg_output( var, size, name )
#define spa_msg_output_var( var )

#define spa_waypoint( id )

#define spa_assume( x )

#define spa_seed_var( id, var, value )
#define spa_seed_file( id, var, fileName )
#define spa_seed_file_size( id, var, fileName )
#define spa_seed( id, var, size, value )

#endif // #ifdef ENABLE_SPA #else


#ifdef __cplusplus
extern "C" {
#endif// #ifdef __cplusplus
	void __attribute__((noinline,weak)) spa_api_entry() {
		static SpaTag_t HandlerType;
		spa_tag( HandlerType, "API" );
	}
	void __attribute__((noinline,weak)) spa_message_handler_entry() {
		static SpaTag_t HandlerType;
		spa_tag( HandlerType, "Message" );
	}
#ifdef __cplusplus
}
#endif// #ifdef __cplusplus


#endif // #ifndef __SPARUNTIME_H__
