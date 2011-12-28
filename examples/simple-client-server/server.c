#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#define PORT	10000

int main( int argc, char **argv ) {
	int serverSocket, clientSocket;
	socklen_t addrLen;
	uint32_t n;
	struct sockaddr_in serverAddr, clientAddr;

	klee_make_symbolic( &n, sizeof( n ), "n" );

	// Get Internet domain socket.
	if ( (serverSocket = socket( AF_INET, SOCK_STREAM, 0 )) == -1 ) {
		perror( "socket" );
		return 1;
	}

	// Fill in the socket structure.
	memset( &serverAddr, 0, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons( PORT );

	// Bind the socket to the port.
	if ( bind( serverSocket, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) == -1 ) {
		perror( "bind" );
		return 1;
	}

	// Listen for incoming connections.
	if ( listen( serverSocket, 1 ) == -1 ) {
		perror( "listen" );
		return 1;
	}

	// Process incoming connections.
	while ( 1 ) {
		addrLen = sizeof( clientAddr );
		if ( (clientSocket = accept( serverSocket, (struct sockaddr *)  &clientAddr, &addrLen )) == -1 ) {
			perror( "accept" );
			return 1;
		}

		printf( "Connection from %s:%d\n", inet_ntoa( clientAddr.sin_addr ), ntohs( clientAddr.sin_port ) );

		// Get integer from client.
		if ( recv( clientSocket, &n, sizeof( n ), 0 ) == -1 ) {
			perror( "recv" );
			return 1;
		}

		// Reply with integer + 1.
		n = htonl( ntohl( n ) + 1 );
		if ( send( clientSocket, &n, sizeof( n ), 0 ) == -1 ) {
			perror( "send" );
			return 1;
		}

		// Close client socket.
		close( clientSocket );
	}
}

 
