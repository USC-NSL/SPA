#include <klee/klee.h>

unsigned char max_reachedMessageHandler;
unsigned int max_numInterestingStatements;

void __attribute__((noinline)) max_init() {
	klee_make_symbolic( &max_numInterestingStatements, sizeof( max_numInterestingStatements ), "max_numInterestingStatements" );
	klee_make_symbolic( &max_reachedMessageHandler, sizeof( max_reachedMessageHandler ), "max_reachedMessageHandler" );
	max_numInterestingStatements = 0;
	max_reachedMessageHandler = 0;
}

void __attribute__((noinline)) max_messagehandler() {
	max_reachedMessageHandler = 1;
}

void __attribute__((noinline)) max_interesting() {
	max_numInterestingStatements++;
	klee_print_expr( "MAX_INTERESTING_STATEMENT", max_numInterestingStatements );
}
