#ifndef __MAX_H__
#define __MAX_H__

#ifdef ENABLE_MAX

uint32_t max_num_interesting;

void max_init() {
	klee_make_symbolic( &max_num_interesting, sizeof( max_num_interesting ), "max_num_interesting" );
	max_num_interesting = 0;
}

void __attribute__((noinline)) max_message_handler_entry() {}
#define max_make_symbolic( var, size, name ) klee_make_symbolic( var, size, name );
void __attribute__((noinline)) max_interesting() {
	max_num_interesting++;
}

#else

#define max_message_handler_entry()
#define max_interesting()
#define max_make_symbolic( var, size, name )

#endif // #ifdef ENABLE_MAX #else

#endif // #ifndef __MAX_H__
