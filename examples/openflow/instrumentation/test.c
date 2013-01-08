#include <stdio.h>

#include "openflow/openflow.h"
#include "vconn.h"
#include "ofpbuf.h"

#include <spa/spaRuntime.h>

void SpaHandleQueryEntry() {
   spa_message_handler_entry();
   struct lswitch* sw;
   struct rconn* rc;
   struct ofpbuf* buf;
   lswitch_process_packet(sw,rc,buf);
}

int main(int argc, char** argv) {
   lswitch_process_packet(NULL, NULL, NULL);
   return 0;
}

