#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define HOST	"localhost"
#define PORT	10000

int main( int argc, char **argv ) {
	int	sock;
	uint32_t n;
	struct sockaddr_in serverAddr;
	struct hostent *host;

	klee_make_symbolic( &n, sizeof( n ), "n" );

	if ( argc != 2 ) {
		printf( "Usage:\n	client <integer>\n" );
		return 1;
	}
	n = htonl( atoi( argv[1] ) );

	// Translate host name.
	if ( (host = gethostbyname( HOST )) == 0 ) {
		perror( "gethostbyname" );
		return 1;
	}

	// Fill in the socket structure with host information.
	memset( &serverAddr, 0, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ((struct in_addr *) (host->h_addr))->s_addr;
	serverAddr.sin_port = htons( PORT );

	// Get Internet domain socket.
	if ( (sock = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
		perror( "socket" );
		return 1;
	}

	// Connect.
	if ( connect( sock, (struct sockaddr *)  &serverAddr, sizeof( serverAddr ) ) == -1 ) {
		perror( "connect" );
		return 1;
	}

	// Send integer.
	if ( send( sock, &n, sizeof( n ), 0 ) == -1 ) {
		perror( "send" );
		return 1;
	}

	// Get result.
	if ( recv( sock, &n, sizeof( n ), 0 ) == -1 ) {
		perror("recv");
		return 1;
	}
	n = ntohl( n );

	printf( "%d\n", n );
	if ( n == 1 ) {
		printf( "One.\n" );
	}

	close( sock );
	return 0;
}
