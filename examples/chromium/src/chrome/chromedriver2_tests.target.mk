# This file is generated by gyp; do not edit.

TOOLSET := target
TARGET := chromedriver2_tests
DEFS_Debug := \
	'-D_FILE_OFFSET_BITS=64' \
	'-DUSE_LINUX_BREAKPAD' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_DEFAULT_RENDER_THEME=1' \
	'-DUSE_LIBJPEG_TURBO=1' \
	'-DUSE_NSS=1' \
	'-DUSE_X11=1' \
	'-DENABLE_ONE_CLICK_SIGNIN' \
	'-DGTK_DISABLE_SINGLE_INCLUDES=1' \
	'-DENABLE_REMOTING=1' \
	'-DENABLE_WEBRTC=1' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DENABLE_INPUT_SPEECH' \
	'-DENABLE_NOTIFICATIONS' \
	'-DENABLE_GPU=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DENABLE_TASK_MANAGER=1' \
	'-DENABLE_EXTENSIONS=1' \
	'-DENABLE_PLUGIN_INSTALLATION=1' \
	'-DENABLE_PLUGINS=1' \
	'-DENABLE_SESSION_SERVICE=1' \
	'-DENABLE_THEMES=1' \
	'-DENABLE_BACKGROUND=1' \
	'-DENABLE_AUTOMATION=1' \
	'-DENABLE_GOOGLE_NOW=1' \
	'-DENABLE_LANGUAGE_DETECTION=1' \
	'-DENABLE_PRINTING=1' \
	'-DENABLE_CAPTIVE_PORTAL_DETECTION=1' \
	'-DENABLE_MANAGED_USERS=1' \
	'-DUNIT_TEST' \
	'-DGTEST_HAS_RTTI=0' \
	'-D__STDC_CONSTANT_MACROS' \
	'-D__STDC_FORMAT_MACROS' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=1' \
	'-DWTF_USE_DYNAMIC_ANNOTATIONS=1' \
	'-D_DEBUG'

# Flags passed to all source files.
CFLAGS_Debug := \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-Werror \
	-pthread \
	-fno-exceptions \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wheader-hygiene \
	-Wno-unused-function \
	-Wno-char-subscripts \
	-Wno-unnamed-type-template-args \
	-Wno-c++11-extensions \
	-Wno-covered-switch-default \
	-Wstring-conversion \
	-Xclang \
	-load \
	-Xclang \
	/home/david/Projects/spa/examples/chromium/src/tools/clang/scripts/../../../third_party/llvm-build/Release+Asserts/lib/libFindBadConstructs.so \
	-Xclang \
	-add-plugin \
	-Xclang \
	find-bad-constructs \
	-Xclang \
	-plugin-arg-find-bad-constructs \
	-Xclang \
	skip-virtuals-in-implementations \
	-pthread \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
	-I/usr/include/gtk-2.0 \
	-I/usr/lib/x86_64-linux-gnu/gtk-2.0/include \
	-I/usr/include/atk-1.0 \
	-I/usr/include/cairo \
	-I/usr/include/gdk-pixbuf-2.0 \
	-I/usr/include/pango-1.0 \
	-I/usr/include/gio-unix-2.0/ \
	-I/usr/include/freetype2 \
	-I/usr/include/pixman-1 \
	-I/usr/include/libpng12 \
	-I/usr/include/harfbuzz \
	-pthread \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
	-O0 \
	-g

# Flags passed to only C files.
CFLAGS_C_Debug :=

# Flags passed to only C++ files.
CFLAGS_CC_Debug := \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare

INCS_Debug := \
	-Ichrome/.., \
	-I. \
	-Itesting/gtest/include

