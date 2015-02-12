#!/bin/sh

set -e

parallel-pic.sh \
  spdylay/spa-client.bc out/llvm/objs/nginx \
  'spa/examples/nginx' \
  '../spdylay/spa-client.e2etest.exe' 'out/test/objs/nginx' 8000 \
  spdylay.paths spdylay-nginx.paths spdylay-nginx-valid.paths \
  2>&1 | tee spdylay-nginx.log
