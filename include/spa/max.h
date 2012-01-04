#ifndef __MAX_H__
#define __MAX_H__

#ifdef ENABLE_MAX

void max_init();
void max_messagehandler();
void max_interesting();
#define max_make_symbolic( a, b, c ) (klee_make_symbolic( a, b, c ))

#else

#define max_init()
#define max_messagehandler()
#define max_interesting()
#define max_make_symbolic( a, b, c )

#endif // #ifdef ENABLE_MAX #else

#endif // #ifndef __MAX_H__
