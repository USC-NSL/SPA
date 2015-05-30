killall bttest ot.validate || true
rm -f validated.paths
spaE2ETest opentracker.paths validated.paths false-positives.paths 6969 './bttest foo' './ot.validate' 2>&1 | tee validated.paths.log
