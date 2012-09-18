#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <eXosip2/eX_setup.h>
#include <eXosip2/eX_call.h>
#include "../libeXosip2-3.6.0/src/eXosip2.h"

#include <spa/spaRuntime.h>

void __attribute__((noinline,used)) spa_HandleInvite() {
	spa_message_handler_entry();


	assert( eXosip_init() == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, 1, 5060 ) == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, socket( AF_INET, SOCK_DGRAM, 0 ), 5060 ) == OSIP_SUCCESS );
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5060, AF_INET, 0) == OSIP_SUCCESS );

	char message[SIP_MESSAGE_MAX_LENGTH + 1];
	size_t len;
	spa_msg_input_var( message );
	spa_msg_input_size( len, "message" );

	_eXosip_handle_incoming_message( message, len, 0, "127.0.0.1", 5060 );

// 	assert( eXosip_execute() == OSIP_SUCCESS );
	eXosip_quit();
}

int main( int argc, char **argv ) {
	spa_HandleInvite();

	return 0;
}
