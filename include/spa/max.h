#ifndef __MAX_H__
#define __MAX_H__

#ifdef ENABLE_MAX

void __attribute__((noinline)) max_message_handler_entry() {}
void __attribute__((noinline)) max_interesting() {}
void __attribute__((noinline)) max_register_symbol( const char *name, ... ) {}

#define max_make_symbolic( var, name ) \
	klee_make_symbolic( &var, sizeof( var ), name ); \
	max_register_symbol( name, var );

#else

#define max_message_handler_entry()
#define max_interesting()
#define max_make_symbolic( name, var )

#endif // #ifdef ENABLE_MAX #else

#endif // #ifndef __MAX_H__
