#include <spa/spaRuntime.h>

extern "C" {
	void __attribute__((noinline)) spa_api_entry() { asm (""); }
	void __attribute__((noinline)) spa_message_handler_entry() { asm (""); }
	void __attribute__((noinline)) spa_checkpoint() { asm (""); }
}
