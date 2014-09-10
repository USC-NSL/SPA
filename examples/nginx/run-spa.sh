#!/bin/sh

parallel-spa-server.sh spdylay.paths spa-server.paths objs/nginx.bc conf/nginx.conf conf/mime.types logs/error.log
