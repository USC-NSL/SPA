#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <eXosip2/eX_setup.h>
#include <eXosip2/eX_call.h>
#include <eXosip2/eX_subscribe.h>

#include <spa/spaRuntime.h>

#define MIN( a, b ) ((a) < (b) ? (a) : (b))
#define MAX( a, b ) ((a) > (b) ? (a) : (b))

char *getConstantFrom() {
	static char from[] = "<sip:f@127.0.0.1:5061>";

// PJSIP Bad Inputs
// 	static char from[30] = "<sip:@"; // Good in eXoSIP server.

	return from;
}

char *getInputFrom() {
	static char from[23];
	spa_api_input_var( from );
	spa_assume( from[sizeof( from ) - 1] == '\0' );

	size_t i, j;
// 	char from_prefix[] = "<sip:";
	char from_suffix[] = "@127.0.0.1:5061>";
// 	assert( sizeof( from ) > sizeof( from_prefix ) + sizeof( from_suffix ) - 1 );
	assert( sizeof( from ) > sizeof( from_suffix ) );
// 	for ( i = 0; i < sizeof( from_prefix ) - 1; i++ )
// 		spa_assume( from[i] == from_prefix[i] );
// 		from[i] = from_prefix[i];
	for ( i = sizeof( from_suffix ) - 1; from[i] != '\0'; i++ );
// 	printf( "From username length: %lu\n", i - (sizeof( from_suffix ) - 1) );
	for ( j = 0; j < i; j++ )
		spa_assume( from[j] != '\0' );
	i -= sizeof( from_suffix ) - 1;
	for ( j = 0; j < sizeof( from_suffix ) - 1; i++, j++ )
		spa_assume( from[i] == from_suffix[j] );
// 		from[i] = from_suffix[j];

	return from;
}

char *getConstantTo() {
	static char to[] = "<sip:t@127.0.0.1:5060>";

// PJSIP Bad Inputs
// 	static char to[30] = "SIP@:";

// eXoSIP Bad Inputs
// 	static char to[30] = "SIP@:";

	return to;
}

char *getInputTo() {
	static char to[23];
	spa_api_input_var( to );
	spa_assume( to[sizeof( to ) - 1] == '\0' );

	size_t i, j;
// 	char to_prefix[] = "<sip:";
	char to_suffix[] = "@127.0.0.1:5060>";
// 	assert( sizeof( to ) > sizeof( to_prefix ) + sizeof( to_suffix ) - 1 );
	assert( sizeof( to ) > sizeof( to_suffix ) );
// 	for ( i = 0; i < sizeof( to_prefix ) - 1; i++ )
// 		spa_assume( to[i] == to_prefix[i] );
// 		to[i] = to_prefix[i];
	for ( i = sizeof( to_suffix ) - 1; to[i] != '\0'; i++ );
// 	printf( "To username length: %lu\n", i - (sizeof( to_suffix ) - 1) );
	for ( j = 0; j < i; j++ )
		spa_assume( to[j] != '\0' );
	i -= sizeof( to_suffix ) - 1;
	for ( j = 0; j < sizeof( to_suffix ) - 1; i++, j++ )
		spa_assume( to[i] == to_suffix[j] );
// 		to[i] = to_suffix[j];

	return to;
}

void init() {
	assert( eXosip_init() == OSIP_SUCCESS );
	assert( eXosip_listen_addr( IPPROTO_UDP, "0.0.0.0", 5061, AF_INET, 0) == OSIP_SUCCESS );
}

void execute() {
	assert( eXosip_execute() == OSIP_SUCCESS );
}

void __attribute__((noinline,used)) spa_SendInviteFrom() {
// 	spa_api_entry();
	
	init();
	osip_message_t *msg;
	assert( eXosip_call_build_initial_invite( &msg, getConstantTo(), getInputFrom(), NULL, NULL ) == OSIP_SUCCESS );
	assert( eXosip_call_send_initial_invite( msg ) >= 0 );
	execute();
}

void __attribute__((noinline,used)) spa_SendInviteTo() {
// 	spa_api_entry();
	
	init();
	osip_message_t *msg;
	assert( eXosip_call_build_initial_invite( &msg, getInputTo(), getConstantFrom(), NULL, NULL ) == OSIP_SUCCESS );
	assert( eXosip_call_send_initial_invite( msg ) >= 0 );
	execute();
}

void __attribute__((noinline,used)) spa_SendOptionsFrom() {
// 	spa_api_entry();
	
	init();
	osip_message_t *msg;
	assert( eXosip_options_build_request( &msg, getConstantTo(), getInputFrom(), NULL ) == OSIP_SUCCESS );
	assert( eXosip_options_send_request( msg ) >= 0 );
	execute();
}

void __attribute__((noinline,used)) spa_SendOptionsTo() {
	spa_api_entry();

	init();
	osip_message_t *msg;
	assert( eXosip_options_build_request( &msg, getInputTo(), getConstantFrom(), NULL ) == OSIP_SUCCESS );
	assert( eXosip_options_send_request( msg ) >= 0 );
	execute();
}

void __attribute__((noinline,used)) spa_SendSubscribe() {
// 	spa_api_entry();

// 	char event[] = "presence";
// 	char event[] = "foo";
	int expires = 300;

	char event[4];
	spa_api_input_var( event );
	spa_assume( event[sizeof( event ) - 1] == '\0' );
// 	int i;
// 	for ( i = 0; i < sizeof( event ) - 1; i++ )
// 		spa_assume( event[i] != '\0' );

	init();
	osip_message_t *msg;
	assert( eXosip_subscribe_build_initial_request( &msg, getConstantTo(), getConstantFrom(), NULL, event, expires ) == OSIP_SUCCESS );
	assert( eXosip_subscribe_send_initial_request( msg ) >= 0 );
	execute();
}

int main( int argc, char **argv ) {
// 	spa_SendInviteFrom();
// 	spa_SendInviteTo();
// 	spa_SendOptionsFrom();
	spa_SendOptionsTo();
// 	spa_SendSubscribe();

	return 0;
}
