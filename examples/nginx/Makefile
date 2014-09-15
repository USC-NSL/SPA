NGINX        := nginx-1.5.5
TARGETS      := out/dbg/objs/nginx out/llvm/objs/nginx out/test/objs/nginx
CLIENT_PATHS := spdylay.paths
BASEFILES    := $(NGINX)/conf/nginx.conf $(NGINX)/conf/mime.types $(NGINX)/logs/error.log

OBJS := objs/src/core/nginx.o objs/src/core/ngx_log.o objs/src/core/ngx_palloc.o objs/src/core/ngx_array.o objs/src/core/ngx_list.o objs/src/core/ngx_hash.o objs/src/core/ngx_buf.o objs/src/core/ngx_queue.o objs/src/core/ngx_output_chain.o objs/src/core/ngx_string.o objs/src/core/ngx_parse.o objs/src/core/ngx_inet.o objs/src/core/ngx_file.o objs/src/core/ngx_crc32.o objs/src/core/ngx_murmurhash.o objs/src/core/ngx_md5.o objs/src/core/ngx_rbtree.o objs/src/core/ngx_radix_tree.o objs/src/core/ngx_slab.o objs/src/core/ngx_times.o objs/src/core/ngx_shmtx.o objs/src/core/ngx_connection.o objs/src/core/ngx_cycle.o objs/src/core/ngx_spinlock.o objs/src/core/ngx_cpuinfo.o objs/src/core/ngx_conf_file.o objs/src/core/ngx_resolver.o objs/src/core/ngx_open_file_cache.o objs/src/core/ngx_crypt.o objs/src/event/ngx_event.o objs/src/event/ngx_event_timer.o objs/src/event/ngx_event_posted.o objs/src/event/ngx_event_busy_lock.o objs/src/event/ngx_event_accept.o objs/src/event/ngx_event_connect.o objs/src/event/ngx_event_pipe.o objs/src/os/unix/ngx_time.o objs/src/os/unix/ngx_errno.o objs/src/os/unix/ngx_alloc.o objs/src/os/unix/ngx_files.o objs/src/os/unix/ngx_socket.o objs/src/os/unix/ngx_recv.o objs/src/os/unix/ngx_readv_chain.o objs/src/os/unix/ngx_udp_recv.o objs/src/os/unix/ngx_send.o objs/src/os/unix/ngx_writev_chain.o objs/src/os/unix/ngx_channel.o objs/src/os/unix/ngx_shmem.o objs/src/os/unix/ngx_process.o objs/src/os/unix/ngx_daemon.o objs/src/os/unix/ngx_setaffinity.o objs/src/os/unix/ngx_setproctitle.o objs/src/os/unix/ngx_posix_init.o objs/src/os/unix/ngx_user.o objs/src/os/unix/ngx_process_cycle.o objs/src/os/unix/ngx_linux_init.o objs/src/event/modules/ngx_epoll_module.o objs/src/os/unix/ngx_linux_sendfile_chain.o objs/src/http/ngx_http.o objs/src/http/ngx_http_core_module.o objs/src/http/ngx_http_special_response.o objs/src/http/ngx_http_request.o objs/src/http/ngx_http_parse.o objs/src/http/ngx_http_header_filter_module.o objs/src/http/ngx_http_write_filter_module.o objs/src/http/ngx_http_copy_filter_module.o objs/src/http/modules/ngx_http_log_module.o objs/src/http/ngx_http_request_body.o objs/src/http/ngx_http_variables.o objs/src/http/ngx_http_script.o objs/src/http/ngx_http_upstream.o objs/src/http/ngx_http_upstream_round_robin.o objs/src/http/ngx_http_parse_time.o objs/src/http/modules/ngx_http_static_module.o objs/src/http/modules/ngx_http_index_module.o objs/src/http/modules/ngx_http_chunked_filter_module.o objs/src/http/modules/ngx_http_range_filter_module.o objs/src/http/modules/ngx_http_headers_filter_module.o objs/src/http/modules/ngx_http_not_modified_filter_module.o objs/src/http/ngx_http_busy_lock.o objs/src/http/ngx_http_spdy.o objs/src/http/ngx_http_spdy_module.o objs/src/http/ngx_http_spdy_filter_module.o objs/ngx_modules.o zlib/adler32.o   zlib/compress.o zlib/crc32.o zlib/deflate.o zlib/gzclose.o zlib/gzlib.o zlib/gzread.o zlib/gzwrite.o zlib/infback.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o zlib/uncompr.o zlib/zutil.o
LIB_SPA_TEST_MODULE := $(CURDIR)/../../Release+Asserts/lib/libSpaTestModule.so


default: all

all: $(TARGETS)

run-spa: BadInputs.tested.txt BadInputs.default

out/%/zlib/Makefile: zlib/configure-%
	mkdir -p `dirname $@`/..; \
	cp -a zlib `dirname $@`/..; \
	cd `dirname $@`; \
	./configure-`echo $@ | cut -d / -f 2`; \
	cd ../../..

out/%/Makefile: $(NGINX)/configure-% out/%/zlib/Makefile
	mkdir -p `dirname $@`
	cp -a $(NGINX)/. `dirname $@`
	cd `dirname $@`; \
	./configure-`echo $@ | cut -d / -f 2`; \
	cd ../..

out/dbg/objs/nginx: out/dbg/Makefile
	mkdir -p `dirname $@`
	$(MAKE) -C `dirname $@`/..

out/llvm/objs/nginx: out/llvm/Makefile
	mkdir -p `dirname $@`
	cd `dirname $@`/..; \
	$(MAKE) -f objs/Makefile $(OBJS); \
	llvm-link -o objs/nginx $(OBJS); \
	cd ../..

out/test/objs/nginx: out/test/Makefile
	mkdir -p `dirname $@`
	cd `dirname $@`/..; \
	$(MAKE) -f objs/Makefile $(OBJS); \
	cc -o objs/nginx $(OBJS) $(LIB_SPA_TEST_MODULE) -lpthread; \
	cd ../..

nginx.paths: $(CLIENT_PATHS) $(BASEFILES)
	parallel-spa-server.sh $< $@ $(NGINX_BC) $(BASEFILES)

# BadInputs.untested: spa-server.paths
# 	spaBadInputs -p 5 -j 100 -server $< -o $@ -d $@.debug 2>&1 | tee $@.log
BadInputs.untested: nginx.paths
	parallel-badinputs.sh $< $@

BadInputs.tested: BadInputs.untested out/test/objs/nginx
	spaE2ETest $< $@ BadInputs.FalsePositives '../spdylay/spa-client.e2etest.exe' './out/test/objs/nginx' 2>&1 | tee $@.log

.PRECIOUS: $(CLIENT_PATHS) nginx.paths BadInputs.untested BadInputs.tested

%.tested.txt: %.tested
	spaTest2Str $< $@ 2>&1 | tee $@.log

BadInputs.default: BadInputs.tested
	spaAnalyzeTest $< 2>&1 | tee $@.log

clean:
	rm -rf out/
