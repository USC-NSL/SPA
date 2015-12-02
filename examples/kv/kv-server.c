#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kv.h"

void runServer(short port) {
  int sockfd;
  struct sockaddr_in srvaddr, cliaddr;
  char msg[sizeof(key_t) + sizeof(value_t)];

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
    ssize_t msglen = recvfrom(sockfd, msg, sizeof(msg), 0,
                              (struct sockaddr *)&cliaddr, &socklen);
    assert(socklen == sizeof(cliaddr));

#ifndef ENABLE_KLEE
    char clistr[INET_ADDRSTRLEN + 1 + 5 + 1];
    inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, clistr, socklen);
    int pos = strlen(clistr);
    clistr[pos++] = ':';
    snprintf(&clistr[pos], 6, "%d", ntohs(cliaddr.sin_port));
#endif

    key_t key = *((key_t *)&msg[0]);
    value_t value;

    if (msglen == sizeof(key_t)) { // Only key present: get $1
      value = kv_get(key);

#ifndef ENABLE_KLEE
      printf("%s %c == %c", clistr, key, value);
#endif
    } else if (msglen == sizeof(key_t) +
               sizeof(value_t)) { // key+value present: set $1=$2
      value = *((value_t *)&msg[sizeof(key_t)]);
      kv_set(key, value);

#ifndef ENABLE_KLEE
      printf("%s %c = %c\n", clistr, key, value);
#endif
    } else {
      assert(0 && "Bad message size.");
    }

    char resp[sizeof(key_t) + sizeof(value_t)];
    *((key_t *)&resp[0]) = key;
    *((value_t *)&resp[sizeof(key_t)]) = value;
    sendto(sockfd, resp, sizeof(resp), 0, (struct sockaddr *)&cliaddr,
           sizeof(cliaddr));
  }
}

int main(int argc, char *argv[]) {
  runServer(atoi(argv[1]));
  return 0;
}

void __attribute__((noinline, used)) spa_entry1() { runServer(12340); }

void __attribute__((noinline, used)) spa_entry2() { runServer(12341); }

void __attribute__((noinline, used)) spa_entry3() { runServer(12342); }
