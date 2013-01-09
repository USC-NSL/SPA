#include <stdio.h>

#include "openflow/openflow.h"
#include "vconn.h"
#include "ofpbuf.h"

#include <spa/spaRuntime.h>

void SpaHandleQueryEntry() {
   spa_message_handler_entry();
   struct ofpbuf buf;
   buf.data = malloc(1500);
   buf.size = 1500;

   lswitch_process_packet(NULL, NULL, buf);
}

int main(int argc, char** argv) {
   lswitch_process_packet(NULL, NULL, NULL);
   return 0;
}

