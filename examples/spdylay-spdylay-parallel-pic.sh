#!/bin/sh

set -e

parallel-pic.sh \
  spdylay/spa-client.bc spa-server.bc \
  'spa/examples/spdylay' \
  './spa-client.e2etest.exe' './spa-server.e2etest.exe' 8000 \
  spdylay.paths spdylay-spdylay.paths spdylay-spdylay-valid.paths \
  2>&1 | tee spdylay-spdylay.log
