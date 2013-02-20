#!/bin/sh

set -e

time spaE2ETest -f sip-request.untested sip-request.tested exosip/spa-eXosip2/client.e2etest.exe pjsip/pjsua.test.exe exosip/spa-eXosip2/server.e2etest.exe 2>&1 | tee -a sip-request.tested.log
