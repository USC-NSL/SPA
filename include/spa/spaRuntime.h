#ifndef __SPARUNTIME_H__
#define __SPARUNTIME_H__

#include <stdarg.h>
#include <klee/klee.h>

typedef void (*SpaRuntimeHandler_t)( va_list );

#define spa_tag( var, value ) __spa_tag( &var, "spa_tag_" #var, value )
void __spa_tag( char **var, char *varName, char *value ) {
	klee_make_symbolic( var, sizeof( char * ), varName );
	*var = value;
}

#define spa_var( var ) spa_input_fixed( &var, sizeof( var ), #var )
#define spa_input_fixed( var, size, name ) klee_make_symbolic( var, size, "spa_input_fixed_" name )
#define spa_input_var( var, size, name ) klee_make_symbolic( var, size, "spa_input_var_" name )

#ifdef __cplusplus
extern "C" {
	void spa_runtime_call( SpaRuntimeHandler_t handler, ... );
}
#else // #ifdef __cplusplus
void spa_runtime_call( SpaRuntimeHandler_t handler, ... );
#endif// #ifdef __cplusplus #else


#endif // #ifndef __SPARUNTIME_H__
