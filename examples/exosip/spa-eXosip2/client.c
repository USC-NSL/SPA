#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <eXosip2/eX_setup.h>
#include <eXosip2/eX_call.h>

#include <spa/spaRuntime.h>

void __attribute__((noinline,used)) spa_SendInvite() {
	spa_api_entry();

	char from[10];
	char to[10];
	spa_api_input_var( from );
	from[sizeof(from)-1] = 0;
	spa_api_input_var( to );
	to[sizeof(to)-1] = 0;

	osip_message_t *invite;

	assert( eXosip_init() == OSIP_SUCCESS );
	assert( eXosip_set_socket( IPPROTO_UDP, 1, 5060 ) == OSIP_SUCCESS );

	assert( eXosip_call_build_initial_invite( &invite, to, from, NULL, NULL ) == OSIP_SUCCESS );
// 	assert( eXosip_call_build_initial_invite( &invite, "sip:to@localhost", "sip:from@localhost", NULL, NULL ) == OSIP_SUCCESS );
	eXosip_call_send_initial_invite( invite );
	assert( eXosip_execute() == OSIP_SUCCESS );
	eXosip_quit();
}

int main( int argc, char **argv ) {
	spa_SendInvite();

	return 0;
}
