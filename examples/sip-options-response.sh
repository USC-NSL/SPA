#!/bin/sh

set -e

time spaBadInputs -client exosip-options-server.paths -server pjsip-options-client.paths -p 8 -d sip-options-response.untested.debug -o sip-options-response.untested 2>&1 | tee -a sip-options-response.untested.log

time spaE2ETest sip-options-response.untested sip-options-response.tested pjsip/pjsua.test.exe exosip/spa-eXosip2/options-server.e2etest.exe 2>&1 | tee -a sip-options-response.tested.log
