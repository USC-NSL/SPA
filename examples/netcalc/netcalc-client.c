#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <map>
#include <iostream>

#include "netcalc.h"

#define DEFAULT_HOST	"localhost"

int main( int argc, char **argv ) {
	assert( argc >= 4 && "Too few arguments." );
	assert( argc <= 5 && "Too many arguments." );

	nc_query_t query;
	query.arg1 = atol( argv[1] );
	query.arg2 = atol( argv[3] );
	query.op = getOpByName( std::string() + argv[2] );

	const char *host = DEFAULT_HOST;
	if ( argc >= 5 )
		host = argv[4];

	std::cerr << "Sending query to " << host << "." << std::endl;

	struct addrinfo hints, *base_result, *result;
	bzero( &hints, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	assert( ! getaddrinfo( host, NULL, &hints, &base_result ) );

	int s;
	for ( result = base_result; result; result = result->ai_next ) {
		((struct sockaddr_in *) result->ai_addr)->sin_port = htons( NETCALC_UDP_PORT );
		if ( (s = socket( result->ai_family, result->ai_socktype, result->ai_protocol )) < 0 )
			continue;
		break;
	}
	assert( s >= 0 && "Host could not be resolved." );

	assert( sendto( s, &query, sizeof( query ), 0, result->ai_addr, result->ai_addrlen ) == sizeof( query ) );
	freeaddrinfo( base_result );

	nc_response_t response;
	assert( recv( s, &response, sizeof( response ), 0 ) == sizeof( response ) );

	if ( response.err == NC_OK ) {
		std::cout << query.arg1 << " " << getOpName( query.op ) << " " << query.arg2 << " = " << response.value << std::endl;
	} else {
		std::cerr << "Error: " << getErrText( response.err ) << std::endl;
	}
}
