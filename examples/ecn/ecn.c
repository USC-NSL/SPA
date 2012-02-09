#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <assert.h>
#include <klee/klee.h>
#include <spa/max.h>


//the port at which the receiver operates
#define RECEIVER_PORT      40404
// The port on which the sender listens for ACKs.
#define SENDER_PORT      40405

//the size of the packets we send
#define PKT_SIZE           100

//the probability which the router marks (i.e., sets the ecn bit to 1) packets.
#define PKT_MARKING_PROB_AT_RTR   0.25

//the probability with which the receiver reflects the marked ecn bit honestly
#define ECN_REFLECTION_PROB_AT_RCVR   1

//maximum number of packets for which the sender has not received a response
#define MAX_PKTS_IN_FLIGHT 100

//maximum number of packets to send for the sender; we exit when this limit is reached
#define MAX_PKTS_TO_SEND_SNDR 1

// Synchronization pipe.
int fdpair[2];

//
//a simple struct for ecn packets
//
typedef struct EcnPkt {
  int ecnBit;
} EcnPkt;


//
// returns a random number between 0 and 1
//
double Random() {
  return ((double) rand())/RAND_MAX;
}

//
// sets the ecn bit of the packet to the specified val
// 
void SetEcnBit(void * buffer, int val) {
  EcnPkt * pkt = (EcnPkt *) buffer;
  pkt->ecnBit = val;
}

//
// returns the EcnBit of the packet
//
int GetEcnBit(void * buffer) {
  EcnPkt * pkt = (EcnPkt *) buffer;
  return pkt->ecnBit;
}

//
// simulates the network by marking the packets probabilistically
// and then sends it to the receiver
//
void Route(int sockfd, void * buffer) {

  if (Random() <= PKT_MARKING_PROB_AT_RTR) {
    SetEcnBit(buffer, 1);
  }

  if (send(sockfd, buffer, PKT_SIZE, 0) <=0) {
    perror("Sender.Route.send");
    exit(1);
  }

  printf("Sent packet with EcnBit %d\n", GetEcnBit(buffer));
}

void SenderHandlePacket( int sockfd, void *buffer, unsigned int length ) {
	//some variables that count packets
	static int num_pkts_in_flight = 1;
	static int num_pkts_sent = 1;
	static int num_pkts_rcvd = 0;

	//this is the number of packets that we'll send in current round
	int num_pkts_to_send = 0;
	//got a valid packet; update stats
	num_pkts_in_flight--;
	num_pkts_rcvd++;

	//sanity check
	assert( num_pkts_in_flight >= 0 ); 

	//read its ecn bit
	int ecnBit = GetEcnBit(buffer);
	max_input_var( &ecnBit, sizeof( ecnBit ), "SenderReceivedEcnBit" );
	//printf("Got a packet with EcnBit %d\n", ecnBit);
	
	//if ecn bit is zero, send two packets in response
	//else send none
	if (ecnBit == 0)
		num_pkts_to_send = 2;
	else 
		num_pkts_to_send = 0;
	
	//make sure that we don't send more than the max number in flight
	if (num_pkts_in_flight + num_pkts_to_send > MAX_PKTS_IN_FLIGHT) {
		printf("Reached the limit of in-flight packets\n");
		num_pkts_to_send = MAX_PKTS_IN_FLIGHT - num_pkts_in_flight;
	}
	
	//update stats
	num_pkts_sent+= num_pkts_to_send;
	num_pkts_in_flight += num_pkts_to_send;
	
// 	printf("Sending %d packets.   %d sent, %d rcvd, %d in-flight\n", 
// 			num_pkts_to_send, num_pkts_sent, num_pkts_rcvd, num_pkts_in_flight);
	
	//send the packets through Route
	while (num_pkts_to_send > 0) {
		SetEcnBit(buffer, 0);
		max_interesting();
		Route(sockfd, buffer);
		num_pkts_to_send--;
	}

	//exit if limit of sending reached
	if (num_pkts_sent > MAX_PKTS_TO_SEND_SNDR) {
		printf("Limit exceeded for number of packet to send (%d)", num_pkts_sent);
		exit(0);
	}
}

void MaxSenderHandlePacketEntry() {
	max_message_handler_entry(); // Annotate function as message handler entry point.

	SenderHandlePacket( 1, calloc( 1, PKT_SIZE ), PKT_SIZE );
}

