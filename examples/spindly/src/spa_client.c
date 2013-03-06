// #include <assert.h>
// #include <ctype.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netdb.h>

#include <spa/spaRuntime.h>

#define HOST			"127.0.0.1"
#define PORT			6121
#define SPDY_VERSION	2
#define NUM_HEADERS		4
#define METHOD_NAME		"method"
#define METHOD_VALUE	"GET"
#define SCHEME_NAME		"scheme"
#define SCHEME_VALUE	"http"
#define URL_NAME		"url"
#define URL_VALUE		"/"
#define VERSION_NAME	"version"
#define VERSION_VALUE	"HTTP/1.1"
#define PRIORITY	7

void __attribute__((noinline,used)) spa_SendRequest() {
	int sock;
	struct hostent *serverHost;
	struct sockaddr_in serverAddr;

	bzero( &serverAddr, sizeof( serverAddr ) );
	serv_addr.sin_family = AF_INET;
	bcopy( serverHost->h_addr, &serverAddr.sin_addr.s_addr, serverHost->h_length );
	serv_addr.sin_port = htons(PORT);


	struct spindly_phys_config physConfig;
	physConfig.age = SPINDLY_CONFIG_AGE;

	struct spindly_stream_config streamConfig;
	streamConfig.age = SPINDLY_CONFIG_AGE;

	struct spindly_header_pair headers[NUM_HEADERS];
	unsigned h = 0;
	spindly_header_pair[h].name = METHOD_NAME;
	spindly_header_pair[h].namelen = strlen( spindly_header_pair[h].name );
	spindly_header_pair[h].value = METHOD_VALUE;
	spindly_header_pair[h].valuelen = strlen( spindly_header_pair[h].value );
	h++;
	spindly_header_pair[h].name = SCHEME_NAME;
	spindly_header_pair[h].namelen = strlen( spindly_header_pair[h].name );
	spindly_header_pair[h].value = SCHEME_VALUE;
	spindly_header_pair[h].valuelen = strlen( spindly_header_pair[h].value );
	h++;
	spindly_header_pair[h].name = URL_NAME;
	spindly_header_pair[h].namelen = strlen( spindly_header_pair[h].name );
	spindly_header_pair[h].value = URL_VALUE;
	spindly_header_pair[h].valuelen = strlen( spindly_header_pair[h].value );
	h++;
	spindly_header_pair[h].name = VERSION_NAME;
	spindly_header_pair[h].namelen = strlen( spindly_header_pair[h].name );
	spindly_header_pair[h].value = VERSION_VALUE;
	spindly_header_pair[h].valuelen = strlen( spindly_header_pair[h].value );


	assert( (sock = socket( AF_INET, SOCK_STREAM, 0 )) >= 0 && "Error opening socket." );
	assert( (serverHost = gethostbyname( HOST )) != NULL && "Host not found." );
	assert( connect(sockfd,&serv_addr,sizeof(serv_addr)) >= 0 && "Error connecting to host." );
// 	assert( fcntl( sock, F_SETFL, fcntl( sock, F_GETFL ) | O_NONBLOCK ) >= 0 && "Error setting non-blocking socket option." );
	struct spindly_phys *phys = spindly_phys_init( CLIENT, SPDY_VERSION, &physConfig );

	struct spindly_stream *stream = NULL;
	assert( spindly_stream_new( phys, PRIORITY, &stream, "Stream 1", streamConfig ) == SPINDLYE_OK );

	assert( spindly_stream_headers( stream, NUM_HEADERS, &headers, "Stream 1 Headers" ) ==  SPINDLYE_OK );

	do {
		{ // spindly -> network.
			unsigned char *data = NULL;
			size_t len = 0;
			do { 
				assert( spindly_phys_outgoing( phys, &data, &len ) == SPINDLYE_OK );
				if ( len > 0 ) {
					assert( send( sock, data, len, 0 ) == len );
					assert( spindly_phys_sent( phys, len ) == SPINDLYE_OK );
				}
			} while ( len > 0 );
		}

		{ // network -> spindly.
			unsigned char buf[RECEIVE_BUFFER_SIZE];
			ssize_t len = 0;
			do {
				len = recv( sock, buf, len, MSG_DONTWAIT );
				assert( len >= 0 && "Error receiving network data." );
				if ( len > 0 ) {
					spindly_phys_incoming( phys, buf, len );
				}
			} while ( len > 0 );
		}

		// spindly -> app events.
		struct spindly_demux dmx;
		do {
			spindly_phys_demux( phys, &dmx );

			switch ( dmx.type ) {
				case SPINDLY_DX_NONE: // No event.
					break;
				case SPINDLY_DX_STREAM_ACK:
					/* one (more) of our streams is now setup, send data */
					spindly_stream_send(PTR->stream, "hello", 5);
					break;
				case SPINDLY_DX_STREAM_REQ:
					/* the peer wants to setup a stream, allow it? */
					spindly_stream_ack(PTR->stream); /* sure! */
					break;
				case SPINDLY_DX_STREAM_KILL:
					/* the stream is killed, kill the stream handle */
					spindly_stream_close(PTR->stream);
					break;
				case SPINDLY_DX_DATA:
					/* data arrives on a stream */
					write(1, PTR->data, PTR->len); /* write to stdout */
					break;
			}

			/* loop around as long as there are messages to handle */
		} while( dmx.type != SPINDLY_DX_NONE );
}









 
 while( !until data comes OR data can be sent ) {
	 
	 
	 
 } /* while () */ 