DEFS_Release := \
	'-D_FILE_OFFSET_BITS=64' \
	'-DUSE_LINUX_BREAKPAD' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_DEFAULT_RENDER_THEME=1' \
	'-DUSE_LIBJPEG_TURBO=1' \
	'-DUSE_NSS=1' \
	'-DUSE_X11=1' \
	'-DENABLE_ONE_CLICK_SIGNIN' \
	'-DGTK_DISABLE_SINGLE_INCLUDES=1' \
	'-DENABLE_REMOTING=1' \
	'-DENABLE_WEBRTC=1' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DENABLE_INPUT_SPEECH' \
	'-DENABLE_NOTIFICATIONS' \
	'-DENABLE_GPU=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DENABLE_TASK_MANAGER=1' \
	'-DENABLE_EXTENSIONS=1' \
	'-DENABLE_PLUGIN_INSTALLATION=1' \
	'-DENABLE_PLUGINS=1' \
	'-DENABLE_SESSION_SERVICE=1' \
	'-DENABLE_THEMES=1' \
	'-DENABLE_BACKGROUND=1' \
	'-DENABLE_AUTOMATION=1' \
	'-DENABLE_GOOGLE_NOW=1' \
	'-DENABLE_LANGUAGE_DETECTION=1' \
	'-DENABLE_PRINTING=1' \
	'-DENABLE_CAPTIVE_PORTAL_DETECTION=1' \
	'-DENABLE_MANAGED_USERS=1' \
	'-DUNIT_TEST' \
	'-DGTEST_HAS_RTTI=0' \
	'-D__STDC_CONSTANT_MACROS' \
	'-D__STDC_FORMAT_MACROS' \
	'-DNDEBUG' \
	'-DNVALGRIND' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=0' \
	'-D_FORTIFY_SOURCE=2'

# Flags passed to all source files.
CFLAGS_Release := \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-Werror \
	-pthread \
	-fno-exceptions \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wheader-hygiene \
	-Wno-unused-function \
	-Wno-char-subscripts \
	-Wno-unnamed-type-template-args \
	-Wno-c++11-extensions \
	-Wno-covered-switch-default \
	-Wstring-conversion \
	-Xclang \
	-load \
	-Xclang \
	/home/david/Projects/spa/examples/chromium/src/tools/clang/scripts/../../../third_party/llvm-build/Release+Asserts/lib/libFindBadConstructs.so \
	-Xclang \
	-add-plugin \
	-Xclang \
	find-bad-constructs \
	-Xclang \
	-plugin-arg-find-bad-constructs \
	-Xclang \
	skip-virtuals-in-implementations \
	-pthread \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
	-I/usr/include/gtk-2.0 \
	-I/usr/lib/x86_64-linux-gnu/gtk-2.0/include \
	-I/usr/include/atk-1.0 \
	-I/usr/include/cairo \
	-I/usr/include/gdk-pixbuf-2.0 \
	-I/usr/include/pango-1.0 \
	-I/usr/include/gio-unix-2.0/ \
	-I/usr/include/freetype2 \
	-I/usr/include/pixman-1 \
	-I/usr/include/libpng12 \
	-I/usr/include/harfbuzz \
	-pthread \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
	-O2 \
	-fdata-sections \
	-ffunction-sections

# Flags passed to only C files.
CFLAGS_C_Release :=

# Flags passed to only C++ files.
CFLAGS_CC_Release := \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare

INCS_Release := \
	-Ichrome/.., \
	-I. \
	-Itesting/gtest/include

OBJS := \
	$(obj).target/$(TARGET)/chrome/test/chromedriver/key_converter_unittest.o \
	$(obj).target/$(TARGET)/chrome/test/chromedriver/keycode_text_conversion_unittest.o \
	$(obj).target/$(TARGET)/chrome/test/chromedriver/net/net_util_unittest.o \
	$(obj).target/$(TARGET)/chrome/test/chromedriver/net/sync_websocket_impl_unittest.o \
	$(obj).target/$(TARGET)/chrome/test/chromedriver/net/test_http_server.o \
	$(obj).target/$(TARGET)/chrome/test/chromedriver/net/websocket_unittest.o \
	$(obj).target/$(TARGET)/chrome/test/chromedriver/test_util.o

