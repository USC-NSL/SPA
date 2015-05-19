#!/bin/sh

set -xe

spa-explore \
  --out-paths netcalc-client.paths \
  --start-from SpaExecuteQueryEntry \
  --toward "spa_msg_output_point" \
  --stop-at "spa_msg_output_point" \
  --output-at "spa_msg_output_point" \
  netcalc-client.bc

spa-explore \
  --in-paths netcalc-client.paths --connect-in-out \
  --out-paths netcalc-server.paths \
  --start-from SpaHandleQueryEntry \
  --away-from spa_valid_path_point \
  --output-terminal \
  --output-at "NOT REACHING spa_valid_path_point" \
  --filter-output "REACHED spa_msg_input_point \
                   AND NOT REACHED spa_valid_path_point" \
  netcalc-server.bc

killall netcalc-client.test netcalc-server.test || true
spa-validate netcalc-server.paths client-server-interop.paths /dev/null \
    "RUN ./netcalc-server.test; \
    WAIT LISTEN UDP 3141; \
    RUN ./netcalc-client.test 0 0 +; \
    WAIT DONE 2; \
    KILL 1; \
    TIMEOUT 2000; \
    CHECK REACHED spa_msg_input_point; \
    CHECK NOT REACHED spa_valid_path_point;"
