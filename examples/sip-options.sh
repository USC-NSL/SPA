#!/bin/sh

set -e

time spaBadInputs -client exosip.paths -server pjsip.paths -p 8 -d sip-options.untested.debug -o sip-options.untested 2>&1 | tee -a sip-options.untested.log

time spaE2ETest sip-options.untested sip-options.tested pjsip/pjsua.test.exe exosip/spa-eXosip2/options-client.e2etest.exe 2>&1 | tee -a sip-options.tested.log
