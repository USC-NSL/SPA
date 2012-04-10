#ifndef __MAX_H__
#define __MAX_H__

#define MAX_PATH_FILE "max_paths.txt"

#ifdef ENABLE_MAX
#define ENABLE_SPA

#include <spa/spaRuntime.h>

SpaTag_t max_HandlerName = NULL;

void maxSolveSymbolicHandler( va_list args );
void maxInputFixedHandler( va_list args );
void maxInputVarHandler( va_list args );

void __attribute__((noinline)) max_message_handler_entry() {}
void __attribute__((noinline)) max_interesting() {}

void max_solve_symbolic( const char *name  );
void max_message_handler( const char *name ) {
	spa_tag( max_HandlerName, name );
	spa_runtime_call( maxSolveSymbolicHandler, name );
}

#define max_state( var, size, name ) spa_state( var, size, name )
#define max_input_fixed( var, size, name ) spa_msg_input( var, size, name ); spa_runtime_call( maxInputFixedHandler, var, size, name )
#define max_input_var( var, size, name ) spa_msg_input( var, size, name ); spa_runtime_call( maxInputVarHandler, var, size, name )

#else // #ifdef ENABLE_MAX

#define max_message_handler_entry()
#define max_message_handler( name )
#define max_interesting()
#define max_state( var, size, name )
#define max_input_fixed( var, size, name )
#define max_input_var( var, size, name )

#endif // #ifdef ENABLE_MAX #else

#endif // #ifndef __MAX_H__
