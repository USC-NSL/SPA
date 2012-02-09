#ifndef __MAX_H__
#define __MAX_H__

#ifdef ENABLE_MAX

uint32_t max_num_interesting = 0;

void max_init() {
	klee_make_symbolic( &max_num_interesting, sizeof( max_num_interesting ), "max_num_interesting" );
	max_num_interesting = 0;
}
void __attribute__((noinline)) max_message_handler_entry() {}
void __attribute__((noinline)) max_interesting() {
	max_num_interesting++;
}

void max_make_symbolic( void *var, size_t size, char *name, int fixed );
#define max_state( var, size, name ) max_make_symbolic( var, size, name, 1 );
#define max_input_fixed( var, size, name ) max_make_symbolic( var, size, name, 1 );
#define max_input_var( var, size, name ) max_make_symbolic( var, size, name, 0 );

#else

#define max_message_handler_entry()
#define max_interesting()
#define max_state( var, size, name )
#define max_input_fixed( var, size, name )
#define max_input_var( var, size, name )

#endif // #ifdef ENABLE_MAX #else

#endif // #ifndef __MAX_H__
