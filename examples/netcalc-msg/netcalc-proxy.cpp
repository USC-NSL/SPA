#include <arpa/inet.h>
#include <iostream>
#include <list>
// #include <strings.h>
// #include <assert.h>
// #include <sys/types.h>
// #include <sys/socket.h>
#include <netdb.h>

#include <spa/spaRuntime.h>

#include "netcalc.h"

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT 3141
#define HOST_PARAM "--host"
#define PORT_PARAM "--port"

#define LISTEN_PORT 3142

// Handles the query and computes the response.
void handleQuery(nc_query_t &in_query, ssize_t size, nc_query_t &out_query) {
  spa_msg_input_var(in_query);
  spa_msg_input_size(size, "in_query");

  out_query = in_query;

  // Expand implicit operators.
  if (in_query.op == NC_ADDITION && size == offsetof(nc_query_t, arg2)) {
    out_query.arg2 = 1;
  }
  if (in_query.op == NC_SUBTRACTION && size == offsetof(nc_query_t, arg2)) {
    out_query.arg1 = 0;
    out_query.arg2 = in_query.arg1;
  }

  spa_msg_output_var(out_query);
}

extern "C" {
void SpaHandleQueryEntry() {
  spa_message_handler_entry();
  nc_query_t in_query, out_query;
  handleQuery(in_query, 0, out_query);
}
}

int main(int argc, char **argv) {
  std::list<std::string> args(argv, argv + argc);

  // Remove program name.
  args.pop_front();

  const char *host = DEFAULT_HOST;
  // Check if user specified a host.
  if (args.size() > 2 && args.front() == HOST_PARAM) {
    args.pop_front();
    host = args.front().c_str();
    args.pop_front();
  }

  int port = DEFAULT_PORT;
  // Check if user specified a port.
  if (args.size() > 2 && args.front() == PORT_PARAM) {
    args.pop_front();
    port = atoi(args.front().c_str());
    args.pop_front();
  }

  struct sockaddr_in si_server;
  int srv_sock, cli_sock;
  struct addrinfo *server_info;

  assert((srv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) > 0);

  bzero(&si_server, sizeof(si_server));
  si_server.sin_family = AF_INET;
  si_server.sin_port = htons(LISTEN_PORT);
  si_server.sin_addr.s_addr = htonl(INADDR_ANY);
  assert(bind(srv_sock, (struct sockaddr *)&si_server, sizeof(si_server)) == 0);
  std::cerr << "Listening for queries on UDP port " << LISTEN_PORT << "."
            << std::endl;

  struct addrinfo hints, *result;
  bzero(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  assert(!getaddrinfo(host, NULL, &hints, &result));
  for (server_info = result; server_info; server_info = server_info->ai_next) {
    ((struct sockaddr_in *)result->ai_addr)->sin_port = htons(port);
    if ((cli_sock = socket(server_info->ai_family, server_info->ai_socktype,
                           server_info->ai_protocol)) < 0)
      continue;
    break;
  }
  assert(cli_sock >= 0 && "Host could not be resolved.");
  std::cerr << "Forwarding queries to " << host << ":" << port << "."
            << std::endl;

  // Handle queries indefinitely.
  while (true) {
    struct sockaddr_in si_client;
    socklen_t si_client_len = sizeof(si_client);
    nc_query_t in_query;

    ssize_t size = recvfrom(srv_sock, &in_query, sizeof(in_query), 0,
                            (struct sockaddr *)&si_client, &si_client_len);
    assert(size >= 0);
    std::cerr << "Received packet from " << inet_ntoa(si_client.sin_addr) << ":"
              << ntohs(si_client.sin_port) << std::endl;

    nc_query_t out_query;
    handleQuery(in_query, size, out_query);

    std::cerr << "Sending transformed packet to server." << std::endl;
    assert(sendto(cli_sock, &out_query, sizeof(out_query), 0,
                  server_info->ai_addr, server_info->ai_addrlen) ==
           sizeof(out_query));

    struct sockaddr_in si_server;
    socklen_t si_server_len = sizeof(si_server);
    nc_response_t response;

    assert(recvfrom(cli_sock, &response, sizeof(response), 0,
                    (struct sockaddr *)&si_server, &si_server_len) ==
           sizeof(response));
    std::cerr << "Received packet from " << inet_ntoa(si_server.sin_addr) << ":"
              << ntohs(si_server.sin_port) << std::endl;

    std::cerr << "Sending response back to client." << std::endl;
    assert(sendto(cli_sock, &response, sizeof(response), 0,
                  (struct sockaddr *)&si_client, si_client_len) ==
           sizeof(response));

    spa_done();
  }

  freeaddrinfo(result);
}
