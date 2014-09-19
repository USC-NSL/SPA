#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <spdylay/spdylay.h>

#include <spa/spaRuntime.h>

#define SERVER_PORT			8000
// #define SPDY_VERSION		SPDYLAY_PROTO_SPDY2
// #define SPDY_VERSION		SPDYLAY_PROTO_SPDY3
// #define SPDY_VERSION   SPDYLAY_PROTO_SPDY3_1
// #define PUSH_METHOD			"GET"
// #define PUSH_SCHEME			"http"
// #define PUSH_PATH			"/"
// #define PUSH_HOST			"127.0.0.1"
// #define PUSH_PRIORITY		3
#define RESPONSE_STATUS		"200 OK"
#define RESPONSE_VERSION	"HTTP/1.1"
// #define RESPONSE_MAXPAIRS	3
// #define RESPONSE_MAXNAME	2
// #define RESPONSE_MAXVALUE	1
#define RESPONSE_DATA		"Response Data"
// #define RESPONSE_MAX_DATA	2
#define RECEIVE_BUFFER_SIZE	1500

// #define PUSH_HOSTPATH	PUSH_HOST ":" #SERVER_PORT

int connectionSock = -1;
int32_t stream_id = -1;

ssize_t send_callback( spdylay_session *session, const uint8_t *data, size_t length, int flags, void *user_data ) {
#ifndef ENABLE_KLEE	
	printf( "Sending %d bytes of data for %s\n", (int) length, (char *) user_data );
#endif // #ifndef ENABLE_KLEE	

	spa_msg_output( data, length, 1024, "response" );
#ifndef ENABLE_KLEE
	assert( send( connectionSock, data, length, 0 ) == length );
#endif // #ifndef ENABLE_KLEE	

	return length;
}

void ctrl_recv_callback( spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame, void *user_data ) {
#ifndef ENABLE_KLEE
	switch ( type ) {
		case SPDYLAY_SYN_STREAM: { // The SYN_STREAM control frame.
			printf( "Received SYN_STREAM control frame on %s.\n", (char *) user_data );
			printf( "	Stream ID: %d.\n", ((spdylay_syn_stream *) frame)->stream_id );
			printf( "	Associated-to-stream ID: %d.\n", ((spdylay_syn_stream *) frame)->assoc_stream_id );
			printf( "	Priority: %d.\n", ((spdylay_syn_stream *) frame)->pri );
			printf( "	Headers:\n" );
			char **nv = ((spdylay_syn_stream *) frame)->nv;
			assert( nv );
			while ( nv[0] ) {
				assert( nv[1] );
				printf( "		%s: %s\n", nv[0], nv[1] );
				nv += 2;
			}
			break;
		}
		case SPDYLAY_SYN_REPLY: { // The SYN_REPLY control frame.
			printf( "Received SYN_REPLY control frame on %s.\n", (char *) user_data );
			printf( "	Stream ID: %d.\n", ((spdylay_syn_reply *) frame)->stream_id );
			printf( "	Headers:\n" );
			char **nv = ((spdylay_syn_reply *) frame)->nv;
			assert( nv );
			while ( nv[0] ) {
				assert( nv[1] );
				printf( "		%s: %s\n", nv[0], nv[1] );
				nv += 2;
			}
			break;
		}
		case SPDYLAY_RST_STREAM: // The RST_STREAM control frame.
			printf( "Received RST_STREAM control frame on %s.\n", (char *) user_data );
			break;
		case SPDYLAY_SETTINGS: // The SETTINGS control frame.
			printf( "Received SETTINGS control frame on %s.\n", (char *) user_data );
			break;
		case SPDYLAY_NOOP: // The NOOP control frame. This was deprecated in SPDY/3.
			printf( "Received NOOP control frame on %s.\n", (char *) user_data );
			break;
		case SPDYLAY_PING: // The PING control frame.
			printf( "Received PING control frame on %s.\n", (char *) user_data );
			break;
		case SPDYLAY_GOAWAY: // The GOAWAY control frame.
			printf( "Received GOAWAY control frame on %s.\n", (char *) user_data );
			break;
		case SPDYLAY_HEADERS: // The HEADERS control frame.
			printf( "Received HEADERS control frame on %s.\n", (char *) user_data );
			break;
		case SPDYLAY_WINDOW_UPDATE: // The WINDOW_UPDATE control frame. This first appeared in SPDY/3.
			printf( "Received WINDOW_UPDATE control frame on %s.\n", (char *) user_data );
			break;
		case SPDYLAY_CREDENTIAL: // The CREDENTIAL control frame. This first appeared in SPDY/3.
			printf( "Received CREDENTIAL control frame on %s.\n", (char *) user_data );
			break;
		default:
			printf( "Received unknown control frame type on %s.\n", (char *) user_data );
			break;
	}
#endif // #ifndef ENABLE_KLEE	
}

