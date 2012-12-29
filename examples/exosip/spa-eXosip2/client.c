#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <eXosip2/eX_setup.h>
#include <eXosip2/eX_call.h>
#include "../libeXosip2-3.6.0/src/eXosip2.h"

extern eXosip_t eXosip;

#include <spa/spaRuntime.h>

#define MIN( a, b ) ((a) < (b) ? (a) : (b))
#define MAX( a, b ) ((a) > (b) ? (a) : (b))

// void spa_DeclareState() {
// 	osip_transaction_t *tmp = eXosip.j_calls->c_out_tr;
// 	spa_state( eXosip.j_calls, sizeof( eXosip_call_t ), "eXosip_j_calls" );
// 	eXosip.j_calls->c_out_tr = tmp;
// 
// 	spa_state( eXosip.j_calls->c_out_tr, sizeof( osip_transaction_t ), "eXosip_j_calls_c_out_tr" );
// }

void __attribute__((noinline,used)) spa_SendInvite() {
	spa_api_entry();

// 	char from[25];
// 	spa_api_input_var( from );
// 	spa_assume( from[sizeof( from ) - 1] == '\0' );

	char to[25];
	spa_api_input_var( to );
	spa_assume( to[sizeof( to ) - 1] == '\0' );

	char from[] = "<sip:from@127.0.0.1:5061>";
// 	char to[] = "<sip:to@127.0.0.1:5060>";

// PJSIP Bad Inputs
// 	char from[30] = "<sip:@"; // Good in eXoSIP server.
// 	char to[30] = "SIP@:";

// eXoSIP Bad Inputs
// 	char to[30] = "SIP@:";

	size_t i, j;
// 	char from_suffix[] = "@127.0.0.1:5061>";
// 	for ( i = sizeof( from ) - 1; from[i] == '\0' && i > sizeof( from_suffix ) - 1; i-- );
//  	printf( "From username length: %lu\n", i - sizeof( from_suffix ) + 2 );
// 	for ( j = 0; j <= i; j++ )
// 		spa_assume( from[j] != '\0' );
// 	for ( j = i - sizeof( from_suffix ) + 2, i = 0; i < sizeof( from_suffix ); i++, j++ )
// 		spa_assume( from[j] == from_suffix[i] );

	char to_suffix[] = "@127.0.0.1:5060>";
	for ( i = sizeof( to ) - 1; to[i] == '\0' && i > sizeof( to_suffix ) - 1; i-- );
 	printf( "To username length: %lu\n", i - sizeof( to_suffix ) + 2 );
	for ( j = 0; j <= i; j++ )
		spa_assume( to[j] != '\0' );
	for ( j = i - sizeof( to_suffix ) + 2, i = 0; i < sizeof( to_suffix ); i++, j++ )
		spa_assume( to[j] == to_suffix[i] );

	printf( "Sending INVITE with from = \"%s\", to = \"%s\"\n", from, to );

	osip_message_t *invite;

	assert( eXosip_init() == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, 1, 5061 ) == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, socket( AF_INET, SOCK_DGRAM, 0 ), 5061 ) == OSIP_SUCCESS );
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5061, AF_INET, 0) == OSIP_SUCCESS );

	assert( eXosip_call_build_initial_invite( &invite, to, from, NULL, NULL ) == OSIP_SUCCESS );

	assert( eXosip_call_send_initial_invite( invite ) >= 0 );

// 	spa_DeclareState();

	assert( eXosip_execute() == OSIP_SUCCESS );
}

void __attribute__((noinline,used)) spa_HandleResponse() {
	spa_message_handler_entry();

// 	assert( eXosip_init() == OSIP_SUCCESS );
// 	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5061, AF_INET, 0) == OSIP_SUCCESS );
// 
// 	char from[] = "<sip:from@127.0.0.1:5061>";
// 	char to[] = "<sip:to@127.0.0.1:5060>";
// 	osip_message_t *invite;
// 	assert( eXosip_call_build_initial_invite( &invite, to, from, NULL, NULL ) == OSIP_SUCCESS );
// 
// 	eXosip_call_t *jc;
// 	osip_transaction_t *transaction;
// 	osip_event_t *sipevent;
// 
// 	// Taken from eXosip_call_send_initial_invite.
// 	assert( invite != NULL );
// 	assert( eXosip_call_init(&jc) == 0 );
// 	assert( _eXosip_transaction_init(&transaction, ICT, eXosip.j_osip, invite) == 0 );
// 	jc->c_out_tr = transaction;
// 	sipevent = osip_new_outgoing_sipmessage(invite);
// 	sipevent->transactionid = transaction->transactionid;
// 	osip_transaction_set_your_instance(transaction, __eXosip_new_jinfo(jc, NULL, NULL, NULL));
// 	osip_transaction_add_event(transaction, sipevent);
// 	jc->external_reference = NULL;
// 	ADD_ELEMENT(eXosip.j_calls, jc);
// 	eXosip_update();			/* fixed? */
// 	__eXosip_wakeup();

// 	spa_DeclareState();

// 	char message[SIP_MESSAGE_MAX_LENGTH + 1];
// 	size_t len;
// 	_eXosip_handle_incoming_message( message, len, 0, "127.0.0.1", 5060 );

// 	printf( "Ready.\n" );
	do {
		assert( eXosip_execute() == OSIP_SUCCESS );
		eXosip_event_t *e = eXosip_event_wait( 0, 50 );
		eXosip_automatic_action();
		if ( e == NULL )
			continue;
		switch ( e->type ) {
			case EXOSIP_CALL_PROCEEDING:
				printf( "Call proceeding.\n" );
				spa_valid_path();
				break;
			case EXOSIP_CALL_ACK:
				printf( "Call ack.\n" );
				spa_valid_path();
				break;
			case EXOSIP_CALL_ANSWERED:
				printf( "Call answered.\n" );
				spa_valid_path();
				break;
			default:
				printf( "Other event: %d.\n", e->type );
				break;
		}
		eXosip_event_free( e );
	} while ( 0 );
	printf( "Done.\n" );
	eXosip_quit();
}

int main( int argc, char **argv ) {
	spa_SendInvite();
	spa_HandleResponse();

	return 0;
}
