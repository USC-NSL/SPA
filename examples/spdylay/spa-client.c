#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <spdylay/spdylay.h>

#include <spa/spaRuntime.h>

// #define SPDY_VERSION		SPDYLAY_PROTO_SPDY2
// #define SPDY_VERSION		SPDYLAY_PROTO_SPDY3
// #define SPDY_VERSION   SPDYLAY_PROTO_SPDY3_1
#define REQUEST_METHOD		"GET"
#define REQUEST_SCHEME		"http"
#define REQUEST_PATH			"/"
#define REQUEST_HOST			"127.0.0.1"
#define REQUEST_PORT			8000
#define REQUEST_VERSION		"HTTP/1.1"
// #define REQUEST_PRIORITY	3
// #define REQUEST_MAXNVBUF 100
// #define REQUEST_MAXNVPAIRS 10
// #define REQUEST_MAXMETHOD		5
#define REQUEST_MAXPATH			10
// #define REQUEST_MAXVERSION	9
// #define REQUEST_MAXHOST			10
// #define REQUEST_MAXSCHEME		6
#define REQUEST_MAXNAME			3
#define REQUEST_MAXVALUE		3
#define RECEIVE_BUFFER_SIZE	1500

#define QUOTE( str ) #str
#define EXPAND_AND_QUOTE( str ) QUOTE( str )
#define REQUEST_HOSTPATH	REQUEST_HOST ":" EXPAND_AND_QUOTE( REQUEST_PORT )


int sock = -1;

