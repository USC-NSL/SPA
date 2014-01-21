#!/bin/sh

set -e

time spaBadInputs -client spdylay/spa-client.paths -server nginx/nginx.paths -f -p 50 -j 1000 -w 5000 -d spdylay-nginx.untested.debug -o spdylay-nginx.untested 2>&1 | tee -a spdylay-nginx.untested.log
