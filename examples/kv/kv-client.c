#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kv.h"

#define R 2
#define W 2

struct {
  const char *ip;
  short port;
} servers[] = { { "127.0.0.1", 12340 }, { "127.0.0.1", 12341 },
                { "127.0.0.1", 12342 } };

struct consensus_t;
typedef struct consensus_t {
  char *value;
  int count;
  struct consensus_t *next;
} consensus_t;
consensus_t *consensus = NULL;

void clearValues() {
  while (consensus) {
    consensus_t *temp = consensus;
    consensus = consensus->next;
    free(temp->value);
    free(temp);
  }
}

int addValue(char *value) {
  consensus_t *it;
  for (it = consensus; it && strcmp(value, it->value) != 0; it = it->next) {
  }
  if (it) {
    return ++it->count;
  } else {
    consensus_t *newValue = malloc(sizeof(consensus_t));
    newValue->value = malloc(strlen(value) + 1);
    strcpy(newValue->value, value);
    newValue->count = 1;
    newValue->next = consensus;
    consensus = newValue;
    return 1;
  }
}

char *kv_get(char *key) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  for (int i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    struct sockaddr_in srvaddr;
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, servers[i].ip, &srvaddr.sin_addr);
    srvaddr.sin_port = htons(servers[i].port);

    sendto(sockfd, key, strlen(key) + 1, 0, (struct sockaddr *)&srvaddr,
           sizeof(srvaddr));
  }

#ifndef ENABLE_KLEE
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
             sizeof(struct timeval));
#endif

  for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    char resp[100];
    struct sockaddr_in srvaddr;
    socklen_t socklen = sizeof(srvaddr);
    ssize_t resplen = recvfrom(sockfd, resp, sizeof(resp) - 1, 0,
                               (struct sockaddr *)&srvaddr, &socklen);
    assert(resplen >= 0 && "Insufficient responses.");
    assert(socklen == sizeof(srvaddr));
    resp[resplen] = '\0';

    char *srv_key = resp;
    int pos = strlen(srv_key) + 1;
    assert(resplen > pos);
    char *srv_value = &resp[pos];

#ifndef ENABLE_KLEE
    char srvstr[INET_ADDRSTRLEN + 1 + 5 + 1];
    inet_ntop(AF_INET, &srvaddr.sin_addr.s_addr, srvstr, socklen);
    pos = strlen(srvstr);
    srvstr[pos++] = ':';
    snprintf(&srvstr[pos], 6, "%d", ntohs(srvaddr.sin_port));

    printf("%s %s == %s\n", srvstr, srv_key, srv_value);
#endif

    assert(strcmp(key, srv_key) == 0);

    if (addValue(srv_value) >= R) {
      clearValues();
      return srv_value;
    }
  }

  assert(0 && "No quorum.");
}

void kv_set(char *key, char *value) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    struct sockaddr_in srvaddr;
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, servers[i].ip, &srvaddr.sin_addr.s_addr);
    srvaddr.sin_port = htons(servers[i].port);

    int key_len = strlen(key);
    int value_len = strlen(value);
    char msg[key_len + 1 + value_len + 1];
    strcpy(msg, key);
    strcpy(&msg[strlen(key) + 1], value);
    sendto(sockfd, msg, key_len + 1 + value_len + 1, 0,
           (struct sockaddr *)&srvaddr, sizeof(srvaddr));
  }

#ifndef ENABLE_KLEE
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
             sizeof(struct timeval));
#endif

  for (unsigned i = 0; i < W; i++) {
    char resp[100];
    struct sockaddr_in srvaddr;
    socklen_t socklen = sizeof(srvaddr);
    ssize_t resplen = recvfrom(sockfd, resp, sizeof(resp) - 1, 0,
                               (struct sockaddr *)&srvaddr, &socklen);
    assert(resplen >= 0 && "Insufficient responses.");
    assert(socklen == sizeof(srvaddr));
    resp[resplen] = '\0';

    char *srv_key = resp;
    int pos = strlen(srv_key) + 1;
    assert(resplen > pos);
    char *srv_value = &resp[pos];

#ifndef ENABLE_KLEE
    char srvstr[INET_ADDRSTRLEN + 1 + 5 + 1];
    inet_ntop(AF_INET, &srvaddr.sin_addr.s_addr, srvstr, socklen);
    pos = strlen(srvstr);
    srvstr[pos++] = ':';
    snprintf(&srvstr[pos], 6, "%d", ntohs(srvaddr.sin_port));

    printf("%s %s = %s\n", srvstr, srv_key, srv_value);
#endif

    assert(strcmp(key, srv_key) == 0);
    assert(strcmp(value, srv_value) == 0);
  }
}
