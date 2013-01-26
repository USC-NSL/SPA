#!/bin/sh

set -e

spaBadInputs -client exosip.paths -server pjsip.paths -o sip-options.untested |& tee sip-options.untested.log

spaE2ETest sip-options.untested sip-options.tested pjsip/pjsua.test.exe exosip/spa-eXosip2/options-client.e2etest.exe |& tee sip-options.tested.log
