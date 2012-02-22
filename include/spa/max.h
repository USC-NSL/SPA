#ifndef __MAX_H__
#define __MAX_H__

#ifdef ENABLE_MAX

const char *max_internal_HandlerName = NULL;
uint32_t max_internal_NumInteresting = 0;

void max_init() {
	klee_make_symbolic( &max_internal_HandlerName, sizeof( max_internal_HandlerName ), "max_internal_HandlerName" );
	max_internal_HandlerName = NULL;
	klee_make_symbolic( &max_internal_NumInteresting, sizeof( max_internal_NumInteresting ), "max_internal_NumInteresting" );
	max_internal_NumInteresting = 0;
}

void __attribute__((noinline)) max_message_handler_entry() {}

void max_solve_symbolic( const char *name  );
void max_message_handler( const char *name ) {
	max_internal_HandlerName = name;
	max_solve_symbolic( name );
}

void __attribute__((noinline)) max_interesting() {
	max_internal_NumInteresting++;
}

void max_make_symbolic( void *var, size_t size, const char *name, int fixed );
#define max_state( var, size, name ) max_make_symbolic( var, size, name, 1 );
#define max_input_fixed( var, size, name ) max_make_symbolic( var, size, name, 1 );
#define max_input_var( var, size, name ) max_make_symbolic( var, size, name, 0 );

#else

#define max_message_handler_entry()
#define max_message_handler( name )
#define max_interesting()
#define max_state( var, size, name )
#define max_input_fixed( var, size, name )
#define max_input_var( var, size, name )

#endif // #ifdef ENABLE_MAX #else

#endif // #ifndef __MAX_H__
