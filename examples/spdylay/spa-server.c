#include <assert.h>

#include <spdylay/spdylay.h>

#define SPDY_VERSION		SPDYLAY_PROTO_SPDY3
#define PUSH_METHOD			"GET"
#define PUSH_SCHEME			"http"
#define PUSH_PATH			"/"
#define PUSH_HOST			"127.0.0.1"
#define PUSH_PORT			6121
#define PUSH_PRIORITY		3
#define RESPONSE_STATUS		"200 OK"
#define RESPONSE_VERSION	"HTTP/1.1"

#define RESPONSE_HOSTPATH	RESPONSE_HOST ":" #RESPONSE_PORT

int32_t stream_id = -1;

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
	assert( spdylay_session_server_new( &session, SPDY_VERSION, &callbacks, "SPDY Server Session" ) == SPDYLAY_OK );

	char *nv[] = {
		":status",	RESPONSE_STATUS,
		":version",	RESPONSE_VERSION,
	};
	assert( spdylay_submit_response( session, stream_id, nv, NULL ) == SPDYLAY_OK );

	assert( spdylay_session_send( session ) == SPDYLAY_OK );
	assert( spdylay_session_mem_recv( session, buf, len) == len );

	spdylay_session_del( session );
}