//
// main function for the sender
//
int RunSender() {
 
  //open a socket for sending packets
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("Sender.socket");
    exit(1);
  }

  //construct the local socket structure and bind to it
  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(SENDER_PORT);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(local_addr.sin_zero, '\0', sizeof local_addr.sin_zero);

  if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof local_addr) < 0) {
	  perror("Sender.bind");
  }

  //construct the sockaddr structure for the receiver
  struct sockaddr_in remote_addr;
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(RECEIVER_PORT);
  remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  memset(remote_addr.sin_zero, '\0', sizeof remote_addr.sin_zero);

  //connect the socket to the remote end
  if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof remote_addr) < 0) {
    perror("Sender.connect");
    exit(1);
  }

  
  //the timeout we use for the select call
  struct timeval tv;
  tv.tv_sec = 1;     //1 second
  tv.tv_usec = 0;

  //the data buffer
  void * buffer = malloc(PKT_SIZE);

  SetEcnBit(buffer, 0);
  Route(sockfd, buffer);

  while (1) {
    printf("-----------------------\n");

    //set up the file descriptors for the select call
    fd_set readfds;
#ifdef ENABLE_MAX
	memset(&readfds, '\0', sizeof readfds);
#else // #ifdef ENABLE_MAX
    FD_ZERO(&readfds);
#endif // #ifdef ENABLE_MAX #else

    FD_SET(sockfd, &readfds);

    //select makes us wait until a packet is received on the socket or timeout, whichever is sooner
    select(sockfd+1, &readfds, NULL, NULL, &tv);

    if (FD_ISSET(sockfd, &readfds)) {
      //select returned because there was something on the socket

      if (recv(sockfd, buffer, PKT_SIZE, 0) <= 0) {
	//but we couldn't read a packet

	//this happens commonly when the receiver is not running, and we receiver a reset
	perror("Sender.recv");
	printf("      -->Receiver has not been started?\n");
	exit(1);
      } else {
		  SenderHandlePacket( sockfd, buffer, PKT_SIZE );
      }
    } else {
      //select returned because we timed out. most likely, the receiver is not running
      perror("Timed out.\n");
	  exit(1);
    }
  }
}


//
// main function for the receiver
//
int RunReceiver() {

  //get a local socket
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("Receiver.socket");
    exit(1);
  }
  
  //construct the local socket structure and bind to it
  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(RECEIVER_PORT);
  local_addr.sin_addr.s_addr = INADDR_ANY;
  memset(local_addr.sin_zero, '\0', sizeof local_addr.sin_zero);
  
  if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof local_addr) < 0) {
    perror("Receiver.bind");
  }
  
  //the buffer used for sending and receiving packets
  void * buffer = malloc(PKT_SIZE);

  //stats
  int num_pkts_processed = 0;

	// Signal sender to start.
	close( fdpair[1] );

  while ( num_pkts_processed <= MAX_PKTS_TO_SEND_SNDR ) {
    printf("---------------\n");

    //prepare for receiving a packet
    struct sockaddr remote_addr;
    int remote_addr_len = sizeof(struct sockaddr);
    int recv_len = 0;

    if ((recv_len = recvfrom(sockfd, buffer, PKT_SIZE, 0, &remote_addr, (socklen_t *)&remote_addr_len)) < 0) {
      perror("Receiver.recvfrom");
      exit(1);
    }

    //get the ecn bit of the packet
    int ecnBit = GetEcnBit(buffer);
    printf("Got pkt of size %d with EcnBit %d\n", recv_len, GetEcnBit(buffer));

    //if the ecnbit is non-zero, consider cheating
    if (ecnBit != 0) {
      if (Random() > ECN_REFLECTION_PROB_AT_RCVR) {
	SetEcnBit(buffer, 0);     
      }
    }

    //send the packet to the sender
    if (sendto(sockfd, buffer, recv_len, 0, &remote_addr, sizeof(struct sockaddr)) <= 0) {
      perror("Receiver.sendto");
      exit(1);
    }

    //update stats
    num_pkts_processed++;
    
    printf("Sent pkt with EcnBit %d.   %d processed\n", GetEcnBit(buffer), num_pkts_processed);
  }
  return 0;
}

//
// print the usage message
//
void Usage(char * progname) {
  printf("Usage: %s <sender|receiver>\n", progname);
  exit(0);
}

// 
// the fun begins here
// 
int main(int argc, char * argv[]) {
  if (argc < 2) {
    Usage(argv[0]);
  }
  if (strcmp(argv[1], "sender") == 0) {
    RunSender();
  }
  else if (strcmp(argv[1], "receiver") == 0) {
    RunReceiver();
  }
	else if (strcmp(argv[1], "cloud9") == 0) {
		// Start receiver first and synchronize using pipe before starting sender.
		assert(pipe(fdpair) == 0);
		pid_t pid = fork();
		assert(pid != -1);

		if ( pid == 0 ) { // Child
			close(fdpair[0]);
			RunReceiver();
		}

		// Wait for receiver.
		close( fdpair[1] );
		int data;
		assert(read(fdpair[0], &data, sizeof(data)) == 0);
		close(fdpair[0]);

		RunSender();
	} else {
    printf("\nSpecify 'sender' or 'receiver'.  You specified '%s'\n\n", argv[1]);
    Usage(argv[0]);
  }

  return 0;
}