# Add to the list of files we specially track dependencies for.
all_deps += $(OBJS)

# Make sure our dependencies are built before any of us.
$(OBJS): | $(obj).target/chrome/libchromedriver2_lib.a $(obj).target/base/libbase.a $(obj).target/base/librun_all_unittests.a $(obj).target/build/temp_gyp/libgoogleurl.a $(obj).target/net/libhttp_server.a $(obj).target/net/libnet.a $(obj).target/net/libnet_test_support.a $(obj).target/testing/libgtest.a $(obj).target/chrome/libchrome_devtools_lib.a $(obj).target/base/libbase_static.a $(obj).target/base/allocator/liballocator_extension_thunks.a $(obj).target/testing/gtest_prod.stamp $(obj).target/third_party/modp_b64/libmodp_b64.a $(obj).target/base/third_party/dynamic_annotations/libdynamic_annotations.a $(obj).target/base/libsymbolize.a $(obj).target/build/linux/glib.stamp $(obj).target/base/libxdg_mime.a $(obj).target/build/linux/gtk.stamp $(obj).target/build/linux/x11.stamp $(obj).target/third_party/libevent/libevent.a $(obj).target/third_party/icu/libicudata.a $(obj).target/third_party/icu/libicui18n.a $(obj).target/third_party/icu/libicuuc.a $(obj).target/base/libbase_i18n.a $(obj).target/crypto/libcrcrypto.a $(obj).target/build/linux/ssl.stamp $(obj).target/net/third_party/nss/libcrssl.a $(obj).target/third_party/zlib/libchrome_zlib.a $(obj).target/sdch/libsdch.a $(obj).target/net/net_resources.stamp $(obj).target/build/linux/gconf.stamp $(obj).target/build/linux/libgio.a $(obj).target/build/linux/libresolv.stamp $(obj).target/build/linux/gdk.stamp $(obj).target/build/linux/dbus.stamp $(obj).target/dbus/libdbus.a $(obj).target/third_party/protobuf/libprotobuf_lite.a $(obj).target/third_party/zlib/libminizip.a $(obj).target/ui/libui.a $(obj).target/skia/libskia.a $(obj).target/skia/libskia_opts.a $(obj).target/skia/libskia_opts_ssse3.a $(obj).target/third_party/sfntly/libsfntly.a $(obj).target/third_party/WebKit/Source/WebKit/chromium/skia_webkit.stamp $(obj).target/build/linux/fontconfig.stamp $(obj).target/build/linux/freetype2.stamp $(obj).target/build/linux/pangocairo.stamp $(obj).target/third_party/libpng/libpng.a $(obj).target/ui/base/strings/ui_strings.stamp $(obj).target/ui/ui_resources.stamp $(obj).target/third_party/libjpeg_turbo/libjpeg_turbo.a $(obj).target/build/linux/xext.stamp $(obj).target/build/linux/xfixes.stamp $(obj).target/base/libtest_support_base.a $(obj).target/testing/libgmock.a $(obj).target/third_party/protobuf/py_proto.stamp $(obj).target/base/allocator/liballocator.a $(obj).target/net/libnet_with_v8.a $(obj).target/v8/tools/gyp/v8.stamp $(obj).target/v8/tools/gyp/libv8_base.x64.a $(obj).target/v8/tools/gyp/libv8_snapshot.a $(obj).host/v8/tools/gyp/js2c.stamp

# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.
$(OBJS): TOOLSET := $(TOOLSET)
$(OBJS): GYP_CFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))  $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_C_$(BUILDTYPE))
$(OBJS): GYP_CXXFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))  $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_CC_$(BUILDTYPE))

# Suffix rules, putting all outputs into $(obj).

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

# Try building from generated source, too.

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

