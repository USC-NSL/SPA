#!/bin/bash

set -e

rm -f kv.paths

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --connect-sockets \
    --start-from spa_entry \
    --toward kv_done \
    --output-when-log-exhausted \
    --output-at kv_done \
    --participant kv-client \
    kv-client.bc \
    2>&1 | tee kv-client.log &
CLIENT_PID=$!

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --connect-sockets \
    --start-from spa_entry1 \
    --toward spa_msg_output_point \
    --output-at spa_msg_output_point \
    --participant kv-server-1 \
    kv-server.bc \
    2>&1 | tee kv-server1.log &
SERVER1_PID=$!

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --connect-sockets \
    --start-from spa_entry2 \
    --toward spa_msg_output_point \
    --output-at spa_msg_output_point \
    --participant kv-server-2 \
    kv-server.bc \
    2>&1 | tee kv-server2.log &
SERVER2_PID=$!

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --out-paths kv.paths \
    --out-paths-append \
    --connect-sockets \
    --start-from spa_entry3 \
    --toward spa_msg_output_point \
    --output-at spa_msg_output_point \
    --participant kv-server-3 \
    kv-server.bc \
    2>&1 | tee kv-server3.log &
SERVER3_PID=$!

wait
