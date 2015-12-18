#!/bin/bash

set -e

./kv-server-tcp 12340 &
./kv-server-tcp 12341 &
./kv-server-tcp 12342 &

trap 'kill $(jobs -p)' EXIT

wait