# End of this set of suffix rules
### Rules for final target.
LDFLAGS_Debug := \
	-Wl,-z,now \
	-Wl,-z,relro \
	-pthread \
	-Wl,-z,noexecstack \
	-fPIC \
	-Wl,--threads \
	-Wl,--thread-count=4 \
	-B$(builddir)/../../third_party/gold \
	-L$(builddir) \
	-Wl,-uIsHeapProfilerRunning,-uProfilerStart \
	-Wl,-u_Z21InitialMallocHook_NewPKvj,-u_Z22InitialMallocHook_MMapPKvS0_jiiix,-u_Z22InitialMallocHook_SbrkPKvi \
	-Wl,-u_Z21InitialMallocHook_NewPKvm,-u_Z22InitialMallocHook_MMapPKvS0_miiil,-u_Z22InitialMallocHook_SbrkPKvl \
	-Wl,-u_ZN15HeapLeakChecker12IgnoreObjectEPKv,-u_ZN15HeapLeakChecker14UnIgnoreObjectEPKv \
	-Wl,--icf=none

LDFLAGS_Release := \
	-Wl,-z,now \
	-Wl,-z,relro \
	-pthread \
	-Wl,-z,noexecstack \
	-fPIC \
	-Wl,--threads \
	-Wl,--thread-count=4 \
	-B$(builddir)/../../third_party/gold \
	-L$(builddir) \
	-Wl,-uIsHeapProfilerRunning,-uProfilerStart \
	-Wl,-u_Z21InitialMallocHook_NewPKvj,-u_Z22InitialMallocHook_MMapPKvS0_jiiix,-u_Z22InitialMallocHook_SbrkPKvi \
	-Wl,-u_Z21InitialMallocHook_NewPKvm,-u_Z22InitialMallocHook_MMapPKvS0_miiil,-u_Z22InitialMallocHook_SbrkPKvl \
	-Wl,-u_ZN15HeapLeakChecker12IgnoreObjectEPKv,-u_ZN15HeapLeakChecker14UnIgnoreObjectEPKv \
	-Wl,--icf=none \
	-Wl,-O1 \
	-Wl,--as-needed \
	-Wl,--gc-sections

LIBS := \
	 \
	-lX11 \
	-lXcursor \
	-lXrandr \
	-lXrender \
	-lrt \
	-ldl \
	-lgmodule-2.0 \
	-lgobject-2.0 \
	-lgthread-2.0 \
	-lglib-2.0 \
	-lXtst \
	-lgtk-x11-2.0 \
	-lgdk-x11-2.0 \
	-latk-1.0 \
	-lgio-2.0 \
	-lpangoft2-1.0 \
	-lpangocairo-1.0 \
	-lgdk_pixbuf-2.0 \
	-lcairo \
	-lpango-1.0 \
	-lfontconfig \
	-lfreetype \
	-lXi \
	-lnss3 \
	-lnssutil3 \
	-lsmime3 \
	-lplds4 \
	-lplc4 \
	-lnspr4 \
	-lgconf-2 \
	-lresolv \
	-ldbus-1 \
	-lXext \
	-lXfixes

