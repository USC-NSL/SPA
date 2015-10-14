#!/bin/bash

# gdb --args \
spa-explore \
    --out-paths kv-client.paths \
    --start-from spa_entry \
    --output-at spa_msg_output_point \
    kv-client.bc \
    2>&1 | tee kv-client.paths.log
