#include <arpa/inet.h>
#include <iostream>

#include <spa/spaRuntime.h>

#define MAX_MESSAGE_SIZE 1500

#define STATUS_NAME "status"

uint64_t getBits(char *buf, int offset, int width) {
  uint64_t result = 0;

  buf += offset >> 3;
  offset &= 0x7;

  if (offset) {
    result = buf[0] & ((1 << (8 - offset)) - 1);
    buf++;
    width -= 8 - offset;
  }

  while (width > 0) {
    result = result << 8 | *buf;
    buf++;
    width -= 8;
  }

  if (width) {
    result >>= (-width);
  }

  return result;
}

void getSpdyResponseStatus(char *msg, ssize_t size) {
  spa_msg_input(msg, MAX_MESSAGE_SIZE, "message");
  spa_msg_input_size(size, "message");

  assert(getBits(msg, 0, 1) && "Not a control frame.");

  switch (getBits(msg, 1, 15)) {
  case 2:
  case 3: {
    assert(getBits(msg, 16, 16) == 2 && "Not a SYN_REPLY frame.");
    char *data = msg + 16;
    for (int num_nv = getBits(msg, 112, 16); num_nv > 0; num_nv--) {
      int name_len = getBits(data, 0, 16);
      data += 2;
      char *name = data;
      data += name_len;
      int value_len = getBits(data, 0, 16);
      data += 2;
      char *value = data;
      data += value_len;

      if (strncmp(STATUS_NAME, name, strlen(STATUS_NAME)) == 0) {
        value += 0;
        spa_msg_output(value, 3, 3, "status");
        printf("Status code: %3s", value);
        break;
      }
    }
  } break;
  default:
    assert(false && "Unknown SPDY version.");
  }
}

extern "C" {
void Entry() {
  spa_message_handler_entry();
  char msg[MAX_MESSAGE_SIZE];
  getSpdyResponseStatus(msg, 0);
}
}

int main(int argc, char **argv) {
  assert(argc == 2 && "Must specify listen port.");
  int port = atoi(argv[1]);

  struct sockaddr_in si_server;
  int s;

  assert((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) > 0);

  bzero(&si_server, sizeof(si_server));
  si_server.sin_family = AF_INET;
  si_server.sin_port = htons(port);
  si_server.sin_addr.s_addr = htonl(INADDR_ANY);
  assert(bind(s, (struct sockaddr *)&si_server, sizeof(si_server)) == 0);

  std::cerr << "Listening on UDP port " << port << "." << std::endl;

  struct sockaddr_in si_client;
  socklen_t si_client_len = sizeof(si_client);
  char msg[MAX_MESSAGE_SIZE];

  ssize_t size = recvfrom(s, msg, MAX_MESSAGE_SIZE, 0,
                          (struct sockaddr *)&si_client, &si_client_len);
  assert(size >= 0);
  std::cerr << "Received message from " << inet_ntoa(si_client.sin_addr) << ":"
            << ntohs(si_client.sin_port) << std::endl;

  getSpdyResponseStatus(msg, size);
}
