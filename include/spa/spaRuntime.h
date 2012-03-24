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
}
#else // #ifdef __cplusplus
void __attribute__((noinline)) spa_api_entry() {}
void __attribute__((noinline)) spa_message_handler_entry() {}
#endif// #ifdef __cplusplus #else

#define spa_input_var( var ) spa_input( &var, sizeof( var ), #var )
#define spa_output_var( var ) spa_output( &var, sizeof( var ), #var )


#ifdef ENABLE_SPA

#define spa_tag( var, value ) __spa_tag( &var, "spa_tag_" #var, value )
void __spa_tag( SpaTag_t *var, const char *varName, SpaTag_t value ) {
	klee_make_symbolic( var, sizeof( char * ), varName );
	*var = value;
}

#define spa_input( var, size, name ) klee_make_symbolic( var, size, "spa_input_" name )

void spa_output( void *var, size_t size, const char *name ) {
	klee_make_symbolic( var, size, (std::string() + "spa_output_" + name).c_str() );
}

#ifdef __cplusplus
extern "C" {
	void spa_runtime_call( SpaRuntimeHandler_t handler, ... );
}
#else // #ifdef __cplusplus
void spa_runtime_call( SpaRuntimeHandler_t handler, ... );
#endif// #ifdef __cplusplus #else

#else // #ifdef ENABLE_SPA

#define spa_tag( var, value )
#define spa_input( var, size, name )
#define spa_output( var, size, name )

#endif // #ifdef ENABLE_SPA #else

#endif // #ifndef __SPARUNTIME_H__
