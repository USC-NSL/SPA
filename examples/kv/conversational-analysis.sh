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
    --ip 127.0.0.1 \
    --start-from spa_entry \
    --toward spa_msg_output_point \
    --output-at spa_msg_output_point \
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
    --ip 127.0.0.1 \
    --start-from spa_entry1 \
    --toward spa_msg_output_point \
    --output-at spa_msg_output_point \
    --participant kv-server-1 \
    kv-server.bc \
    2>&1 | tee kv-server.log &
SERVER1_PID=$!

wait
