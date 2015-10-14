#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kv.h"

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

#ifndef ENABLE_KLEE
  printf("Listening on port: %d\n", port);
#endif

  for (;;) {
    socklen_t socklen = sizeof(cliaddr);
    ssize_t msglen = recvfrom(sockfd, msg, sizeof(msg) - 1, 0,
                              (struct sockaddr *)&cliaddr, &socklen);
    assert(socklen == sizeof(cliaddr));
    msg[msglen] = '\0';

#ifndef ENABLE_KLEE
    char clistr[INET_ADDRSTRLEN + 1 + 5 + 1];
    inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, clistr, socklen);
    int pos = strlen(clistr);
    clistr[pos++] = ':';
    snprintf(&clistr[pos], 6, "%d", ntohs(cliaddr.sin_port));
#endif

    char *key = msg;
    char *value;

    // Check if more than 1 string in message
    int key_len = strlen(key);
    assert(msglen >= key_len + 1);
    if (msglen > key_len + 1) { // 2+ strings: set $1=$2
      value = &msg[key_len + 1];
      kv_set(key, value);

#ifndef ENABLE_KLEE
      printf("%s %s = %s\n", clistr, key, value);
#endif
    } else { // 1 string: get $1
      value = kv_get(key);

#ifndef ENABLE_KLEE
      printf("%s %s == %s", clistr, key, value);
#endif
    }

    int value_len = strlen(value);
    char resp[key_len + 1 + value_len + 1];
    strcpy(resp, key);
    strcpy(&resp[key_len + 1], value);
    sendto(sockfd, resp, key_len + 1 + value_len + 1, 0,
           (struct sockaddr *)&cliaddr, sizeof(cliaddr));
  }
  return 0;
}
