#!/bin/sh

[ -f Makefile ] || GYP_GENERATORS=make build/gyp_chromium

CXXFLAGS='-I/home/david/Projects/spa/include' make -kj48 fetch_client
