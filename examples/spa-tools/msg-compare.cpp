#include <arpa/inet.h>
#include <iostream>

#include <spa/spaRuntime.h>

#define MAX_MESSAGE_SIZE 1500

SpaTag_t MessagesEqual;

extern "C" {
void done();
void MsgsEqual();
void MsgsDifferent();
}

void compareMsgs(char *msg1, ssize_t size1, char *msg2, ssize_t size2) {
  spa_msg_input(msg1, MAX_MESSAGE_SIZE, "message1");
  spa_msg_input_size(size1, "message1");
  spa_msg_input(msg2, MAX_MESSAGE_SIZE, "message2");
  spa_msg_input_size(size2, "message2");

  if (size1 == size2 && memcmp(msg1, msg2, size1) == 0) {
    spa_tag(MessagesEqual, "1");
    MsgsEqual();
  } else {
    spa_tag(MessagesEqual, "0");
    MsgsDifferent();
  }
  done();
}

extern "C" {
void MsgCompareEntry() {
  spa_message_handler_entry();
  char msg1[MAX_MESSAGE_SIZE], msg2[MAX_MESSAGE_SIZE];
  compareMsgs(msg1, 0, msg2, 0);
}
void __attribute__((noinline)) done() {
  // Complicated NOP to prevent inlining.
  static uint8_t i = 0;
  i++;
}
void __attribute__((noinline)) MsgsEqual() {
  // Complicated NOP to prevent inlining.
  static uint8_t i = 0;
  i++;
}
void __attribute__((noinline)) MsgsDifferent() {
  // Complicated NOP to prevent inlining.
  static uint8_t i = 0;
  i++;
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
  char msg1[MAX_MESSAGE_SIZE], msg2[MAX_MESSAGE_SIZE];

  ssize_t size1 = recvfrom(s, msg1, MAX_MESSAGE_SIZE, 0,
                           (struct sockaddr *)&si_client, &si_client_len);
  assert(size1 >= 0);
  std::cerr << "Received first message from " << inet_ntoa(si_client.sin_addr)
            << ":" << ntohs(si_client.sin_port) << std::endl;

  ssize_t size2 = recvfrom(s, msg2, MAX_MESSAGE_SIZE, 0,
                           (struct sockaddr *)&si_client, &si_client_len);
  assert(size2 >= 0);
  std::cerr << "Received second message from " << inet_ntoa(si_client.sin_addr)
            << ":" << ntohs(si_client.sin_port) << std::endl;

  compareMsgs(msg1, size1, msg2, size2);
}
