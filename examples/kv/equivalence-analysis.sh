#!/bin/bash

set -e

rm -f kv.paths

# gdb --args \
spa-explore \
    --out-paths kv.paths \
    --out-paths-append \
    --start-from spa_entry \
    --toward kv_done \
    --output-at kv_done \
    --output-at kv_done \
    --stop-at kv_done \
    --participant kv-local \
    kv-local.bc \
    2>&1 | tee kv-local.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --dont-load-empty-path \
    --out-paths kv.paths \
    --out-paths-append \
    --connect spa_in_api_operations_kv-client=spa_in_api_operations_kv-local \
    --connect-sockets \
    --start-from spa_entry \
    --toward kv_done \
    --output-at spa_msg_no_input_point \
    --output-at kv_done \
    --stop-at kv_done \
    --participant kv-client \
    --ip 127.0.0.1 \
    kv-client-udp.bc \
    2>&1 | tee kv-client.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --dont-load-empty-path \
    --out-paths kv.paths \
    --out-paths-append \
    --connect-sockets \
    --start-from spa_entry1 \
    --toward spa_msg_no_input_point \
    --output-at spa_msg_no_input_point \
    --stop-at spa_msg_no_input_point \
    --participant kv-server-1 \
    --ip 127.0.0.2 \
    kv-server-udp.bc \
    2>&1 | tee kv-server1.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --dont-load-empty-path \
    --out-paths kv.paths \
    --out-paths-append \
    --connect-sockets \
    --start-from spa_entry2 \
    --toward spa_msg_no_input_point \
    --output-at spa_msg_no_input_point \
    --stop-at spa_msg_no_input_point \
    --participant kv-server-2 \
    --ip 127.0.0.3 \
    kv-server-udp.bc \
    2>&1 | tee kv-server2.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --dont-load-empty-path \
    --out-paths kv.paths \
    --out-paths-append \
    --connect-sockets \
    --start-from spa_entry3 \
    --toward spa_msg_no_input_point \
    --output-at spa_msg_no_input_point \
    --stop-at spa_msg_no_input_point \
    --participant kv-server-3 \
    --ip 127.0.0.4 \
    kv-server-udp.bc \
    2>&1 | tee kv-server3.log &

# gdb --args \
spa-explore \
    --in-paths kv.paths \
    --follow-in-paths \
    --dont-load-empty-path \
    --out-paths kv.paths \
    --out-paths-append \
    --connect spa_in_api_in1_compare=spa_out_api_value_kv-local \
    --connect spa_in_api_size_in1_compare=spa_out_api_size_value_kv-local \
    --connect spa_in_api_in2_compare=spa_out_api_value_kv-client \
    --connect spa_in_api_size_in2_compare=spa_out_api_size_value_kv-client \
    --start-from ApiCompareEntry \
    --toward done \
    --output-at done \
    --stop-at done \
    --participant compare \
    ../spa-tools/msg-compare.bc \
    2>&1 | tee compare.log &

trap 'kill $(jobs -p)' EXIT

wait
