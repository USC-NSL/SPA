#!/bin/sh

set -e

parallel-pic.sh \
  exosip/spa-eXosip2/client.bc pjsip/pjsua.spa.bc \
  'spa/examples' \
  'cd exosip/spa-eXosip2; ./client.e2etest.exe' 'cd pjsip; ./pjsua.test.exe' 5061 \
  exosip.paths exosip-pjsip.paths exosip-pjsip-valid.paths \
  2>&1 | tee exosip-pjsip.log
