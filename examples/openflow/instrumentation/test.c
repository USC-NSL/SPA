#include <stdio.h>

#include "openflow/openflow.h"
#include "vconn.h"
#include "ofpbuf.h"

#include <spa/spaRuntime.h>

void __attribute__((noinline,used)) SpaHandleQueryEntry() {
   spa_message_handler_entry();
   struct lswitch* sw = lswitch_create(NULL, 1, OFP_FLOW_PERMANENT);
   struct ofpbuf buf;
   struct rconn* rc = NULL;
   buf.data = malloc(1500);
   buf.size = 1500;

   lswitch_process_packet(sw, rc, buf);
}

int main(int argc, char** argv) {
   lswitch_process_packet(NULL, NULL, NULL);
   return 0;
}

