#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kv.h"

#define R 2
#define W 2

#ifdef ENABLE_KLEE
#undef FD_ZERO
#define FD_ZERO(fdset) memset(fdset, 0, sizeof(fd_set))
#endif

struct {
  const char *ip;
  short port;
} servers[] = { { "127.0.0.2", 12340 }, { "127.0.0.3", 12341 },
                { "127.0.0.4", 12342 } };

struct consensus_t;
typedef struct consensus_t {
  value_t value;
  int count;
  struct consensus_t *next;
} consensus_t;
consensus_t *consensus = NULL;

void clearValues() {
  while (consensus) {
    consensus_t *temp = consensus;
    consensus = consensus->next;
    free(temp);
  }
}

int addValue(value_t value) {
  consensus_t *it;
  for (it = consensus; it && value != it->value; it = it->next) {
  }
  if (it) {
    return ++it->count;
  } else {
    consensus_t *newValue = malloc(sizeof(consensus_t));
    newValue->value = value;
    newValue->count = 1;
    newValue->next = consensus;
    consensus = newValue;
    return 1;
  }
}

value_t kv_get(key_t key) {
  int sockfd[sizeof(servers) / sizeof(servers[0])];
  fd_set set;
  int max_fd = -1;

  FD_ZERO(&set);
  for (int i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    assert((sockfd[i] = socket(AF_INET, SOCK_STREAM, 0)) >= 0);

    struct sockaddr_in srvaddr;
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, servers[i].ip, &srvaddr.sin_addr);
    srvaddr.sin_port = htons(servers[i].port);

    if (connect(sockfd[i], (struct sockaddr *)&srvaddr, sizeof(srvaddr)) == 0) {
      FD_SET(sockfd[i], &set);
      if (sockfd[i] > max_fd) {
        max_fd = sockfd[i];
      }
    } else {
#ifndef ENABLE_KLEE
      assert(close(sockfd[i]) == 0);
#endif
      sockfd[i] = -1;
    }
  }

  struct timeval tv = { 0, 0 };
  assert(select(max_fd + 1, NULL, &set, NULL, &tv) >= 0);
  for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    if (sockfd[i] >= 0 && FD_ISSET(sockfd[i], &set)) {
      send(sockfd[i], &key, sizeof(key_t), 0);
    }
  }

  for (;;) {
    int selected_server = -1;
    FD_ZERO(&set);
    int num_sockfds = 0;
    for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
      if (sockfd[i] >= 0) {
        FD_SET(sockfd[i], &set);
        num_sockfds++;
      }
    }
    assert(num_sockfds > 0 && "Not enough available servers.");

    struct timeval tv = { 1, 0 };
    assert(select(max_fd + 1, &set, NULL, NULL, &tv) >= 0);

    for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
      if (sockfd[i] >= 0 && FD_ISSET(sockfd[i], &set)) {
        selected_server = i;
        break;
      }
    }
    assert(selected_server >= 0);

    char resp[sizeof(key_t) + sizeof(value_t)];
    ssize_t resplen = recv(sockfd[selected_server], resp, sizeof(resp), 0);

#ifndef ENABLE_KLEE
    assert(close(sockfd[selected_server]) == 0);
#endif
    sockfd[selected_server] = -1;

    assert(resplen == sizeof(resp));
    key_t srv_key = *((key_t *)&resp[0]);
    value_t srv_value = *((value_t *)&resp[sizeof(key_t)]);

#ifndef ENABLE_KLEE
    printf("%s:%d %c == %c\n", servers[selected_server].ip,
           servers[selected_server].port, srv_key, srv_value);
#endif

    assert(key == srv_key);

    if (addValue(srv_value) >= R) {
      clearValues();
      return srv_value;
    }
  }

  assert(0 && "No quorum.");
}

void kv_set(key_t key, value_t value) {
  int sockfd[sizeof(servers) / sizeof(servers[0])];
  fd_set set;
  int max_fd = -1;

  FD_ZERO(&set);
  for (int i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    assert((sockfd[i] = socket(AF_INET, SOCK_STREAM, 0)) >= 0);

    struct sockaddr_in srvaddr;
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, servers[i].ip, &srvaddr.sin_addr);
    srvaddr.sin_port = htons(servers[i].port);

    if (connect(sockfd[i], (struct sockaddr *)&srvaddr, sizeof(srvaddr)) == 0) {
      FD_SET(sockfd[i], &set);
      if (sockfd[i] > max_fd) {
        max_fd = sockfd[i];
      }
    } else {
#ifndef ENABLE_KLEE
      assert(close(sockfd[i]) == 0);
#endif
      sockfd[i] = -1;
    }
  }
  assert(max_fd >= 0 && "No available servers.");

  struct timeval tv = { 1, 0 };
  assert(select(max_fd + 1, NULL, &set, NULL, &tv) >= 0);
  for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
    if (sockfd[i] >= 0 && FD_ISSET(sockfd[i], &set)) {
      char msg[sizeof(key_t) + sizeof(value_t)];
      *((key_t *)&msg[0]) = key;
      *((value_t *)&msg[sizeof(key_t)]) = value;
      send(sockfd[i], msg, sizeof(msg), 0);
    }
  }

  for (unsigned i = 0; i < W; i++) {
    int selected_server = -1;
    //     do {
    FD_ZERO(&set);
    int num_sockfds = 0;
    for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
      if (sockfd[i] >= 0) {
        FD_SET(sockfd[i], &set);
        num_sockfds++;
      }
    }
    assert(num_sockfds > 0 && "Not enough available servers.");

    struct timeval tv = { 1, 0 };
    assert(select(max_fd + 1, &set, NULL, NULL, &tv) >= 0);

    for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++) {
      if (sockfd[i] >= 0 && FD_ISSET(sockfd[i], &set)) {
        selected_server = i;
        break;
      }
    }
    //     } while (selected_server < 0);
    assert(selected_server >= 0);

    char resp[sizeof(key_t) + sizeof(value_t)];
    ssize_t resplen = recv(sockfd[selected_server], resp, sizeof(resp), 0);
#ifndef ENABLE_KLEE
    assert(close(sockfd[selected_server]) == 0);
#endif
    sockfd[selected_server] = -1;

    assert(resplen == sizeof(resp));
    key_t srv_key = *((key_t *)&resp[0]);
    value_t srv_value = *((value_t *)&resp[sizeof(key_t)]);

#ifndef ENABLE_KLEE
    printf("%s:%d %c = %c\n", servers[selected_server].ip,
           servers[selected_server].port, srv_key, srv_value);
#endif

    assert(key == srv_key);
    assert(value == srv_value);
  }
}
