#!/bin/bash
OUTFILE=openflow-badinputs.untested
spaBadInputs -client client.paths -server server.paths -o $OUTFILE -d $OUTFILE.debug 2>&1 | tee $OUTFILE.log
