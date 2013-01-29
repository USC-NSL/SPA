#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <eXosip2/eX_setup.h>
#include <eXosip2/eX_call.h>
#include "../libeXosip2-3.6.0/src/eXosip2.h"

#include <spa/spaRuntime.h>

void __attribute__((noinline,used)) spa_Handler() {
	spa_api_entry();

	assert( eXosip_init() == OSIP_SUCCESS );
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5061, AF_INET, 0) == OSIP_SUCCESS );

#ifdef ENABLE_KLEE
	char message[SIP_MESSAGE_MAX_LENGTH + 1];
	size_t len = 0;
	_eXosip_handle_incoming_message( message, len, 0, "127.0.0.1", 5060 );
#endif

	printf( "Ready.\n" );
	do {
		assert( eXosip_execute() == OSIP_SUCCESS );
		eXosip_event_t *e = eXosip_event_wait( 0, 50 );
		eXosip_automatic_action();
		if ( e == NULL )
			continue;
// 		printf( "Info: %s\n", e->textinfo );
		switch ( e->type ) {
			case EXOSIP_MESSAGE_NEW:
				printf( "New request. Method: %s\n", e->request->sip_method );
				if ( strcmp( "OPTIONS", e->request->sip_method ) == 0 ) {
					int status = 200;
					char header[3];
// 					char header[] = "foo";
// 					char value[5];
					char value[] = "bar";
// 					char value[] = "a\r\nb";

// 					spa_api_input_var( status );

					spa_api_input_var( header );
					spa_assume( header[sizeof( header ) - 1] == '\0' );

// 					spa_api_input_var( value );
// 					spa_assume( value[sizeof( value ) - 1] == '\0' );

					osip_message_t *answer;
					assert( eXosip_options_build_answer( e->tid, status, &answer ) == OSIP_SUCCESS );
// 					assert( osip_message_set_supported( answer, value ) == OSIP_SUCCESS );
					assert( osip_message_set_header( answer, header, value ) == OSIP_SUCCESS );

// 					assert( eXosip_options_send_answer( e->tid, status, NULL ) == OSIP_SUCCESS );
					assert( eXosip_options_send_answer( e->tid, status, answer ) == OSIP_SUCCESS );
					assert( eXosip_execute() == OSIP_SUCCESS );
				}
				break;
			default:
				printf( "Other event. Type: %d\n", e->type );
				printf( "tid: %d, did: %d, rid: %d. cid: %d, sid: %d, nid: %d\n", e->tid, e->did, e->rid, e->cid, e->sid, e->nid );
				printf( "request: %d, response: %d, ack: %d\n", !!e->request, !!e->response, !!e->ack );
				break;
		}
		eXosip_event_free( e );
#ifdef ENABLE_KLEE
	} while ( 0 );
#else
	} while ( 1 );
#endif
	printf( "Done.\n" );
	eXosip_quit();
}

int main( int argc, char **argv ) {
	spa_Handler();

	return 0;
}
