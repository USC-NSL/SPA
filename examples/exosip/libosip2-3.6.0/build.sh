#!/bin/sh

set -e

OSIP_OBJS="fsm_misc.o ict_fsm.o ict.o ist_fsm.o ist.o nict_fsm.o \
	nict.o nist_fsm.o nist.o osip_dialog.o osip_event.o osip.o \
	osip_time.o osip_transaction.o port_fifo.o"

OSIPPARSER_OBJS="osip_accept_encoding.o osip_accept_language.o \
	osip_accept.o osip_alert_info.o osip_allow.o \
	osip_authentication_info.o osip_authorization.o osip_body.o \
	osip_call_id.o osip_call_info.o osip_contact.o \
	osip_content_disposition.o osip_content_encoding.o \
	osip_content_length.o osip_content_type.o osip_cseq.o \
	osip_error_info.o osip_from.o osip_header.o osip_list.o \
	osip_md5c.o osip_message.o osip_message_parse.o \
	osip_message_to_str.o osip_mime_version.o osip_parser_cfg.o \
	osip_port.o osip_proxy_authenticate.o \
	osip_proxy_authentication_info.o osip_proxy_authorization.o \
	osip_record_route.o osip_route.o osip_to.o osip_uri.o osip_via.o \
	osip_www_authenticate.o sdp_accessor.o sdp_message.o"

rm -f libosip2.bc libosip2-fix.bc libosip2-dbg.o libosip2-test.o libosip2-test-fix.o

./configure-llvm
make -skj clean
make -oj`grep -c processor /proc/cpuinfo` -C src/osip2 $OSIP_OBJS
make -oj`grep -c processor /proc/cpuinfo` -C src/osipparser2 $OSIPPARSER_OBJS
llvm-link src/osip*/*.o -o libosip2.bc

./configure-llvm-fix
make -skj clean
make -oj`grep -c processor /proc/cpuinfo` -C src/osip2 $OSIP_OBJS
make -oj`grep -c processor /proc/cpuinfo` -C src/osipparser2 $OSIPPARSER_OBJS
llvm-link src/osip*/*.o -o libosip2-fix.bc

./configure-dbg
make -skj clean
make -oj`grep -c processor /proc/cpuinfo` -C src/osip2 $OSIP_OBJS
make -oj`grep -c processor /proc/cpuinfo` -C src/osipparser2 $OSIPPARSER_OBJS
ld -r src/osip*/*.o -o libosip2-dbg.o

./configure-test
make -skj clean
make -oj`grep -c processor /proc/cpuinfo` -C src/osip2 $OSIP_OBJS
make -oj`grep -c processor /proc/cpuinfo` -C src/osipparser2 $OSIPPARSER_OBJS
ld -r src/osip*/*.o -o libosip2-test.o

./configure-test-fix
make -skj clean
make -oj`grep -c processor /proc/cpuinfo` -C src/osip2 $OSIP_OBJS
make -oj`grep -c processor /proc/cpuinfo` -C src/osipparser2 $OSIPPARSER_OBJS
ld -r src/osip*/*.o -o libosip2-test-fix.o
