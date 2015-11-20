#include <strings.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <list>
#include <iostream>

#include <spa/spaRuntime.h>

#include "netcalc.h"

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT 3141
#define HOST_PARAM "--host"
#define PORT_PARAM "--port"

int sock;
struct addrinfo *server;

std::list<nc_value_t> stack;

// Executes a single query on the remote server.
nc_value_t executeQuery(nc_operator_t op, nc_value_t arg1, nc_value_t arg2) {
  spa_api_input_var(op);
  spa_api_input_var(arg1);
  spa_api_input_var(arg2);

  assert(op >= 0 && op < NC_OPERATOR_END && "Invalid operator in query.");
#ifdef ENABLE_FIX
  assert((op != NC_DIVISION || arg2 != 0) && "Division by zero.");
  assert((op != NC_MODULO || arg2 != 0) && "Modulo by zero.");
#endif

  nc_query_t query;
  ssize_t size;

  query.op = op;
  if (op == NC_ADDITION && (arg1 == 1 || arg2 == 1)) {
    query.arg1 = (arg2 == 1) ? arg1 : arg2;
    size = offsetof(nc_query_t, arg2);
  } else if (op == NC_SUBTRACTION && arg2 == 1) {
    query.arg1 = arg1;
    size = offsetof(nc_query_t, arg2);
  } else {
    query.arg1 = arg1;
    query.arg2 = arg2;
    size = sizeof(query);
  }

  spa_msg_output(&query, size, sizeof(query), "query");

// Send query.
#ifndef ENABLE_KLEE
  assert(sendto(sock, &query, size, 0, server->ai_addr, server->ai_addrlen) ==
         size);
#endif
  // Get response.
  nc_response_t response;
#ifndef ENABLE_KLEE
  assert(recv(sock, &response, sizeof(response), 0) == sizeof(response));
#endif
  spa_msg_input_var(response);

  // Output operation.
  if (response.err == NC_OK) {
#ifndef ENABLE_KLEE
    std::cerr << "	" << arg1 << " " << getOpName(query.op) << " " << arg2
              << " = " << response.value << std::endl;
#endif
    spa_valid_path();
  } else {
#ifndef ENABLE_KLEE
    std::cerr << "Error: " << getErrText(response.err) << std::endl;
#endif
    //     spa_invalid_path();
  }
  assert(response.err == NC_OK && "Query failed.");

  return response.value;
}

extern "C" {
void __attribute__((used)) SpaExecuteQueryEntry() {
  spa_api_entry();

  // 	struct addrinfo hints;
  // 	bzero( &hints, sizeof( hints ) );
  // 	hints.ai_family = AF_INET;
  // 	hints.ai_socktype = SOCK_DGRAM;
  // 	hints.ai_protocol = IPPROTO_UDP;
  // 	assert( ! getaddrinfo( DEFAULT_HOST, NULL, &hints, &server ) );
  // 	((struct sockaddr_in *) server->ai_addr)->sin_port = htons(
  // NETCALC_UDP_PORT );
  // 	assert( (sock = socket( server->ai_family, server->ai_socktype,
  // server->ai_protocol )) >= 0 && "Host could not be resolved." );

  executeQuery(NC_ADDITION, 0, 0);
}
}

// Handle an RPN operation (either an operator or a number to push).
void handleRPNOp(std::string rpnOp) {
  nc_operator_t ncOp;

  // Check whether operator or number.
  if ((ncOp = getOpByName(rpnOp)) ==
      NC_OPERATOR_END) { // Not a known operator. Assume number.
    stack.push_back(atoi(rpnOp.c_str()));
  } else {
    assert(stack.size() >= 2 &&
           "Not enough data in stack to run an operation.");
    // Pop arguments.
    nc_value_t arg2 = stack.back();
    stack.pop_back();
    nc_value_t arg1 = stack.back();
    stack.pop_back();
    // Push result.
    stack.push_back(executeQuery(ncOp, arg1, arg2));
  }
}

// Output stack elements.
void dumpStack() {
  for (std::list<nc_value_t>::iterator it = stack.begin(), ie = stack.end();
       it != ie; it++) {
    if (it != stack.begin())
      std::cout << "	";
    std::cout << *it;
  }
  std::cout << std::endl;
}

// Execute a list of RPN operations and output result.
void executeRPN(std::list<std::string> rpn) {
  for (std::list<std::string>::iterator it = rpn.begin(), ie = rpn.end();
       it != ie; it++)
    handleRPNOp(*it);
  dumpStack();
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

  std::cerr << "Sending queries to " << host << ":" << port << "." << std::endl;

  struct addrinfo hints, *result;
  bzero(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  assert(!getaddrinfo(host, NULL, &hints, &result));

  for (server = result; server; server = server->ai_next) {
    ((struct sockaddr_in *)result->ai_addr)->sin_port = htons(port);
    if ((sock = socket(server->ai_family, server->ai_socktype,
                       server->ai_protocol)) < 0)
      continue;
    break;
  }
  assert(sock >= 0 && "Host could not be resolved.");

  executeRPN(args);

  freeaddrinfo(result);
}
