#!/bin/sh

spa-explore \
	--in-paths libbt.paths --connect-in-out \
	--out-paths opentracker.paths \
	--start-from spa_handleAnnounce \
	--away-from spa_valid_path_point \
	--output-terminal \
	--output-at "NOT REACHING spa_valid_path_point" \
	--filter-output "REACHED spa_msg_input_point \
	                 AND NOT REACHED spa_valid_path_point" \
	./opentracker 2>&1 | tee opentracker.paths.log