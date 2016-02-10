#!/bin/bash

set -e

rm -f kv.paths

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --shallow-exploration \
    --connect-sockets \
    --start-from spa_entry \
    --toward kv_done \
    --output-at spa_msg_no_input_point \
    --output-at kv_done \
    --participant kv-client \
    --ip 127.0.0.1 \
    kv-client-tcp.bc \
    2>&1 | tee kv-client.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --shallow-exploration \
    --connect-sockets \
    --start-from spa_entry1 \
    --toward spa_msg_no_input_point \
    --output-at spa_msg_no_input_point \
    --participant kv-server-1 \
    --ip 127.0.0.2 \
    kv-server-tcp.bc \
    2>&1 | tee kv-server1.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --shallow-exploration \
    --connect-sockets \
    --start-from spa_entry2 \
    --toward spa_msg_no_input_point \
    --output-at spa_msg_no_input_point \
    --participant kv-server-2 \
    --ip 127.0.0.3 \
    kv-server-tcp.bc \
    2>&1 | tee kv-server2.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --shallow-exploration \
    --connect-sockets \
    --start-from spa_entry3 \
    --toward spa_msg_no_input_point \
    --output-at spa_msg_no_input_point \
    --participant kv-server-3 \
    --ip 127.0.0.4 \
    kv-server-tcp.bc \
    2>&1 | tee kv-server3.log &

# gdb --args \
spa-explore-conversation \
    --connect-sockets \
    --follow-in-paths \
    kv.paths \
    2>&1 | tee derived.log &

trap 'kill $(jobs -p)' EXIT

wait
