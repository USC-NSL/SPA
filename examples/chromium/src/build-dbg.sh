#!/bin/sh

GYP_GENERATORS=make build/gyp_chromium

make -j8 fetch_client
