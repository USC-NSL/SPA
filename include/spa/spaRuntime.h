#ifndef __SPARUNTIME_H__
#define __SPARUNTIME_H__

#include <stdarg.h>
#include <klee/klee.h>

typedef const char *SpaTag_t;
typedef void (*SpaRuntimeHandler_t)( va_list );

#ifdef __cplusplus
extern "C" {
	void __attribute__((noinline)) spa_api_entry() {}
	void __attribute__((noinline)) spa_message_handler_entry() {}
	void __attribute__((noinline)) spa_checkpoint() {}
	void __attribute__((noinline)) spa_runtime_call( SpaRuntimeHandler_t handler, ... );
}
#else // #ifdef __cplusplus
void __attribute__((noinline)) spa_api_entry() {}
void __attribute__((noinline)) spa_message_handler_entry() {}
void __attribute__((noinline)) spa_checkpoint() {}
void __attribute__((noinline)) spa_runtime_call( SpaRuntimeHandler_t handler, ... );
#endif// #ifdef __cplusplus #else


#ifdef ENABLE_SPA

SpaTag_t ValidPath;
#define spa_invalid_path() spa_tag( ValidPath, "0" );
#define spa_valid_path() spa_tag( ValidPath, "1" );

#define spa_tag( var, value ) __spa_tag( &var, "spa_tag_" #var, value )
void __spa_tag( SpaTag_t *var, const char *varName, SpaTag_t value ) {
	klee_make_symbolic( var, sizeof( char * ), varName );
	*var = value;
}

#define spa_api_input( var, size, name ) klee_make_symbolic( var, size, "spa_in_api_" name )
#define spa_api_input_var( var ) spa_api_input( &var, sizeof( var ), #var )
#define spa_state( var, size, name ) klee_make_symbolic( var, size, "spa_state_" name )
#define spa_state_var( var ) spa_state( &var, sizeof( var ), #var )
#define spa_api_output( var, size, name ) __spa_output( var, size, "spa_out_api_" name )
#define spa_api_output_var( var ) spa_api_output( var, sizeof( var ), #var )
#define spa_msg_input( var, size, name ) klee_make_symbolic( var, size, "spa_in_msg_" name )
#define spa_msg_input_var( var ) spa_msg_input( &var, sizeof( var ), #var )
#define spa_msg_output( var, size, name ) __spa_output( var, size, "spa_out_msg_" name )
#define spa_msg_output_var( var ) spa_msg_output( &var, sizeof( var ), #var )
void __spa_output( void *var, size_t size, const char *name ) {
	static SpaTag_t Output;
	void *buffer = malloc( size );
	klee_make_symbolic( buffer, size, name );
	memcpy( buffer, var, size );
	spa_tag( Output, "1" );
	spa_checkpoint();
}

#else // #ifdef ENABLE_SPA

#define spa_invalid_path()
#define spa_valid_path()

#define spa_tag( var, value )

#define spa_api_input( var, size, name )
#define spa_api_input_var( var )
#define spa_state( var, size, name )
#define spa_state_var( var )
#define spa_api_output( var, size, name )
#define spa_api_output_var( var )
#define spa_msg_input( var, size, name )
#define spa_msg_input_var( var )
#define spa_msg_output( var, size, name )
#define spa_msg_output_var( var )

#endif // #ifdef ENABLE_SPA #else

#endif // #ifndef __SPARUNTIME_H__
