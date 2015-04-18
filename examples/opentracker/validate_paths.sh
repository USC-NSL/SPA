killall bttest ot.validate || true
spa-validate -d opentracker.paths validated.paths false-positives.paths \
"RUN ./ot.validate; \
		WAIT LISTEN TCP 6969; \
		RUN ./bttest foo; \
		WAIT DONE 2; \
		KILL 1; \
		TIMEOUT 2000; \
		CHECK REACHED spa_msg_input_point; \
		CHECK NOT REACHED spa_valid_path_point;" \
2>&1 | tee validated.paths.log