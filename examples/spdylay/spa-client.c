#include <assert.h>

#include <spdylay/spdylay.h>

#define SPDY_VERSION		SPDYLAY_PROTO_SPDY3
#define REQUEST_METHOD		"GET"
#define REQUEST_SCHEME		"http"
#define REQUEST_PATH		"/"
#define REQUEST_HOST		"127.0.0.1"
#define REQUEST_PORT		6121
#define REQUEST_VERSION		"HTTP/1.1"
#define REQUEST_PRIORITY	3

#define STR( x ) #x
#define REQUEST_HOSTPATH	REQUEST_HOST ":" STR( REQUEST_PORT )


void __attribute__((noinline,used)) spa_SendRequest() {
	int sock;
	struct hostent *serverHost;
	struct sockaddr_in serverAddr;

	bzero( &serverAddr, sizeof( serverAddr ) );
	serv_addr.sin_family = AF_INET;
	bcopy( serverHost->h_addr, &serverAddr.sin_addr.s_addr, serverHost->h_length );
	serv_addr.sin_port = htons(PORT);

	assert( (sock = socket( AF_INET, SOCK_STREAM, 0 )) >= 0 && "Error opening socket." );
	assert( (serverHost = gethostbyname( HOST )) != NULL && "Host not found." );
	assert( connect(sockfd,&serv_addr,sizeof(serv_addr)) >= 0 && "Error connecting to host." );
	// 	assert( fcntl( sock, F_SETFL, fcntl( sock, F_GETFL ) | O_NONBLOCK ) >= 0 && "Error setting non-blocking socket option." );

	spdylay_session_callbacks callbacks;
	callbacks.send_callback = NULL; // Callback function invoked when the |session| wants to send data to the remote peer.
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
	assert( spdylay_session_client_new( &session, SPDY_VERSION, &callbacks, "SPDY Client Session" ) == SPDYLAY_OK );

// 	assert( spdylay_session_set_initial_client_cert_origin( session, REQUEST_SCHEME, REQUEST_HOST, REQUEST_PORT ) == SPDYLAY_OK );

	char *nv[] = {
		":method",	REQUEST_METHOD,
		":scheme",	REQUEST_SCHEME,
		":path",	REQUEST_PATH,
		":version",	REQUEST_VERSION,
		":host",	REQUEST_HOSTPATH,
	};
	assert( spdylay_submit_request( session, REQUEST_PRIORITY, nv, NULL, "SPDY Client Stream 1" ) == SPDYLAY_OK );
	
	assert( spdylay_session_send( session ) == SPDYLAY_OK );
	assert( send( sock, data, len, 0 ) == len );

	unsigned char buf[RECEIVE_BUFFER_SIZE];
	ssize_t len = 0;
	do {
		len = recv( sock, buf, len, MSG_DONTWAIT );
		assert( len >= 0 && "Error receiving network data." );
		if ( len > 0 ) {
			assert( spdylay_session_mem_recv( session, buf, len) == len );
		}
	} while ( len > 0 );

	spdylay_session_del( session );
}

int main( int argc, char **argv ) {
	spa_SendRequest();
}
