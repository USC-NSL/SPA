#include <map>
#include <iostream>
#include <cassert>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kv.h"

#define R 2
#define W 2

std::string servers[] = { "127.0.0.1:12340", "127.0.0.1:12341",
                                     "127.0.0.1:12342" };

std::string kv_get(std::string key) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  for (std::string server : servers) {
    struct sockaddr_in srvaddr;
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, server.substr(0, server.find(":")).c_str(),
              &srvaddr.sin_addr.s_addr);
    srvaddr.sin_port =
        htons((short) atol(server.substr(server.find(":") + 1).c_str()));

    sendto(sockfd, key.c_str(), key.size() + 1, 0, (struct sockaddr *)&srvaddr,
           sizeof(srvaddr));
  }

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
             sizeof(struct timeval));

  std::map<std::string, int> values;
  for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    char resp[100];
    struct sockaddr_in srvaddr;
    socklen_t socklen = sizeof(srvaddr);
    ssize_t resplen = recvfrom(sockfd, resp, sizeof(resp) - 1, 0,
                               (struct sockaddr *)&srvaddr, &socklen);
    assert(resplen >= 0 && "Insufficient responses.");
    assert(socklen == sizeof(srvaddr));
    resp[resplen] = '\0';

    char srvaddrstr[INET_ADDRSTRLEN + 1];
    inet_ntop(AF_INET, &srvaddr.sin_addr.s_addr, srvaddrstr, socklen);
    std::string srvstr =
        std::string(srvaddrstr) + ":" + std::to_string(ntohs(srvaddr.sin_port));

    std::string srv_key = resp;
    assert(resplen > (signed) key.size() + 1);
    std::string srv_value = &resp[key.size() + 1];

    std::cout << srvaddrstr << " " << srv_key << " == " << srv_value
              << std::endl;

    assert(key == srv_key);
    values[srv_value]++;

    if (values[srv_value] >= R) {
      return srv_value;
    }
  }

  assert(false && "No quorum.");
}

void kv_set(std::string key, std::string value) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  for (std::string server : servers) {
    struct sockaddr_in srvaddr;
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, server.substr(0, server.find(":")).c_str(),
              &srvaddr.sin_addr.s_addr);
    srvaddr.sin_port =
        htons((short) atol(server.substr(server.find(":") + 1).c_str()));

    char msg[key.size() + 1 + value.size() + 1];
    strcpy(msg, key.c_str());
    strcpy(&msg[key.size() + 1], value.c_str());
    sendto(sockfd, msg, key.size() + 1 + value.size() + 1, 0,
           (struct sockaddr *)&srvaddr, sizeof(srvaddr));
  }

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
             sizeof(struct timeval));

  for (unsigned i = 0; i < W; i++) {
    char resp[100];
    struct sockaddr_in srvaddr;
    socklen_t socklen = sizeof(srvaddr);
    ssize_t resplen = recvfrom(sockfd, resp, sizeof(resp) - 1, 0,
                               (struct sockaddr *)&srvaddr, &socklen);
    assert(resplen >= 0 && "Insufficient responses.");
    assert(socklen == sizeof(srvaddr));
    resp[resplen] = '\0';

    char srvaddrstr[INET_ADDRSTRLEN + 1];
    inet_ntop(AF_INET, &srvaddr.sin_addr.s_addr, srvaddrstr, socklen);
    std::string srvstr =
        std::string(srvaddrstr) + ":" + std::to_string(ntohs(srvaddr.sin_port));

    std::string srv_key = resp;
    assert(resplen > (signed) key.size() + 1);
    std::string srv_value = &resp[key.size() + 1];

    std::cout << srvstr << " " << srv_key << " = " << srv_value << std::endl;

    assert(key == srv_key);
    assert(value == srv_value);
  }
}
