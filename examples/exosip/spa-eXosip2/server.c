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
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5060, AF_INET, 0) == OSIP_SUCCESS );

// 	char message[SIP_MESSAGE_MAX_LENGTH + 1];
// 	size_t len;
// 	_eXosip_handle_incoming_message( message, len, 0, "127.0.0.1", 5060 );

	printf( "Ready.\n" );
	do {
		assert( eXosip_execute() == OSIP_SUCCESS );
		eXosip_event_t *e = eXosip_event_wait( 0, 50 );
		eXosip_automatic_action();
		if ( e == NULL )
			continue;
		switch ( e->type ) {
			case EXOSIP_CALL_INVITE:
				printf( "Incoming call.\n" );
				spa_valid_path();
				break;
			case EXOSIP_MESSAGE_NEW:
				printf( "New request. Method: %s\n", e->request->sip_method );
// 				if ( strcmp( "OPTIONS", e->request->sip_method ) == 0 ) {
// 					assert( eXosip_options_send_answer ( e->tid, 200, NULL ) == OSIP_SUCCESS );
// 				}
				spa_valid_path();
				break;
			case EXOSIP_IN_SUBSCRIPTION_NEW:
				printf( "Incoming subscription.\n" );
				spa_valid_path();
				break;
			default:
				printf( "Other event. Type: %d\n", e->type );
				printf( "tid: %d, did: %d, rid: %d. cid: %d, sid: %d, nid: %d\n", e->tid, e->did, e->rid, e->cid, e->sid, e->nid );
				printf( "request: %d, response: %d, ack: %d\n", !!e->request, !!e->response, !!e->ack );
				break;
		}
		printf( "Info: %s\n", e->textinfo );
		eXosip_event_free( e );
	} while ( 1 );
	printf( "Done.\n" );
	eXosip_quit();
}

int main( int argc, char **argv ) {
	spa_HandleInvite();

	return 0;
}
