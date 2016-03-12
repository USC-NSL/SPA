#include <string.h>
#include <assert.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define ENABLE_SPA
#define ENABLE_KLEE
#include <spa/spaRuntime.h>

#define INITIAL_SOCKFD 10
struct {
  int type;
  int listen;
  struct sockaddr_in bind_addr;
  struct sockaddr_in connect_addr;
} sockets[20];
int next_available_sockfd = INITIAL_SOCKFD;

int spa_socket(int family, int type, int protocol) {
  if (family != AF_INET) {
    return -1;
  }

  memset(&sockets[next_available_sockfd], 0,
         sizeof(sockets[next_available_sockfd]));
  sockets[next_available_sockfd].type = type;
  // Bind default IP.
  sockets[next_available_sockfd].bind_addr.sin_family = AF_INET;
  inet_pton(AF_INET, spa_internal_defaultBindAddr,
            &sockets[next_available_sockfd].bind_addr.sin_addr);
  // Generate default port from socket number.
  sockets[next_available_sockfd].bind_addr.sin_port =
      htons(10000 + next_available_sockfd);

  printf("[model socket] Created socket: %d.\n", next_available_sockfd);

  return next_available_sockfd++;
}

int spa_bind(int sockfd, const struct sockaddr *myaddr, socklen_t addrlen) {
  memcpy(&sockets[sockfd].bind_addr, myaddr, addrlen);

  // Bind default IP if none given.
  if (sockets[sockfd].bind_addr.sin_addr.s_addr == htonl(INADDR_ANY)) {
    struct in_addr default_bind_addr;
    inet_pton(AF_INET, spa_internal_defaultBindAddr, &default_bind_addr);
    sockets[sockfd].bind_addr.sin_addr = default_bind_addr;
  }

  printf("[model bind] Bound socket: %d.\n", sockfd);

  return 0;
}

int spa_listen(int sockfd, int backlog) {
  sockets[sockfd].listen = 1;

  printf("[model listen] Listening on socket: %d.\n", sockfd);

  return 0;
}

int connect(int sockfd, const struct sockaddr *saddr, socklen_t addrlen) {
  char bind_addr[100], connect_addr[100], connect_name[100],
      connectsize_name[100];

  memcpy(&sockets[sockfd].connect_addr, saddr, addrlen);

  assert(sockets[sockfd].connect_addr.sin_family == AF_INET);
  inet_ntop(AF_INET, &sockets[sockfd].bind_addr.sin_addr.s_addr, bind_addr,
            sizeof(bind_addr));
  inet_ntop(AF_INET, &sockets[sockfd].connect_addr.sin_addr.s_addr,
            connect_addr, sizeof(connect_addr));
  spa_snprintf5(
      connect_name, sizeof(connect_name), "spa_out_msg_connect_%s.%d.%s.%s.%d",
      bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port),
      sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp", connect_addr,
      ntohs(sockets[sockfd].connect_addr.sin_port));
  spa_snprintf5(connectsize_name, sizeof(connectsize_name),
                "spa_out_msg_size_connect_%s.%d.%s.%s.%d", bind_addr,
                ntohs(sockets[sockfd].bind_addr.sin_port),
                sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                connect_addr, ntohs(sockets[sockfd].connect_addr.sin_port));

  __spa_output((void *)&sockets[sockfd].bind_addr, sizeof(struct sockaddr_in),
               sizeof(struct sockaddr_in), connect_name, connectsize_name);

  printf("[model connect] Connect socket %d with %s:%d:%s:%s:%d.\n", sockfd,
         bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port),
         sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp", connect_addr,
         ntohs(sockets[sockfd].connect_addr.sin_port));

  return 0;
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
  static char bind_addr[100], connect_name[100], init_connect_name[100];
  static uint8_t **init_connect_value = NULL;

  inet_ntop(AF_INET, &sockets[s].bind_addr.sin_addr.s_addr, bind_addr,
            sizeof(bind_addr));
  spa_snprintf5(connect_name, sizeof(connect_name),
                "spa_in_msg_connect_%s.%d.%s.%s.%d", "0.0.0.0", 0,
                sockets[s].type == SOCK_STREAM ? "tcp" : "udp", bind_addr,
                ntohs(sockets[s].bind_addr.sin_port));
  spa_snprintf5(init_connect_name, sizeof(init_connect_name),
                "spa_init_in_msg_connect_%s.%d.%s.%s.%d", "0.0.0.0", 0,
                sockets[s].type == SOCK_STREAM ? "tcp" : "udp", bind_addr,
                ntohs(sockets[s].bind_addr.sin_port));

  if (spa_check_symbol(connect_name, pathID) >= 0) {
    if (addr && addrlen) {
      assert(*addrlen >= sizeof(struct sockaddr_in));
      spa_input(addr, sizeof(struct sockaddr_in), connect_name,
                &init_connect_value, init_connect_name);
      unsigned int i;
      for (i = 0; i < *addrlen; i++) {
        ((char *)addr)[i] = klee_get_value_i32(((char *)addr)[i]);
      }
      *addrlen = sizeof(struct sockaddr_in);
    }

    // Initialize new socket.
    memset(&sockets[next_available_sockfd], 0,
           sizeof(sockets[next_available_sockfd]));
    // Keep type and bind address from parent socket.
    sockets[next_available_sockfd].type = sockets[s].type;
    memcpy(&sockets[next_available_sockfd].bind_addr, &sockets[s].bind_addr,
           sizeof(struct sockaddr_in));
    // Get connect address from received symbol.
    memcpy(&sockets[next_available_sockfd].connect_addr, addr,
           sizeof(struct sockaddr_in));

    printf("[model accept] Accept on socket %d created new socket %d.\n", s,
           next_available_sockfd);

    return next_available_sockfd++;
  } else {
    printf("[model accept] Accept on socket %d failed.\n", s);

    spa_msg_no_input_point();
    return -1;
  }
}