ssize_t send_callback( spdylay_session *session, const uint8_t *data, size_t length, int flags, void *user_data ) {
	spa_msg_output( data, length, 1024, "request" );

#ifndef ENABLE_KLEE
	printf( "Sending %d bytes of data for %s\n", (int) length, (char *) user_data );
	assert( send( sock, data, length, 0 ) == length );
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
      assert( ((spdylay_settings *) frame)->iv );
      printf( "	Headers:\n" );
      spdylay_settings_entry *iv;
      for (iv = ((spdylay_settings *) frame)->iv; iv - ((spdylay_settings *) frame)->iv < ((spdylay_settings *) frame)->niv; iv++) {
        printf( "		%d: %d (%d)\n", iv->settings_id, iv->value, iv->flags);
      }
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

void data_chunk_recv_callback( spdylay_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data ) {
#ifndef ENABLE_KLEE
  char data_str[len + 1];
  strncpy(data_str, (char *) data, len);
  printf( "Received data on stream %d of %s: \"%s\"\n", stream_id, (char *) user_data, data_str );
#endif // #ifndef ENABLE_KLEE
	spa_valid_path();
}

void __attribute__((noinline,used)) spa_SendRequest() {
#ifndef ANALYZE_RESPONSE
	spa_api_entry();
#else
	spa_message_handler_entry();
#endif // #ifndef ANALYZE_RESPONSE #else

#ifndef ENABLE_KLEE
	struct hostent *serverHost;
	struct sockaddr_in serverAddr;

	assert( (serverHost = gethostbyname( REQUEST_HOST )) != NULL && "Host not found." );
	bzero( &serverAddr, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	bcopy( serverHost->h_addr, &serverAddr.sin_addr.s_addr, serverHost->h_length );
	serverAddr.sin_port = htons( REQUEST_PORT );

	assert( (sock = socket( AF_INET, SOCK_STREAM, 0 )) >= 0 && "Error opening socket." );
	assert( connect( sock, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) >= 0 && "Error connecting to host." );
// 	assert( fcntl( sock, F_SETFL, fcntl( sock, F_GETFL ) | O_NONBLOCK ) >= 0 && "Error setting non-blocking socket option." );
	printf( "Connected to remote host.\n" );
#endif // #ifndef ENABLE_KLEE

	spdylay_session_callbacks callbacks;
	callbacks.send_callback = send_callback; // Callback function invoked when the |session| wants to send data to the remote peer.
	callbacks.recv_callback = NULL; // Callback function invoked when the |session| wants to receive data from the remote peer.
	callbacks.on_ctrl_recv_callback = ctrl_recv_callback; // Callback function invoked by `spdylay_session_recv()` when a control frame is received.
	callbacks.on_invalid_ctrl_recv_callback = NULL; // Callback function invoked by `spdylay_session_recv()` when an invalid control frame is received.
	callbacks.on_data_chunk_recv_callback = data_chunk_recv_callback; // Callback function invoked when a chunk of data in DATA frame is received.
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
	uint16_t clientVersion = 0;
#ifdef SPDY_VERSION
	clientVersion = SPDY_VERSION;
#else // #ifdef SPDY_VERSION
	spa_api_input_var( clientVersion );
#endif // #ifdef SPDY_VERSION #else
	assert( spdylay_session_client_new( &session, clientVersion, &callbacks, "SPDY Client Session" ) == SPDYLAY_OK );

	uint8_t priority = 0;
#ifdef REQUEST_PRIORITY
	priority = REQUEST_PRIORITY;
#else // #ifdef REQUEST_PRIORITY
	spa_api_input_var( priority );
#endif // #ifdef REQUEST_PRIORITY #else

#ifdef ANALYZE_RESPONSE
	const char *nv[] = {
		NULL
	};
#elif defined( ENABLE_SPA ) // #ifdef ANALYZE_RESPONSE
//   uint8_t numPairs = 0;
//   char name[REQUEST_MAXNAME + 1], value[REQUEST_MAXVALUE + 1];
//   spa_api_input_var( numPairs );
//   spa_assume( numPairs <= REQUEST_MAXNVPAIRS );
//   spa_api_input_var( name );
//   spa_assume( name[sizeof( name ) - 1] == '\0' );
//   spa_api_input_var( value );
//   spa_assume( value[sizeof( value ) - 1] == '\0' );
// 
//   const char *nv[2 * (REQUEST_MAXNVPAIRS + 1)];
//   uint8_t i;
//   for ( i = 0; i < numPairs; i++ ) {
//     nv[2*i] = name;
//     nv[2*i+1] = value;
//   }
//   nv[2*numPairs] = NULL;

//   char nvbuf[REQUEST_MAXNVBUF];
//   spa_api_input_var(nvbuf);
//   spa_assume(nvbuf[sizeof(nvbuf) - 1] == '\0');
// 
//   uint16_t numPairs;
//   spa_api_input_var(numPairs);
//   spa_assume(numPairs <= REQUEST_MAXNVPAIRS);
// 
//   const char *nv[REQUEST_MAXNVPAIRS * 2 + 2];
//   int pair, pos = 0;
//   for (pair = 0; pair != numPairs; pair++) {
//     int i;
//     for (i = 0; i < 2; i++ ) {
//       assert(pos < sizeof(nvbuf));
//       for (; pos < sizeof(nvbuf); pos++) {
//         if (pos == 0 || nvbuf[pos - 1] == '\0') {
//           nv[pair + i] = &nvbuf[pos];
//           break;
//         }
//       }
//     }
//   }
//   nv[pair] = NULL;
//   nv[pair + 1] = NULL;

//   char name1[REQUEST_MAXNAME];
//   spa_api_input_var(name1);
//   spa_assume(name1[sizeof(name1) - 1] == '\0');
// 
//   char value1[REQUEST_MAXNAME];
//   spa_api_input_var(value1);
//   spa_assume(value1[sizeof(value1) - 1] == '\0');
// 
//   char name2[REQUEST_MAXNAME];
//   spa_api_input_var(name2);
//   spa_assume(name2[sizeof(name2) - 1] == '\0');
// 
//   char value2[REQUEST_MAXNAME];
//   spa_api_input_var(value2);
//   spa_assume(value2[sizeof(value2) - 1] == '\0');
// 
//   char name3[REQUEST_MAXNAME];
//   spa_api_input_var(name3);
//   spa_assume(name3[sizeof(name3) - 1] == '\0');
// 
//   char value3[REQUEST_MAXNAME];
//   spa_api_input_var(value3);
//   spa_assume(value3[sizeof(value3) - 1] == '\0');
// 
//   const char *nv[] = {
//     name1,  value1,
//     name2,  value2,
//     name3,  value3,
//     NULL
//   };

//   char method[REQUEST_MAXMETHOD];
//   spa_api_input_var(method);
//   spa_assume(method[sizeof(method) - 1] == '\0');
// 
//   char path[REQUEST_MAXPATH];
//   spa_api_input_var(path);
//   spa_assume(path[sizeof(path) - 1] == '\0');
// 
//   char version[REQUEST_MAXVERSION];
//   spa_api_input_var(version);
//   spa_assume(version[sizeof(version) - 1] == '\0');
// 
//   char host[REQUEST_MAXHOST];
//   spa_api_input_var(host);
//   spa_assume(host[sizeof(host) - 1] == '\0');
// 
//   char scheme[REQUEST_MAXSCHEME];
//   spa_api_input_var(scheme);
//   spa_assume(scheme[sizeof(scheme) - 1] == '\0');
// 
//   char name[REQUEST_MAXNAME];
//   spa_api_input_var(name);
//   spa_assume(name[sizeof(name) - 1] == '\0');
// 
//   char value[REQUEST_MAXVALUE];
//   spa_api_input_var(value);
//   spa_assume(value[sizeof(value) - 1] == '\0');
// 
//   const char *nv[] = {
//     ":method",  method,
//     ":path",  path,
//     ":version", version,
//     ":host",  host,
//     ":scheme", scheme,
//     name, value,
//     NULL
//   };

  const char *methods[] = {
    "GET", "POST", "HEAD", "OPTIONS", "PUT", "DELETE", "TRACE", "CONNECT"
  };
  uint8_t methodId = 0;
  spa_api_input_var(methodId);
  spa_assume(methodId < (sizeof(methods) / sizeof(methods[0])));
  const char *method = methods[methodId];

  char path[REQUEST_MAXPATH];
  spa_api_input_var(path);
  spa_assume(path[0] == '/');
  spa_assume(path[sizeof(path) - 1] == '\0');

  const char *versions[] = {
    "HTTP/1.1", "HTTP/1.0", "HTTP/0.9"
  };
  uint8_t versionId = 0;
  spa_api_input_var(versionId);
  spa_assume(versionId < (sizeof(versions) / sizeof(versions[0])));
  const char *version = versions[versionId];

  char host[] = REQUEST_HOSTPATH;

  const char *schemes[] = {
    "http", "https"
  };
  uint8_t schemeId = 0;
  spa_api_input_var(schemeId);
  spa_assume(schemeId < (sizeof(schemes) / sizeof(schemes[0])));
  const char *scheme = schemes[schemeId];

  char name1[REQUEST_MAXNAME];
  spa_api_input_var(name1);
  spa_assume(name1[sizeof(name1) - 1] == '\0');

  char value1[REQUEST_MAXVALUE];
  spa_api_input_var(value1);
  spa_assume(value1[sizeof(value1) - 1] == '\0');

  char name2[REQUEST_MAXNAME];
  spa_api_input_var(name2);
  spa_assume(name2[sizeof(name2) - 1] == '\0');

  char value2[REQUEST_MAXVALUE];
  spa_api_input_var(value2);
  spa_assume(value2[sizeof(value2) - 1] == '\0');

  const char *nv[] = {
    ":method",  method,
    ":path",    path,
    ":version", version,
    ":host",    host,
    ":scheme",  scheme,
    name1,      value1,
    name2,      value2,
    NULL
  };
#else // #ifdef ANALYZE_RESPONSE #elif defined( ENABLE_SPA )
	const char *nv[] = {
		":method",	REQUEST_METHOD,
		":scheme",	REQUEST_SCHEME,
		":path",	REQUEST_PATH,
		":version",	REQUEST_VERSION,
		":host",	REQUEST_HOSTPATH,
		NULL
	};
#endif // #ifdef ANALYZE_RESPONSE #elif defined( ENABLE_SPA ) #else
	assert( spdylay_submit_request( session, priority, nv, NULL, "SPDY Client Stream 1" ) == SPDYLAY_OK );

	assert( spdylay_session_send( session ) == SPDYLAY_OK );

	unsigned char buf[RECEIVE_BUFFER_SIZE];
	ssize_t len = 0;
	do {
#ifndef ENABLE_KLEE
		len = recv( sock, buf, RECEIVE_BUFFER_SIZE, 0 /*MSG_DONTWAIT*/ );
#endif // #ifndef ENABLE_KLEE	
#ifdef ANALYZE_RESPONSE
		spa_msg_input( buf, RECEIVE_BUFFER_SIZE, "response" );
		spa_msg_input_size( len, "response" );
#endif // #ifdef ANALYZE_RESPONSE
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

#ifndef ENABLE_KLEE
	printf( "Closing down session.\n" );
#endif // #ifndef ENABLE_KLEE
	spdylay_session_del( session );

#ifndef ENABLE_KLEE
	assert( close( sock ) == 0 );
#endif // #ifndef ENABLE_KLEE	
	}

int main( int argc, char **argv ) {
	spa_SendRequest();

	return 0;
}
