#!/bin/sh

spaAnalyzeTest -f sip-request.tested 2>&1 | tee -a BadInputs.default.log