ssize_t send(int sockfd, const void *buffer, size_t len, int flags) {
  return (sendto(sockfd, buffer, len, flags, NULL, 0));
}

int __attribute__((used, noinline)) spa_faultmodel_none() {
  return klee_get_value_i32(0);
}

int __attribute__((used, noinline)) spa_faultmodel_symbolic() {
  uint8_t choice = 0;
  static uint8_t **initialValue = NULL;

  spa_input(&choice, sizeof(choice), "spa_in_lossMask", &initialValue,
            "spa_init_in_lossMask");

  if (choice) {
    return klee_get_value_i32(1);
  } else {
    return klee_get_value_i32(0);
  }
}

ssize_t sendto(int sockfd, const void *buffer, size_t len, int flags,
               const struct sockaddr *to, socklen_t tolen) {
  char bind_addr[100], connect_addr[100], msg_name[100], size_name[100];

  assert(to->sa_family == AF_INET);

  if (!to) {
    to = (const struct sockaddr *)&sockets[sockfd].connect_addr;
  }

  int drop = spa_faultmodel_none();

  inet_ntop(AF_INET, &sockets[sockfd].bind_addr.sin_addr.s_addr, bind_addr,
            sizeof(bind_addr));
  inet_ntop(AF_INET, &((struct sockaddr_in *)to)->sin_addr.s_addr, connect_addr,
            sizeof(connect_addr));
  spa_snprintf5(
      msg_name, sizeof(msg_name), drop ? "spa_out_msg_dropped_%s.%d.%s.%s.%d"
                                       : "spa_out_msg_%s.%d.%s.%s.%d",
      bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port),
      sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp", connect_addr,
      ntohs(((struct sockaddr_in *)to)->sin_port));
  spa_snprintf5(size_name, sizeof(size_name),
                drop ? "spa_out_msg_size_dropped_%s.%d.%s.%s.%d"
                     : "spa_out_msg_size_%s.%d.%s.%s.%d",
                bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port),
                sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                connect_addr, ntohs(((struct sockaddr_in *)to)->sin_port));

  if (sockets[sockfd].type == SOCK_STREAM) {
    size_t offset;
    for (offset = 0; offset < len; offset++) {
      __spa_output((void *)&((const char *)buffer)[offset], 1, 1, msg_name,
                   size_name);
    }
  } else {
    char src_name[100], srcsize_name[100];
    spa_snprintf5(src_name, sizeof(src_name),
                  drop ? "spa_out_msg_src_dropped_%s.%d.%s.%s.%d"
                       : "spa_out_msg_src_%s.%d.%s.%s.%d",
                  bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port),
                  sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                  connect_addr, ntohs(((struct sockaddr_in *)to)->sin_port));
    spa_snprintf5(srcsize_name, sizeof(srcsize_name),
                  drop ? "spa_out_msg_size_src_dropped_%s.%d.%s.%s.%d"
                       : "spa_out_msg_size_src_%s.%d.%s.%s.%d",
                  bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port),
                  sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                  connect_addr, ntohs(((struct sockaddr_in *)to)->sin_port));

    __spa_output((void *)buffer, len, 1500, msg_name, size_name);
    __spa_output((void *)&sockets[sockfd].bind_addr, sizeof(struct sockaddr_in),
                 sizeof(struct sockaddr_in), src_name, srcsize_name);
  }

  printf("[model sendto] %s message of size %ld on socket %d with "
         "%s:%d:%s:%s:%d.\n",
         drop ? "Dropping" : "Sending", len, sockfd, bind_addr,
         ntohs(sockets[sockfd].bind_addr.sin_port),
         sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp", connect_addr,
         ntohs(((struct sockaddr_in *)to)->sin_port));
  spa_msg_output_point();

  if (drop && sockets[sockfd].type == SOCK_STREAM) {
    return -1;
  } else {
    return len;
  }
}

