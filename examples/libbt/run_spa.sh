spa-explore \
		--out-paths libbt.paths \
		--start-from SpaMainEntry \
		--toward "spa_msg_output_point" \
		--stop-at "spa_msg_output_point" \
		--output-at "spa_msg_output_point" \
		src/spaget.bc 2>&1 | tee libbt.paths.log
