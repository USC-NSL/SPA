#!/bin/sh

set -e

time spaTest2Str -f sip-request.tested sip-request.tested.txt 2>&1 | tee -a sip-request.tested.txt.log
