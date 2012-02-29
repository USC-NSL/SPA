#include <assert.h>
#include <arpa/inet.h>
// #include <netinet/in.h>
// #include <stdio.h>
// #include <sys/types.h>
#include <sys/socket.h>
// #include <unistd.h>
#include <strings.h>

#include <iostream>
#include <limits>

#include "netcalc.h"

void handleQuery( nc_query_t &query, nc_response_t &response ) {
	response.err = NC_OK;

	switch ( query.op ) {
		case NC_ADDITION : {
			response.value = query.arg1 + query.arg2;
		} break;
		case NC_SUBTRACTION : {
			response.value = query.arg1 - query.arg2;
		} break;
		case NC_MUTIPLICATION : {
			response.value = query.arg1 * query.arg2;
		} break;
		case NC_DIVISION : {
			if ( query.arg2 == 0 ) {
				response.err = NC_DIV0;
				return;
			} else {
				response.value = query.arg1 / query.arg2;
			}
		} break;
		default : {
			response.err = NC_BADINPUT;
		} break;
	}

	if ( response.err == NC_OK ) {
		std::cout << query.arg1 << " " << getOpName( query.op ) << " " << query.arg2 << " = " << response.value << std::endl;
	} else {
		std::cerr << "Error: " << getErrText( response.err ) << std::endl;
	}
}

int main( int argc, char **argv ) {
	struct sockaddr_in si_server;
	int s;

	assert( (s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP )) > 0 );

	bzero( &si_server, sizeof( si_server ) );
	si_server.sin_family = AF_INET;
	si_server.sin_port = htons( NETCALC_UDP_PORT );
	si_server.sin_addr.s_addr = htonl( INADDR_ANY );
	assert( bind( s, (struct sockaddr *) &si_server, sizeof( si_server ) ) == 0 );

	std::cerr << "Listening for queries on UDP port " << NETCALC_UDP_PORT << "." << std::endl;

	while ( true ) {
		struct sockaddr_in si_client;
		socklen_t si_client_len = sizeof( si_client );
		nc_query_t query;

		assert( recvfrom( s, &query, sizeof( query ), 0, (struct sockaddr *) &si_client, &si_client_len ) == sizeof( query ) );
		std::cerr << "Received packet from " << inet_ntoa( si_client.sin_addr ) << ":" << ntohs( si_client.sin_port ) << std::endl;

		nc_response_t response;
		handleQuery( query, response );

		assert( sendto( s, &response, sizeof( response ), 0, (struct sockaddr *) &si_client, si_client_len ) == sizeof( response ) );
	}
}
