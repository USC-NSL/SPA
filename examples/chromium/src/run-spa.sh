#!/bin/sh

spa \
	--path-file=fetch_client.paths \
	--client out/Debug/fetch_client \
	2>&1 | tee fetch_client.paths.log
