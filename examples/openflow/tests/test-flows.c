#include <config.h>
#include "flow.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "openflow/openflow.h"
#include "timeval.h"
#include "ofpbuf.h"
#include "ofp-print.h"
#include "pcap.h"
#include "util.h"
#include "vlog.h"

#undef NDEBUG
#include <assert.h>

int
main(int argc UNUSED, char *argv[])
{
    struct ofp_match expected_match;
    FILE *flows, *pcap;
    int retval;
    int n = 0, errors = 0;

    set_program_name(argv[0]);
    time_init();
    vlog_init();

    flows = stdin;
    pcap = fdopen(3, "rb");
    if (!pcap) {
        ofp_fatal(errno, "failed to open fd 3 for reading");
    }

    retval = pcap_read_header(pcap);
    if (retval) {
        ofp_fatal(retval > 0 ? retval : 0, "reading pcap header failed");
    }

    while (fread(&expected_match, sizeof expected_match, 1, flows)) {
        struct ofpbuf *packet;
        struct ofp_match extracted_match;
        struct flow flow;

        n++;

        retval = pcap_read(pcap, &packet);
        if (retval == EOF) {
            ofp_fatal(0, "unexpected end of file reading pcap file");
        } else if (retval) {
            ofp_fatal(retval, "error reading pcap file");
        }

        flow_extract(packet, 0, &flow);
        flow_fill_match(&extracted_match, &flow, 0);

        if (memcmp(&expected_match, &extracted_match, sizeof expected_match)) {
            char *exp_s = ofp_match_to_string(&expected_match, 2);
            char *got_s = ofp_match_to_string(&extracted_match, 2);
            errors++;
            printf("mismatch on packet #%d (1-based).\n", n);
            printf("Packet:\n");
            ofp_print_packet(stdout, packet->data, packet->size, packet->size);
            printf("Expected flow:\n%s\n", exp_s);
            printf("Actually extracted flow:\n%s\n", got_s);
            printf("\n");
            free(exp_s);
            free(got_s);
        }

        ofpbuf_delete(packet);
    }
    printf("checked %d packets, %d errors\n", n, errors);
    return errors != 0;
}