ssize_t recv(int sockfd, __ptr_t buffer, size_t len, int flags) {
  return (recvfrom(sockfd, buffer, len, flags, NULL, NULL));
}

ssize_t recvfrom(int sockfd, __ptr_t buffer, size_t len, int flags,
                 struct sockaddr *src, socklen_t *srclen) {
  int64_t size = 0;

  if (sockets[sockfd].type == SOCK_STREAM) {
    static char bind_addr[100], connect_addr[100], msg_name[100],
        init_msg_name[100];
    static uint8_t **init_msg_value = NULL;

    inet_ntop(AF_INET, &sockets[sockfd].bind_addr.sin_addr.s_addr, bind_addr,
              sizeof(bind_addr));
    inet_ntop(AF_INET, &sockets[sockfd].connect_addr.sin_addr.s_addr,
              connect_addr, sizeof(connect_addr));
    spa_snprintf5(msg_name, sizeof(msg_name), "spa_in_msg_%s.%d.%s.%s.%d",
                  connect_addr, ntohs(sockets[sockfd].connect_addr.sin_port),
                  sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                  bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port));
    spa_snprintf5(
        init_msg_name, sizeof(init_msg_name), "spa_init_in_msg_%s.%d.%s.%s.%d",
        connect_addr, ntohs(sockets[sockfd].connect_addr.sin_port),
        sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp", bind_addr,
        ntohs(sockets[sockfd].bind_addr.sin_port));

    if (spa_check_symbol(msg_name, pathID) < 0) {
      printf("[model recvfrom] Receiving message on socket %d "
             "failed.\n",
             sockfd);
      spa_msg_no_input_point();
      return -1;
    }

    size_t offset;
    for (offset = 0; offset < len && spa_check_symbol(msg_name, pathID) >= 0;
         offset++) {
      spa_input(&((char *)buffer)[offset], 1, msg_name, &init_msg_value,
                init_msg_name);
      size++;
    }
    if (src && srclen) {
      assert(*srclen >= sizeof(sockets[sockfd].connect_addr));
      memcpy(src, &sockets[sockfd].connect_addr,
             sizeof(sockets[sockfd].connect_addr));
      *srclen = sizeof(struct sockaddr_in);
    }
  } else {
    static char bind_addr[100], msg_name[100], init_msg_name[100],
        size_name[100], init_size_name[100], src_name[100], init_src_name[100];
    static uint8_t **init_msg_value = NULL, **init_size_value = NULL,
                   **init_src_value = NULL;

    inet_ntop(AF_INET, &sockets[sockfd].bind_addr.sin_addr.s_addr, bind_addr,
              sizeof(bind_addr));
    spa_snprintf5(
        msg_name, sizeof(msg_name), "spa_in_msg_%s.%d.%s.%s.%d", "0.0.0.0", 0,
        sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp", bind_addr,
        ntohs(sockets[sockfd].bind_addr.sin_port));

    if (spa_check_symbol(msg_name, pathID) < 0) {
      printf("[model recvfrom] Receiving message on socket %d failed.\n",
             sockfd);
      spa_msg_no_input_point();
      return -1;
    }

    spa_snprintf5(init_msg_name, sizeof(init_msg_name),
                  "spa_init_in_msg_%s.%d.%s.%s.%d", "0.0.0.0", 0,
                  sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                  bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port));
    spa_snprintf5(size_name, sizeof(size_name),
                  "spa_in_msg_size_%s.%d.%s.%s.%d", "0.0.0.0", 0,
                  sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                  bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port));
    spa_snprintf5(init_size_name, sizeof(init_size_name),
                  "spa_init_in_msg_size_%s.%d.%s.%s.%d", "0.0.0.0", 0,
                  sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                  bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port));

    spa_input(buffer, len, msg_name, &init_msg_value, init_msg_name);
    spa_input(&size, sizeof(size), size_name, &init_size_value, init_size_name);
    spa_assume(size > 0);

    if (src && srclen) {
      spa_snprintf5(src_name, sizeof(src_name), "spa_in_msg_src_%s.%d.%s.%s.%d",
                    "0.0.0.0", 0,
                    sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                    bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port));
      spa_snprintf5(init_src_name, sizeof(init_src_name),
                    "spa_init_in_msg_src_%s.%d.%s.%s.%d", "0.0.0.0", 0,
                    sockets[sockfd].type == SOCK_STREAM ? "tcp" : "udp",
                    bind_addr, ntohs(sockets[sockfd].bind_addr.sin_port));

      assert(*srclen >= sizeof(struct sockaddr_in));
      spa_input(src, sizeof(struct sockaddr_in), src_name, &init_src_value,
                init_src_name);
      unsigned int i;
      for (i = 0; i < *srclen; i++) {
        ((char *)src)[i] = klee_get_value_i32(((char *)src)[i]);
      }
      *srclen = sizeof(struct sockaddr_in);
    }
  }

  printf("[model recvfrom] Received message on socket %d.\n", sockfd);

  spa_tag(MsgReceived, "1");
  spa_msg_input_point();

  return size;
}

