#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <strings.h>

#include <iostream>
#include <limits>

#include <spa/spaRuntime.h>

#include "netcalc.h"

SpaTag_t QueryValid;

// Handles the query and computes the response.
void handleQuery( nc_query_t &query, ssize_t size, nc_response_t &response ) {
	spa_msg_input_var( query );
  spa_msg_input_size(size, "query");
// 	spa_seed_file( 1, &query, "query.seed" );
// 	spa_seed( 1, &query, sizeof( query ), "\0\0\0\0\171\171\171\171\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" );

	response.err = NC_OK;

	// Check operator.
	switch ( query.op ) {
		case NC_ADDITION : {
      if (size == sizeof(query)) {
        response.value = query.arg1 + query.arg2;
      } else if (size == offsetof(nc_query_t, arg2)) {
        response.value = query.arg1 + 1;
      } else {
        response.err = NC_BADINPUT;
      }
		} break;
		case NC_SUBTRACTION : {
      if (size == sizeof(query)) {
        response.value = query.arg1 - query.arg2;
      } else if (size == offsetof(nc_query_t, arg2)) {
        response.value = - query.arg1;
      } else {
        response.err = NC_BADINPUT;
      }
		} break;
		case NC_MUTIPLICATION : {
      if (size == sizeof(query)) {
        response.value = query.arg1 * query.arg2;
      } else {
        response.err = NC_BADINPUT;
      }
		} break;
		case NC_DIVISION : {
      if (size != sizeof(query)) {
        response.err = NC_BADINPUT;
      } else if ( query.arg2 == 0 ) {
				response.err = NC_DIV0;
			} else {
				response.value = query.arg1 / query.arg2;
			}
		} break;
// 		case NC_MODULO : {
//       if (size != sizeof(query)) {
//         response.err = NC_BADINPUT;
//       } else if ( query.arg2 == 0 ) {
// 				response.err = NC_DIV0;
// 			} else {
// 				response.value = query.arg1 % query.arg2;
// 			}
// 		} break;
		default : { // Unknown operator.
			response.err = NC_BADINPUT;
		} break;
	}

	// Output the operation.
// 	if ( response.err == NC_OK ) {
// 		std::cout << query.arg1 << " " << getOpName( query.op ) << " " << query.arg2 << " = " << response.value << std::endl;
// 	} else {
// 		std::cerr << "Error: " << getErrText( response.err ) << std::endl;
// 	}

 if ( response.err == NC_OK ) {
// 	if ( response.err != NC_BADINPUT ) {
		spa_valid_path();
// 	} else {
// 		spa_invalid_path();
	}
	spa_msg_output_var( response );
}

void SpaHandleQueryEntry() {
	spa_message_handler_entry();
	nc_query_t query;
  ssize_t size = 0;
	nc_response_t response;
	handleQuery( query, size, response );
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

	// Handle queries indefinitely.
	while ( true ) {
		struct sockaddr_in si_client;
		socklen_t si_client_len = sizeof( si_client );
		nc_query_t query;

		ssize_t size = recvfrom(s, &query, sizeof(query), 0, (struct sockaddr *) &si_client, &si_client_len);
		assert(size >= 0);
		std::cerr << "Received packet from " << inet_ntoa( si_client.sin_addr ) << ":" << ntohs( si_client.sin_port ) << std::endl;

		nc_response_t response;
		handleQuery( query, size, response );

		assert( sendto( s, &response, sizeof( response ), 0, (struct sockaddr *) &si_client, si_client_len ) == sizeof( response ) );
	}
}
