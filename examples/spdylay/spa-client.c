#include <spdylay/spdylay.h>

#define SPDY_VERSION	SPDYLAY_PROTO_SPDY3
#define REQUEST_METHOD	"GET"
#define REQUEST_SCHEME	"http"
#define REQUEST_PATH	"/"
#define REQUEST_PATH	"/"
#define REQUEST_HOST	"127.0.0.1"
#define REQUEST_PORT	6121
#define REQUEST_VERSION	"HTTP/1.1"

#define REQUEST_HOSTPATH	REQUEST_HOST ":" #REQUEST_PORT


int main( int argc, char **argv ) {
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
	assert( spdylay_submit_request( session, REQUEST_PRIORITY, const char **nv, NULL, "SPDY Client Stream 1" ) == SPDYLAY_OK );
	
	assert( spdylay_session_send( session ) == SPDYLAY_OK );
	assert( spdylay_session_mem_recv( session, buf, len) == len );

	spdylay_session_del( session );
}