void __attribute__((noinline, weak)) spa_msg_select_no_input_point() {
  // Complicated NOP to prevent inlining.
  static uint8_t i = 0;
  i++;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
  // The number of fds that won't block.
  int fd_count = 0, read_fd = -1, read_fd_distance = INT_MAX, read_tried = 0;

  int i;
  for (i = 0; i < nfds; i++) {
    // Writes and exceptions never block.
    fd_count += (writefds && FD_ISSET(i, writefds) ? 1 : 0) +
                (exceptfds && FD_ISSET(i, exceptfds) ? 1 : 0);

    // Finds available read fd which is closest in the log,.
    if (readfds && FD_ISSET(i, readfds)) {
      static char src_name[100];

      printf("[model select] Checking socket %d for read.\n", i);

      if (sockets[i].type == SOCK_STREAM && !sockets[i].listen) {
        static char bind_addr[100], connect_addr[100];
        inet_ntop(AF_INET, &sockets[i].bind_addr.sin_addr.s_addr, bind_addr,
                  sizeof(bind_addr));
        inet_ntop(AF_INET, &sockets[i].connect_addr.sin_addr.s_addr,
                  connect_addr, sizeof(connect_addr));
        spa_snprintf5(src_name, sizeof(src_name), "spa_in_msg_%s.%d.%s.%s.%d",
                      connect_addr, ntohs(sockets[i].connect_addr.sin_port),
                      "tcp", bind_addr, ntohs(sockets[i].bind_addr.sin_port));
      } else {
        static char bind_addr[100];
        inet_ntop(AF_INET, &sockets[i].bind_addr.sin_addr.s_addr, bind_addr,
                  sizeof(bind_addr));
        if (sockets[i].listen) {
          spa_snprintf5(src_name, sizeof(src_name),
                        "spa_in_msg_connect_%s.%d.%s.%s.%d", "0.0.0.0", 0,
                        sockets[i].type == SOCK_STREAM ? "tcp" : "udp",
                        bind_addr, ntohs(sockets[i].bind_addr.sin_port));
        } else {
          spa_snprintf5(src_name, sizeof(src_name), "spa_in_msg_%s.%d.%s.%s.%d",
                        "0.0.0.0", 0,
                        sockets[i].type == SOCK_STREAM ? "tcp" : "udp",
                        bind_addr, ntohs(sockets[i].bind_addr.sin_port));
        }
      }

      int distance = spa_check_symbol(src_name, pathID);
      if (distance >= 0 && distance < read_fd_distance) {
        read_fd = i;
        read_fd_distance = distance;
      }

      FD_CLR(i, readfds);
      read_tried = 1;
    }
  }

  if (read_fd >= 0) {
    FD_SET(read_fd, readfds);
    fd_count++;
    printf("[model select] Choosing socket %d for next read.\n", read_fd);
  } else {
    printf("[model select] No read socket chosen.\n");
    if (read_tried) {
      spa_msg_select_no_input_point();
      spa_msg_no_input_point();
    }
  }

  return fd_count;
}

int spa_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  if (sockfd >= INITIAL_SOCKFD && sockfd < next_available_sockfd) {
    memcpy(addr, &sockets[sockfd].connect_addr,
           *addrlen < sizeof(sockets[sockfd].connect_addr)
               ? *addrlen
               : sizeof(sockets[sockfd].connect_addr));
    *addrlen = sizeof(sockets[sockfd].connect_addr);
    return 0;
  } else {
    return -1;
  }
}
