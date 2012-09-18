#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <eXosip2/eX_setup.h>
#include <eXosip2/eX_call.h>

#include <spa/spaRuntime.h>

void __attribute__((noinline,used)) spa_SendInvite() {
	spa_api_entry();

// 	char from[30];
// 	spa_api_input_var( from );

// 	char to[30];
// 	spa_api_input_var( to );

	char from[] = "<sip:from@127.0.0.1:5061>";
// 	char to[] = "<sip:to@127.0.0.1:5060>";

// 	char from[30] = "<sip:@";
	char to[30] = "SIP@:";

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
	to[6] = '\0';

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
	strcat( to, "@127.0.0.1:5060>" );

	osip_message_t *invite;

	assert( eXosip_init() == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, 1, 5061 ) == OSIP_SUCCESS );
// 	assert( eXosip_set_socket( IPPROTO_UDP, socket( AF_INET, SOCK_DGRAM, 0 ), 5061 ) == OSIP_SUCCESS );
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5061, AF_INET, 0) == OSIP_SUCCESS );

	assert( eXosip_call_build_initial_invite( &invite, to, from, NULL, NULL ) == OSIP_SUCCESS );
// 	assert( eXosip_call_build_initial_invite( &invite, to_uri, from_uri, NULL, NULL ) == OSIP_SUCCESS );
// 	assert( eXosip_call_build_initial_invite( &invite, "<sip:to@127.0.0.1>", "<sip:from@127.0.0.1:5061>", NULL, NULL ) == OSIP_SUCCESS );
	assert( eXosip_call_send_initial_invite( invite ) >= 0 );
	assert( eXosip_execute() == OSIP_SUCCESS );
	eXosip_quit();
}

int main( int argc, char **argv ) {
	spa_SendInvite();

	return 0;
}
