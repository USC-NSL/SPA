#include <string.h>
#include <assert.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define ENABLE_SPA
#define ENABLE_KLEE
#include <spa/spaRuntime.h>

struct {
  int type;
  int listen;
  struct sockaddr_in bind_addr;
  struct sockaddr_in connect_addr;
} sockets[20];
int next_available_sockfd = 10;

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
  return 0;
}

int connect(int sockfd, const struct sockaddr *saddr, socklen_t addrlen) {
  char addr[100], src_name[100], srcsize_name[100];

  memcpy(&sockets[sockfd].connect_addr, saddr, addrlen);

  assert(sockets[sockfd].connect_addr.sin_family == AF_INET);
  inet_ntop(AF_INET, &sockets[sockfd].connect_addr.sin_addr.s_addr, addr,
            sizeof(addr));
  spa_snprintf3(src_name, sizeof(src_name), "%s_%s.%d", "spa_out_msg_connect",
                addr, ntohs(sockets[sockfd].connect_addr.sin_port));
  spa_snprintf3(srcsize_name, sizeof(srcsize_name), "%s_%s.%d",
                "spa_out_msg_size_connect", addr,
                ntohs(sockets[sockfd].connect_addr.sin_port));

  __spa_output((void *)&sockets[sockfd].bind_addr, sizeof(struct sockaddr_in),
               sizeof(struct sockaddr_in), src_name, srcsize_name);

  printf("[model connect] Connect socket %d to %s:%d.\n", sockfd, addr,
         ntohs(sockets[sockfd].connect_addr.sin_port));

  return 0;
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
  static char addr_str[100], src_name[100], init_src_name[100];
  static uint8_t **init_src_value = NULL;

  inet_ntop(AF_INET, &sockets[s].bind_addr.sin_addr.s_addr, addr_str,
            sizeof(addr_str));
  spa_snprintf3(src_name, sizeof(src_name), "%s_%s.%d", "spa_in_msg_connect",
                addr_str, ntohs(sockets[s].bind_addr.sin_port));
  spa_snprintf3(init_src_name, sizeof(init_src_name), "%s_%s.%d",
                "spa_init_in_msg_connect", addr_str,
                ntohs(sockets[s].bind_addr.sin_port));

  if (spa_check_symbol(src_name, pathID) >= 0) {
    if (addr && addrlen) {
      assert(*addrlen >= sizeof(struct sockaddr_in));
      spa_input(addr, sizeof(struct sockaddr_in), src_name, &init_src_value,
                init_src_name);
      for (unsigned int i = 0; i < *addrlen; i++) {
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

ssize_t sendto(int sockfd, const void *buffer, size_t len, int flags,
               const struct sockaddr *to, socklen_t tolen) {
  char addr[100], msg_name[100], size_name[100], src_name[100],
      srcsize_name[100];

  if (!to) {
    to = (const struct sockaddr *)&sockets[sockfd].connect_addr;
  }

  assert(to->sa_family == AF_INET);
  inet_ntop(AF_INET, &((struct sockaddr_in *)to)->sin_addr.s_addr, addr,
            sizeof(addr));
  spa_snprintf3(msg_name, sizeof(msg_name), "%s_%s.%d", "spa_out_msg", addr,
                ntohs(((struct sockaddr_in *)to)->sin_port));
  spa_snprintf3(size_name, sizeof(size_name), "%s_%s.%d", "spa_out_msg_size",
                addr, ntohs(((struct sockaddr_in *)to)->sin_port));
  spa_snprintf3(src_name, sizeof(src_name), "%s_%s.%d", "spa_out_msg_src", addr,
                ntohs(((struct sockaddr_in *)to)->sin_port));
  spa_snprintf3(srcsize_name, sizeof(srcsize_name), "%s_%s.%d",
                "spa_out_msg_size_src", addr,
                ntohs(((struct sockaddr_in *)to)->sin_port));

  __spa_output((void *)buffer, len, 1500, msg_name, size_name);
  spa_runtime_call(spa_msg_output_handler, buffer, len, 1500, msg_name);

  __spa_output((void *)&sockets[sockfd].bind_addr, sizeof(struct sockaddr_in),
               sizeof(struct sockaddr_in), src_name, srcsize_name);

  printf("[model sendto] Sending message on socket %d to %s:%d.\n", sockfd,
         addr, ntohs(((struct sockaddr_in *)to)->sin_port));

  spa_msg_output_point();

  return len;
}

ssize_t recv(int sockfd, __ptr_t buffer, size_t len, int flags) {
  return (recvfrom(sockfd, buffer, len, flags, NULL, NULL));
}

ssize_t recvfrom(int sockfd, __ptr_t buffer, size_t len, int flags,
                 struct sockaddr *src, socklen_t *srclen) {
  static char addr[100], msg_name[100], init_msg_name[100], size_name[100],
      init_size_name[100], src_name[100], init_src_name[100];
  static uint8_t **init_msg_value = NULL, **init_size_value = NULL,
                 **init_src_value = NULL;
  int64_t size = 0;

  inet_ntop(AF_INET, &sockets[sockfd].bind_addr.sin_addr.s_addr, addr,
            sizeof(addr));
  spa_snprintf3(msg_name, sizeof(msg_name), "%s_%s.%d", "spa_in_msg", addr,
                ntohs(sockets[sockfd].bind_addr.sin_port));
  spa_snprintf3(init_msg_name, sizeof(init_msg_name), "%s_%s.%d",
                "spa_init_in_msg", addr,
                ntohs(sockets[sockfd].bind_addr.sin_port));
  spa_snprintf3(size_name, sizeof(size_name), "%s_%s.%d", "spa_in_msg_size",
                addr, ntohs(sockets[sockfd].bind_addr.sin_port));
  spa_snprintf3(init_size_name, sizeof(init_size_name), "%s_%s.%d",
                "spa_init_in_msg_size", addr,
                ntohs(sockets[sockfd].bind_addr.sin_port));
  spa_snprintf3(src_name, sizeof(src_name), "%s_%s.%d", "spa_in_msg_src", addr,
                ntohs(sockets[sockfd].bind_addr.sin_port));
  spa_snprintf3(init_src_name, sizeof(init_src_name), "%s_%s.%d",
                "spa_init_in_msg_src", addr,
                ntohs(sockets[sockfd].bind_addr.sin_port));

  if (spa_check_symbol(msg_name, pathID) >= 0) {
    spa_input(buffer, len, msg_name, &init_msg_value, init_msg_name);
    spa_runtime_call(spa_msg_input_handler, buffer, len, msg_name);

    spa_input(&size, sizeof(size), size_name, &init_size_value, init_size_name);
    spa_runtime_call(spa_msg_input_size_handler, &size, sizeof(size),
                     size_name);
    spa_assume(size > 0);

    if (src && srclen) {
      assert(*srclen >= sizeof(struct sockaddr_in));
      spa_input(src, sizeof(struct sockaddr_in), src_name, &init_src_value,
                init_src_name);
      for (unsigned int i = 0; i < *srclen; i++) {
        ((char *)src)[i] = klee_get_value_i32(((char *)src)[i]);
      }
      *srclen = sizeof(struct sockaddr_in);
    }

    printf("[model recvfrom] Received message on socket %d.\n", sockfd);

    spa_tag(MsgReceived, "1");
    spa_msg_input_point();

    return size;
  } else {
    printf("[model recvfrom] Receiving message on socket %d failed.\n", sockfd);
    spa_msg_no_input_point();
    return -1;
  }
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
  // The number of fds that won't block.
  int fd_count = 0, read_fd = -1, read_fd_distance = INT_MAX, read_tried = 0;

  for (int i = 0; i < nfds; i++) {
    // Writes and exceptions never block.
    fd_count += (writefds && FD_ISSET(i, writefds) ? 1 : 0) +
                (exceptfds && FD_ISSET(i, exceptfds) ? 1 : 0);

    // Finds available read fd which is closest in the log,.
    if (readfds && FD_ISSET(i, readfds)) {
      static char addr[100], src_name[100];

      printf("[model select] Checking socket %d for read.\n", i);

      inet_ntop(AF_INET, &sockets[i].bind_addr.sin_addr.s_addr, addr,
                sizeof(addr));
      spa_snprintf3(src_name, sizeof(src_name), "%s_%s.%d",
                    sockets[i].listen ? "spa_in_msg_connect" : "spa_in_msg",
                    addr, ntohs(sockets[i].bind_addr.sin_port));

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
      spa_msg_no_input_point();
    }
  }

  return fd_count;
}
