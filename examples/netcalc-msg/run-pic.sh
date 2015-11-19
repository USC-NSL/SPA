#/bin/bash

set -e

[ -d ../../Release+Asserts/bin ] && DIR=../../Release+Asserts/bin || DIR=../../Debug+Asserts/bin

# Client-side analysis.
$DIR/spa-pic --client netcalc-client.bc --path-file netcalc-client.paths

# Server-side analysis
$DIR/spa-pic --sender-paths netcalc-client.paths --server netcalc-server.bc --path-file netcalc-server.paths

# Validation stage
$DIR/spaE2ETest netcalc-server.paths validated.paths false-positives.paths 3141 './netcalc-client.test 1 1 +' './netcalc-server.test'

# Clustering
$DIR/spa-cluster-manual validated.paths