ssize_t read_callback( spdylay_session *session, int32_t stream_id, uint8_t *buf, size_t length, int *eof, spdylay_data_source *source, void *user_data ) {
	printf( "Generating data for stream %d of %s.\n", stream_id, (char *) user_data );

#ifndef ANALYZE_RESPONSE
	assert( length >= sizeof( RESPONSE_DATA ) && "Not enough buffer space to send all response data at once." );

	bcopy( RESPONSE_DATA, buf, sizeof( RESPONSE_DATA ) );
	*eof = 1;

	return sizeof( RESPONSE_DATA );
#else // #ifndef ANALYZE_RESPONSE
	ssize_t dataLength = 0;
	spa_api_input_var( dataLength );
	spa_assume( 0 <= dataLength && dataLength <= RESPONSE_MAX_DATA );

	uint8_t data[RESPONSE_MAX_DATA];
	spa_api_input_var( data );
	bcopy( data, buf, dataLength );

	assert( length >= dataLength );
	*eof = 1;
	return dataLength;
#endif // #ifndef ANALYZE_RESPONSE #else
}

void request_recv_callback( spdylay_session *session, int32_t stream_id, void *user_data ) {
#ifndef ENABLE_KLEE	
	printf( "Receiving request on stream %d of %s\n", stream_id, (char *) user_data );
#endif // #ifndef ENABLE_KLEE	
#ifndef ANALYZE_RESPONSE
	spa_valid_path();
#endif // #ifndef ANALYZE_RESPONSE

// #ifdef ENABLE_SPA
// 	uint8_t numPairs = 0;
// 	char name[RESPONSE_MAXNAME + 1], value[RESPONSE_MAXVALUE + 1];
// 	spa_api_input_var( numPairs );
// 	spa_assume( numPairs <= RESPONSE_MAXPAIRS );
// 	spa_api_input_var( name );
// 	spa_assume( name[sizeof( name ) - 1] == '\0' );
// 	spa_api_input_var( value );
// 	spa_assume( value[sizeof( value ) - 1] == '\0' );
// 
// 	const char *nv[2 * (RESPONSE_MAXPAIRS + 1)];
// 	uint8_t i;
// 	for ( i = 0; i < numPairs; i++ ) {
// 		nv[2*i] = name;
// 		nv[2*i+1] = value;
// 	}
// 	nv[2*numPairs] = NULL;
// #else // #ifdef ENABLE_SPA
	const char *nv[] = {
		":status",	RESPONSE_STATUS,
		":version",	RESPONSE_VERSION,
		NULL
	};
// #endif // #ifdef ENABLE_SPA #else

	spdylay_data_provider dp;
	dp.source.ptr = NULL;
	dp.read_callback = read_callback;
	assert( spdylay_submit_response( session, stream_id, nv, &dp ) == SPDYLAY_OK );

	assert( spdylay_session_send( session ) == SPDYLAY_OK );
}

