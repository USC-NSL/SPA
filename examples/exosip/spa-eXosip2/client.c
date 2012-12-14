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

void spa_DeclareState() {
	osip_transaction_t *tmp = eXosip.j_calls->c_out_tr;
	spa_state( eXosip.j_calls, sizeof( eXosip_call_t ), "eXosip_j_calls" );
	eXosip.j_calls->c_out_tr = tmp;

	spa_state( eXosip.j_calls->c_out_tr, sizeof( osip_transaction_t ), "eXosip_j_calls_c_out_tr" );
}

void __attribute__((noinline,used)) spa_SendInvite() {
	spa_api_entry();

// 	char from[30];
// 	spa_api_input_var( from );

// 	char to[30];
// 	spa_api_input_var( to );

	char from[] = "<sip:from@127.0.0.1:5061>";
	char to[] = "<sip:to@127.0.0.1:5060>";

// PJSIP Bad Inputs
// 	char from[30] = "<sip:@"; // Good in eXoSIP server.
// 	char to[30] = "SIP@:";

// eXoSIP Bad Inputs
// 	char to[30] = "SIP@:";

// 	spa_assume( from[0] == '<' );
// 	spa_assume( from[1] == 's' );
// 	spa_assume( from[2] == 'i' );
// 	spa_assume( from[3] == 'p' );
// 	spa_assume( from[4] == ':' );
// 	spa_assume( isprint( from[5] ) );
// 	spa_assume( from[5] == 'f' );
// 	spa_assume( from[6] == '@' );
// 	spa_assume( from[7] == '1' );
// 	spa_assume( from[8] == '2' );
// 	spa_assume( from[9] == '7' );
// 	spa_assume( from[10] == '.' );
// 	spa_assume( from[11] == '0' );
// 	spa_assume( from[12] == '.' );
// 	spa_assume( from[13] == '0' );
// 	spa_assume( from[14] == '.' );
// 	spa_assume( from[15] == '1' );
// 	spa_assume( from[16] == ':' );
// 	spa_assume( from[17] == '5' );
// 	spa_assume( from[18] == '0' );
// 	spa_assume( from[19] == '6' );
// 	spa_assume( from[20] == '1' );
// 	spa_assume( from[21] == '>' );
// 	spa_assume( from[22] == '\0' );
// 
// 	spa_assume( to[0] == '<' );
// 	spa_assume( to[1] == 's' );
// 	spa_assume( to[2] == 'i' );
// 	spa_assume( to[3] == 'p' );
// 	spa_assume( to[4] == ':' );
// 	spa_assume( isprint( to[5] ) );
// 	spa_assume( to[5] == 't' );
// 	spa_assume( to[6] == '@' );
// 	spa_assume( to[7] == '1' );
// 	spa_assume( to[8] == '2' );
// 	spa_assume( to[9] == '7' );
// 	spa_assume( to[10] == '.' );
// 	spa_assume( to[11] == '0' );
// 	spa_assume( to[12] == '.' );
// 	spa_assume( to[13] == '0' );
// 	spa_assume( to[14] == '.' );
// 	spa_assume( to[15] == '1' );
// 	spa_assume( to[16] == ':' );
// 	spa_assume( to[17] == '5' );
// 	spa_assume( to[18] == '0' );
// 	spa_assume( to[19] == '6' );
// 	spa_assume( to[20] == '0' );
// 	spa_assume( to[21] == '>' );
// 	spa_assume( to[22] == '\0' );

// 	uint8_t i;
// 	for ( i = 0; i < sizeof(from) - 1; i++ )
// 		spa_assume( isprint( from[i] ) );
// 	spa_assume( from[sizeof(from)-1] == '\0' );
// 	from[sizeof(from)-1] = '\0';
// 	from[6] = '\0';
// 	for ( i = 0; i < sizeof(to) - 1; i++ )
// 		spa_assume( isprint( to[i] ) );
// 	spa_assume( to[sizeof(to)-1] == '\0' );
// 	to[sizeof(to)-1] = '\0';
// 	spa_assume( from[10] == '\0' );
// 	to[6] = '\0';

// 	char from_uri[sizeof(from)+30];
// 	from_uri[0] = '\0';
// 	strcat( from_uri, "<sip:" );
// 	strcat( from_uri, from );
// 	strcat( from_uri, "@127.0.0.1:5061>" );
// 
// 	char to_uri[sizeof(to)+30];
// 	to_uri[0] = '\0';
// 	strcat( to_uri, "<sip:" );
// 	strcat( to_uri, to );
// 	strcat( to_uri, "@127.0.0.1:5060>" );

// 	strcat( from, "@127.0.0.1:5061>" );
// 	strcat( to, "@127.0.0.1:5060>" );

// 	spa_api_input_var( from );
// 	spa_api_input_var( to );
	printf( "Sending INVITE with from = \"%s\", to = \"%s\"\n", from, to );

	osip_message_t *invite;

	assert( eXosip_init() == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, 1, 5061 ) == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, socket( AF_INET, SOCK_DGRAM, 0 ), 5061 ) == OSIP_SUCCESS );
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5061, AF_INET, 0) == OSIP_SUCCESS );

	assert( eXosip_call_build_initial_invite( &invite, to, from, NULL, NULL ) == OSIP_SUCCESS );