$(builddir)/chromedriver2_tests: GYP_LDFLAGS := $(LDFLAGS_$(BUILDTYPE))
$(builddir)/chromedriver2_tests: LIBS := $(LIBS)
$(builddir)/chromedriver2_tests: LD_INPUTS := $(OBJS) $(obj).target/chrome/libchromedriver2_lib.a $(obj).target/base/libbase.a $(obj).target/base/librun_all_unittests.a $(obj).target/build/temp_gyp/libgoogleurl.a $(obj).target/net/libhttp_server.a $(obj).target/net/libnet.a $(obj).target/net/libnet_test_support.a $(obj).target/testing/libgtest.a $(obj).target/chrome/libchrome_devtools_lib.a $(obj).target/base/libbase_static.a $(obj).target/base/allocator/liballocator_extension_thunks.a $(obj).target/third_party/modp_b64/libmodp_b64.a $(obj).target/base/third_party/dynamic_annotations/libdynamic_annotations.a $(obj).target/base/libsymbolize.a $(obj).target/base/libxdg_mime.a $(obj).target/third_party/libevent/libevent.a $(obj).target/third_party/icu/libicudata.a $(obj).target/third_party/icu/libicui18n.a $(obj).target/third_party/icu/libicuuc.a $(obj).target/base/libbase_i18n.a $(obj).target/crypto/libcrcrypto.a $(obj).target/net/third_party/nss/libcrssl.a $(obj).target/third_party/zlib/libchrome_zlib.a $(obj).target/sdch/libsdch.a $(obj).target/build/linux/libgio.a $(obj).target/dbus/libdbus.a $(obj).target/third_party/protobuf/libprotobuf_lite.a $(obj).target/third_party/zlib/libminizip.a $(obj).target/ui/libui.a $(obj).target/skia/libskia.a $(obj).target/skia/libskia_opts.a $(obj).target/skia/libskia_opts_ssse3.a $(obj).target/third_party/sfntly/libsfntly.a $(obj).target/third_party/libpng/libpng.a $(obj).target/third_party/libjpeg_turbo/libjpeg_turbo.a $(obj).target/base/libtest_support_base.a $(obj).target/testing/libgmock.a $(obj).target/base/allocator/liballocator.a $(obj).target/net/libnet_with_v8.a $(obj).target/v8/tools/gyp/libv8_base.x64.a $(obj).target/v8/tools/gyp/libv8_snapshot.a
$(builddir)/chromedriver2_tests: TOOLSET := $(TOOLSET)
$(builddir)/chromedriver2_tests: $(OBJS) $(obj).target/chrome/libchromedriver2_lib.a $(obj).target/base/libbase.a $(obj).target/base/librun_all_unittests.a $(obj).target/build/temp_gyp/libgoogleurl.a $(obj).target/net/libhttp_server.a $(obj).target/net/libnet.a $(obj).target/net/libnet_test_support.a $(obj).target/testing/libgtest.a $(obj).target/chrome/libchrome_devtools_lib.a $(obj).target/base/libbase_static.a $(obj).target/base/allocator/liballocator_extension_thunks.a $(obj).target/third_party/modp_b64/libmodp_b64.a $(obj).target/base/third_party/dynamic_annotations/libdynamic_annotations.a $(obj).target/base/libsymbolize.a $(obj).target/base/libxdg_mime.a $(obj).target/third_party/libevent/libevent.a $(obj).target/third_party/icu/libicudata.a $(obj).target/third_party/icu/libicui18n.a $(obj).target/third_party/icu/libicuuc.a $(obj).target/base/libbase_i18n.a $(obj).target/crypto/libcrcrypto.a $(obj).target/net/third_party/nss/libcrssl.a $(obj).target/third_party/zlib/libchrome_zlib.a $(obj).target/sdch/libsdch.a $(obj).target/build/linux/libgio.a $(obj).target/dbus/libdbus.a $(obj).target/third_party/protobuf/libprotobuf_lite.a $(obj).target/third_party/zlib/libminizip.a $(obj).target/ui/libui.a $(obj).target/skia/libskia.a $(obj).target/skia/libskia_opts.a $(obj).target/skia/libskia_opts_ssse3.a $(obj).target/third_party/sfntly/libsfntly.a $(obj).target/third_party/libpng/libpng.a $(obj).target/third_party/libjpeg_turbo/libjpeg_turbo.a $(obj).target/base/libtest_support_base.a $(obj).target/testing/libgmock.a $(obj).target/base/allocator/liballocator.a $(obj).target/net/libnet_with_v8.a $(obj).target/v8/tools/gyp/libv8_base.x64.a $(obj).target/v8/tools/gyp/libv8_snapshot.a FORCE_DO_CMD
	$(call do_cmd,link)

all_deps += $(builddir)/chromedriver2_tests
# Add target alias
.PHONY: chromedriver2_tests
chromedriver2_tests: $(builddir)/chromedriver2_tests

# Add executable to "all" target.
.PHONY: all
all: $(builddir)/chromedriver2_tests

