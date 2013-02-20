#!/bin/sh

set -e

time spaBadInputs -client exosip/spa-eXosip2/client.paths -server pjsip/pjsip.paths -p 6 -f -d sip-request.untested.debug -o sip-request.untested 2>&1 | tee -a sip-request.untested.log

# time spaE2ETest sip-request.untested sip-request.tested exosip/spa-eXosip2/client.e2etest.exe pjsip/pjsua.test.exe 2>&1 | tee -a sip-request.tested.log