// 	assert( eXosip_call_build_initial_invite( &invite, to_uri, from_uri, NULL, NULL ) == OSIP_SUCCESS );
// 	assert( eXosip_call_build_initial_invite( &invite, "<sip:to@127.0.0.1>", "<sip:from@127.0.0.1:5061>", NULL, NULL ) == OSIP_SUCCESS );

	assert( eXosip_call_send_initial_invite( invite ) >= 0 );

	spa_DeclareState();

	assert( eXosip_execute() == OSIP_SUCCESS );
}

void __attribute__((noinline,used)) spa_HandleResponse() {
	spa_message_handler_entry();

	assert( eXosip_init() == OSIP_SUCCESS );
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5061, AF_INET, 0) == OSIP_SUCCESS );

	char from[] = "<sip:from@127.0.0.1:5061>";
	char to[] = "<sip:to@127.0.0.1:5060>";
	osip_message_t *invite;
	assert( eXosip_call_build_initial_invite( &invite, to, from, NULL, NULL ) == OSIP_SUCCESS );

	eXosip_call_t *jc;
	osip_transaction_t *transaction;
	osip_event_t *sipevent;

	// Taken from eXosip_call_send_initial_invite.
	assert( invite != NULL );
	assert( eXosip_call_init(&jc) == 0 );
	assert( _eXosip_transaction_init(&transaction, ICT, eXosip.j_osip, invite) == 0 );
	jc->c_out_tr = transaction;
	sipevent = osip_new_outgoing_sipmessage(invite);
	sipevent->transactionid = transaction->transactionid;
	osip_transaction_set_your_instance(transaction, __eXosip_new_jinfo(jc, NULL, NULL, NULL));
	osip_transaction_add_event(transaction, sipevent);
	jc->external_reference = NULL;
	ADD_ELEMENT(eXosip.j_calls, jc);
// 	eXosip_update();			/* fixed? */
// 	__eXosip_wakeup();

	spa_DeclareState();

// 	char message[SIP_MESSAGE_MAX_LENGTH + 1];
// 	size_t len;
// 	_eXosip_handle_incoming_message( message, len, 0, "127.0.0.1", 5060 );

	printf( "Ready.\n" );
	while ( 1 ) {
		assert( eXosip_execute() == OSIP_SUCCESS );
		eXosip_event_t *e = eXosip_event_wait( 0, 50 );
		eXosip_automatic_action();
		if ( e == NULL )
			continue;
		switch ( e->type ) {
			case EXOSIP_CALL_PROCEEDING:
				printf( "Call proceeding.\n" );
				break;
			case EXOSIP_CALL_ACK:
				printf( "Call ack.\n" );
				break;
			case EXOSIP_CALL_ANSWERED:
				printf( "Call answered.\n" );
				break;
			default:
				printf( "Other event: %d.\n", e->type );
				break;
		}
		eXosip_event_free( e );
	}
	printf( "Done.\n" );
	eXosip_quit();
}

int main( int argc, char **argv ) {
// 	spa_SendInvite();
	spa_HandleResponse();

	return 0;
}