void __attribute__((noinline,used)) spa_HandleRequest() {
#ifndef ANALYZE_RESPONSE
	spa_message_handler_entry();
#else
	spa_api_entry();
#endif // #ifndef ANALYZE_RESPONSE #else

#ifndef ENABLE_KLEE
	int listenSock;
	struct sockaddr_in serverAddr;

	assert( (listenSock = socket( AF_INET, SOCK_STREAM, 0 )) >= 0 && "Error opening socket." );
	{
		int yes = 1;
		assert( setsockopt( listenSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( yes ) ) == 0 );
	}

	bzero( &serverAddr, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl( INADDR_ANY );
	serverAddr.sin_port = htons( SERVER_PORT );
	assert( bind( listenSock, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) == 0 && "Could not bind to server address." );

	assert( listen( listenSock, 1 ) == 0 );
	printf( "Listening for SPDY requests on port: %d\n", SERVER_PORT );

	assert( (connectionSock = accept( listenSock, NULL, NULL )) >= 0 );
	printf( "Accepting request.\n" );
#endif // #ifndef ENABLE_KLEE

	spdylay_session_callbacks callbacks;
	callbacks.send_callback = send_callback; // Callback function invoked when the |session| wants to send data to the remote peer.
	callbacks.recv_callback = NULL; // Callback function invoked when the |session| wants to receive data from the remote peer.
	callbacks.on_ctrl_recv_callback = ctrl_recv_callback; // Callback function invoked by `spdylay_session_recv()` when a control frame is received.
	callbacks.on_invalid_ctrl_recv_callback = NULL; // Callback function invoked by `spdylay_session_recv()` when an invalid control frame is received.
	callbacks.on_data_chunk_recv_callback = NULL; // Callback function invoked when a chunk of data in DATA frame is received.
	callbacks.on_data_recv_callback = NULL; // Callback function invoked when DATA frame is received.
	callbacks.before_ctrl_send_callback = NULL; // Callback function invoked before the control frame is sent.
	callbacks.on_ctrl_send_callback = NULL; // Callback function invoked after the control frame is sent.
	callbacks.on_ctrl_not_send_callback = NULL; // The callback function invoked when a control frame is not sent because of an error.
	callbacks.on_data_send_callback = NULL; // Callback function invoked after DATA frame is sent.
	callbacks.on_stream_close_callback = NULL; // Callback function invoked when the stream is closed.
	callbacks.on_request_recv_callback = request_recv_callback; // Callback function invoked when request from the remote peer is received.
	callbacks.get_credential_proof = NULL; // Callback function invoked when the library needs the cryptographic proof that the client has possession of the private key associated with the certificate.
	callbacks.get_credential_ncerts = NULL; // Callback function invoked when the library needs the length of the client certificate chain.
	callbacks.get_credential_cert = NULL; // Callback function invoked when the library needs the client certificate.
	callbacks.on_ctrl_recv_parse_error_callback = NULL; // Callback function invoked when the received control frame octets could not be parsed correctly.
	callbacks.on_unknown_ctrl_recv_callback = NULL; // Callback function invoked when the received control frame type is unknown.

	spdylay_session *session;
#ifdef SPDY_VERSION
	uint16_t serverVersion = SPDY_VERSION;
#else
	uint16_t serverVersion = 0;
	spa_api_input_var( serverVersion );
// 	spa_assume( serverVersion == SPDYLAY_PROTO_SPDY2 || serverVersion == SPDYLAY_PROTO_SPDY3 );
#endif // #ifdef SPDY_VERSION
	assert( spdylay_session_server_new( &session, serverVersion, &callbacks, "SPDY Server Session" ) == SPDYLAY_OK );

	unsigned char buf[RECEIVE_BUFFER_SIZE];
	ssize_t len = 0;
	do {
#ifndef ENABLE_KLEE
		len = recv( connectionSock, buf, RECEIVE_BUFFER_SIZE, 0 /*MSG_DONTWAIT*/ );
#endif // #ifndef ENABLE_KLEE	
		spa_msg_input( buf, sizeof( buf ), "request" );
		spa_msg_input_size( len, "request" );

		assert( len >= 0 && "Error receiving network data." );
		if ( len > 0 ) {
#ifndef ENABLE_KLEE
			printf( "Received %d bytes of data.\n", (int) len );
#endif // #ifndef ENABLE_KLEE	
			assert( spdylay_session_mem_recv( session, buf, len) == len );
		}
#ifdef ENABLE_SPA
	} while ( 0 );
#else // #ifdef ENABLE_SPA
	} while ( len > 0 );
#endif // #ifdef ENABLE_SPA #else

	printf( "Closing down session.\n" );
	spdylay_session_del( session );

#ifndef ENABLE_KLEE
	assert( close( connectionSock ) == 0 );
	assert( close( listenSock ) == 0 );
#endif // #ifndef ENABLE_KLEE	
}

int main( int argc, char **argv ) {
	spa_HandleRequest();

	return 0;
}
