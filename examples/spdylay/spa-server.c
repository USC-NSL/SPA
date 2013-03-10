#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <spdylay/spdylay.h>

#include <spa/spaRuntime.h>

#define SERVER_PORT			6121
#define SPDY_VERSION		SPDYLAY_PROTO_SPDY3
#define PUSH_METHOD			"GET"
#define PUSH_SCHEME			"http"
#define PUSH_PATH			"/"
#define PUSH_HOST			"127.0.0.1"
#define PUSH_PRIORITY		3
#define RESPONSE_STATUS		"200 OK"
#define RESPONSE_VERSION	"HTTP/1.1"
#define RECEIVE_BUFFER_SIZE	100

#define PUSH_HOSTPATH	PUSH_HOST ":" #SERVER_PORT

int connectionSock = -1;
int32_t stream_id = -1;

ssize_t send_callback( spdylay_session *session, const uint8_t *data, size_t length, int flags, void *user_data ) {
	printf( "Sending data for %s\n", (char *) user_data );

	spa_msg_output( data, length, 1024, "response" );
#ifndef ENABLE_KLEE
	assert( send( connectionSock, data, length, 0 ) == length );
#endif // #ifndef ENABLE_KLEE	

	return length;
}

void __attribute__((noinline,used)) spa_HandleRequest() {
	spa_message_handler_entry();

	int listenSock;
	struct sockaddr_in serverAddr;

	assert( (listenSock = socket( AF_INET, SOCK_STREAM, 0 )) >= 0 && "Error opening socket." );

	bzero( &serverAddr, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl( INADDR_ANY );
	serverAddr.sin_port = htons( SERVER_PORT );
	assert( bind( listenSock, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) == 0 && "Could not bind to server address." );

	assert( listen( listenSock, 1 ) == 0 );

	assert( (connectionSock = accept( listenSock, NULL, NULL )) >= 0 );

	spdylay_session_callbacks callbacks;
	callbacks.send_callback = send_callback; // Callback function invoked when the |session| wants to send data to the remote peer.
	callbacks.recv_callback = NULL; // Callback function invoked when the |session| wants to receive data from the remote peer.
	callbacks.on_ctrl_recv_callback = NULL; // Callback function invoked by `spdylay_session_recv()` when a control frame is received.
	callbacks.on_invalid_ctrl_recv_callback = NULL; // Callback function invoked by `spdylay_session_recv()` when an invalid control frame is received.
	callbacks.on_data_chunk_recv_callback = NULL; // Callback function invoked when a chunk of data in DATA frame is received.
	callbacks.on_data_recv_callback = NULL; // Callback function invoked when DATA frame is received.
	callbacks.before_ctrl_send_callback = NULL; // Callback function invoked before the control frame is sent.
	callbacks.on_ctrl_send_callback = NULL; // Callback function invoked after the control frame is sent.
	callbacks.on_ctrl_not_send_callback = NULL; // The callback function invoked when a control frame is not sent because of an error.
	callbacks.on_data_send_callback = NULL; // Callback function invoked after DATA frame is sent.
	callbacks.on_stream_close_callback = NULL; // Callback function invoked when the stream is closed.
	callbacks.on_request_recv_callback = NULL; // Callback function invoked when request from the remote peer is received.
	callbacks.get_credential_proof = NULL; // Callback function invoked when the library needs the cryptographic proof that the client has possession of the private key associated with the certificate.
	callbacks.get_credential_ncerts = NULL; // Callback function invoked when the library needs the length of the client certificate chain.
	callbacks.get_credential_cert = NULL; // Callback function invoked when the library needs the client certificate.
	callbacks.on_ctrl_recv_parse_error_callback = NULL; // Callback function invoked when the received control frame octets could not be parsed correctly.
	callbacks.on_unknown_ctrl_recv_callback = NULL; // Callback function invoked when the received control frame type is unknown.

	spdylay_session *session;
	assert( spdylay_session_server_new( &session, SPDY_VERSION, &callbacks, "SPDY Server Session" ) == SPDYLAY_OK );

	unsigned char buf[RECEIVE_BUFFER_SIZE];
	ssize_t len = 0;
// 	do {
#ifndef ENABLE_KLEE
		len = recv( connectionSock, buf, len, 0 /*MSG_DONTWAIT*/ );
#endif // #ifndef ENABLE_KLEE	
		spa_msg_input( buf, sizeof( buf ), "request" );
		spa_msg_input_size( len, "request" );

		assert( len >= 0 && "Error receiving network data." );
		if ( len > 0 ) {
			assert( spdylay_session_mem_recv( session, buf, len) == len );
		}
// 	} while ( len > 0 );

	const char *nv[] = {
		":status",	RESPONSE_STATUS,
		":version",	RESPONSE_VERSION,
	};
	assert( spdylay_submit_response( session, stream_id, nv, NULL ) == SPDYLAY_OK );

	assert( spdylay_session_send( session ) == SPDYLAY_OK );

	spa_valid_path();

	spdylay_session_del( session );
}

int main( int argc, char **argv ) {
	spa_HandleRequest();

	return 0;
}
