#include <map>
#include <iostream>
#include <cassert>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kv.h"

static std::map<std::string, std::string> store;

int main(int argc, char *argv[]) {
  short port = (short) atoi(argv[1]);

  int sockfd;
  struct sockaddr_in srvaddr, cliaddr;
  char msg[100];

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  bzero(&srvaddr, sizeof(srvaddr));
  srvaddr.sin_family = AF_INET;
  srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  srvaddr.sin_port = htons(port);
  bind(sockfd, (struct sockaddr *)&srvaddr, sizeof(srvaddr));

  std::cout << "Listening on port: " << port << std::endl;

  while (true) {
    socklen_t socklen = sizeof(cliaddr);
    ssize_t msglen = recvfrom(sockfd, msg, sizeof(msg) - 1, 0,
                              (struct sockaddr *)&cliaddr, &socklen);
    assert(socklen == sizeof(cliaddr));
    msg[msglen] = '\0';

    char cliaddrstr[INET_ADDRSTRLEN + 1];
    inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, cliaddrstr, socklen);
    std::string clistr =
        std::string(cliaddrstr) + ":" + std::to_string(ntohs(cliaddr.sin_port));

    std::string key = msg;
    std::string value;

    // Check if more than 1 string in message
    if (msglen > (signed) key.size() + 1) { // 2+ strings: set $1=$2
      value = &msg[key.size() + 1];
      store[key] = value;

      std::cout << clistr << " " << key << " = " << value << std::endl;
    } else { // 1 string: get $1
      value = store[key];

      std::cout << clistr << " " << key << " == " << value << std::endl;
    }

    char resp[key.size() + 1 + value.size() + 1];
    strcpy(resp, key.c_str());
    strcpy(&resp[key.size() + 1], value.c_str());
    sendto(sockfd, resp, key.size() + 1 + value.size() + 1, 0,
           (struct sockaddr *)&cliaddr, sizeof(cliaddr));
  }
  return 0;
}
