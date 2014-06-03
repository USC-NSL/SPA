# We borrow heavily from the kernel build setup, though we are simpler since
# we don't have Kconfig tweaking settings on us.

# The implicit make rules have it looking for RCS files, among other things.
# We instead explicitly write all the rules we care about.
# It's even quicker (saves ~200ms) to pass -r on the command line.
MAKEFLAGS=-r

# The source directory tree.
srcdir := .
abs_srcdir := $(abspath $(srcdir))

# The name of the builddir.
builddir_name ?= out

# The V=1 flag on command line makes us verbosely print command lines.
ifdef V
  quiet=
else
  quiet=quiet_
endif

# Specify BUILDTYPE=Release on the command line for a release build.
BUILDTYPE ?= Debug

# Directory all our build output goes into.
# Note that this must be two directories beneath src/ for unit tests to pass,
# as they reach into the src/ directory for data with relative paths.
builddir ?= $(builddir_name)/$(BUILDTYPE)
abs_builddir := $(abspath $(builddir))
depsdir := $(builddir)/.deps

# Object output directory.
obj := $(builddir)/obj
abs_obj := $(abspath $(obj))

# We build up a list of every single one of the targets so we can slurp in the
# generated dependency rule Makefiles in one pass.
all_deps :=

ifneq (,$(filter $(origin CC), undefined default))
  CC = $(abspath third_party/llvm-build/Release+Asserts/bin/clang)
endif
ifneq (,$(filter $(origin CXX), undefined default))
  CXX = $(abspath third_party/llvm-build/Release+Asserts/bin/clang++)
endif
LINK ?= flock $(builddir)/linker.lock $(CXX)
ifneq (,$(filter $(origin CC.host), undefined default))
  CC.host = $(CC)
endif
ifneq (,$(filter $(origin CXX.host), undefined default))
  CXX.host = $(CXX)
endif
LINK.host ?= $(LINK)


# C++ apps need to be linked with g++.
#
# Note: flock is used to seralize linking. Linking is a memory-intensive
# process so running parallel links can often lead to thrashing.  To disable
# the serialization, override LINK via an envrionment variable as follows:
#
#   export LINK=g++
#
# This will allow make to invoke N linker processes as specified in -jN.
LINK ?= flock $(builddir)/linker.lock $(CXX.target)

CC.target ?= $(CC)
CFLAGS.target ?= $(CFLAGS)
CXX.target ?= $(CXX)
CXXFLAGS.target ?= $(CXXFLAGS)
LINK.target ?= $(LINK)
LDFLAGS.target ?= $(LDFLAGS)
AR.target ?= $(AR)

# TODO(evan): move all cross-compilation logic to gyp-time so we don't need
# to replicate this environment fallback in make as well.
CC.host ?= gcc
CFLAGS.host ?=
CXX.host ?= g++
CXXFLAGS.host ?=
LINK.host ?= g++
LDFLAGS.host ?=
AR.host ?= ar

# Define a dir function that can handle spaces.
# http://www.gnu.org/software/make/manual/make.html#Syntax-of-Functions
# "leading spaces cannot appear in the text of the first argument as written.
# These characters can be put into the argument value by variable substitution."
empty :=
space := $(empty) $(empty)

# http://stackoverflow.com/questions/1189781/using-make-dir-or-notdir-on-a-path-with-spaces
replace_spaces = $(subst $(space),?,$1)
unreplace_spaces = $(subst ?,$(space),$1)
dirx = $(call unreplace_spaces,$(dir $(call replace_spaces,$1)))

# Flags to make gcc output dependency info.  Note that you need to be
# careful here to use the flags that ccache and distcc can understand.
# We write to a dep file on the side first and then rename at the end
# so we can't end up with a broken dep file.
depfile = $(depsdir)/$(call replace_spaces,$@).d
DEPFLAGS = -MMD -MF $(depfile).raw

# We have to fixup the deps output in a few ways.
# (1) the file output should mention the proper .o file.
# ccache or distcc lose the path to the target, so we convert a rule of
# the form:
#   foobar.o: DEP1 DEP2
# into
#   path/to/foobar.o: DEP1 DEP2
# (2) we want missing files not to cause us to fail to build.
# We want to rewrite
#   foobar.o: DEP1 DEP2 \
#               DEP3
# to
#   DEP1:
#   DEP2:
#   DEP3:
# so if the files are missing, they're just considered phony rules.
# We have to do some pretty insane escaping to get those backslashes
# and dollar signs past make, the shell, and sed at the same time.
# Doesn't work with spaces, but that's fine: .d files have spaces in
# their names replaced with other characters.
define fixup_dep
# The depfile may not exist if the input file didn't have any #includes.
touch $(depfile).raw
# Fixup path as in (1).
sed -e "s|^$(notdir $@)|$@|" $(depfile).raw >> $(depfile)
# Add extra rules as in (2).
# We remove slashes and replace spaces with new lines;
# remove blank lines;
# delete the first line and append a colon to the remaining lines.
sed -e 's|\\||' -e 'y| |\n|' $(depfile).raw |\
  grep -v '^$$'                             |\
  sed -e 1d -e 's|$$|:|'                     \
    >> $(depfile)
rm $(depfile).raw
endef

# Command definitions:
# - cmd_foo is the actual command to run;
# - quiet_cmd_foo is the brief-output summary of the command.

quiet_cmd_cc = CC($(TOOLSET)) $@
cmd_cc = $(CC.$(TOOLSET)) $(GYP_CFLAGS) $(DEPFLAGS) $(CFLAGS.$(TOOLSET)) -c -o $@ $<

quiet_cmd_cxx = CXX($(TOOLSET)) $@
cmd_cxx = $(CXX.$(TOOLSET)) $(GYP_CXXFLAGS) $(DEPFLAGS) $(CXXFLAGS.$(TOOLSET)) -c -o $@ $<

quiet_cmd_touch = TOUCH $@
cmd_touch = touch $@

quiet_cmd_copy = COPY $@
# send stderr to /dev/null to ignore messages when linking directories.
cmd_copy = ln -f "$<" "$@" 2>/dev/null || (rm -rf "$@" && cp -af "$<" "$@")

quiet_cmd_alink = AR($(TOOLSET)) $@
cmd_alink = rm -f $@ && $(AR.$(TOOLSET)) crs $@ $(filter %.o,$^)

quiet_cmd_alink_thin = AR($(TOOLSET)) $@
cmd_alink_thin = rm -f $@ && $(AR.$(TOOLSET)) crsT $@ $(filter %.o,$^)

# Due to circular dependencies between libraries :(, we wrap the
# special "figure out circular dependencies" flags around the entire
# input list during linking.
quiet_cmd_link = LINK($(TOOLSET)) $@
cmd_link = $(LINK.$(TOOLSET)) $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -o $@ -Wl,--start-group $(LD_INPUTS) -Wl,--end-group $(LIBS)

# We support two kinds of shared objects (.so):
# 1) shared_library, which is just bundling together many dependent libraries
# into a link line.
# 2) loadable_module, which is generating a module intended for dlopen().
#
# They differ only slightly:
# In the former case, we want to package all dependent code into the .so.
# In the latter case, we want to package just the API exposed by the
# outermost module.
# This means shared_library uses --whole-archive, while loadable_module doesn't.
# (Note that --whole-archive is incompatible with the --start-group used in
# normal linking.)

# Other shared-object link notes:
# - Set SONAME to the library filename so our binaries don't reference
# the local, absolute paths used on the link command-line.
quiet_cmd_solink = SOLINK($(TOOLSET)) $@
cmd_solink = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--whole-archive $(LD_INPUTS) -Wl,--no-whole-archive $(LIBS)

quiet_cmd_solink_module = SOLINK_MODULE($(TOOLSET)) $@
cmd_solink_module = $(LINK.$(TOOLSET)) -shared $(GYP_LDFLAGS) $(LDFLAGS.$(TOOLSET)) -Wl,-soname=$(@F) -o $@ -Wl,--start-group $(filter-out FORCE_DO_CMD, $^) -Wl,--end-group $(LIBS)


# Define an escape_quotes function to escape single quotes.
# This allows us to handle quotes properly as long as we always use
# use single quotes and escape_quotes.
escape_quotes = $(subst ','\'',$(1))
# This comment is here just to include a ' to unconfuse syntax highlighting.
# Define an escape_vars function to escape '$' variable syntax.
# This allows us to read/write command lines with shell variables (e.g.
# $LD_LIBRARY_PATH), without triggering make substitution.
escape_vars = $(subst $$,$$$$,$(1))
# Helper that expands to a shell command to echo a string exactly as it is in
# make. This uses printf instead of echo because printf's behaviour with respect
# to escape sequences is more portable than echo's across different shells
# (e.g., dash, bash).
exact_echo = printf '%s\n' '$(call escape_quotes,$(1))'

# Helper to compare the command we're about to run against the command
# we logged the last time we ran the command.  Produces an empty
# string (false) when the commands match.
# Tricky point: Make has no string-equality test function.
# The kernel uses the following, but it seems like it would have false
# positives, where one string reordered its arguments.
#   arg_check = $(strip $(filter-out $(cmd_$(1)), $(cmd_$@)) \
#                       $(filter-out $(cmd_$@), $(cmd_$(1))))
# We instead substitute each for the empty string into the other, and
# say they're equal if both substitutions produce the empty string.
# .d files contain ? instead of spaces, take that into account.
command_changed = $(or $(subst $(cmd_$(1)),,$(cmd_$(call replace_spaces,$@))),\
                       $(subst $(cmd_$(call replace_spaces,$@)),,$(cmd_$(1))))

# Helper that is non-empty when a prerequisite changes.
# Normally make does this implicitly, but we force rules to always run
# so we can check their command lines.
#   $? -- new prerequisites
#   $| -- order-only dependencies
prereq_changed = $(filter-out FORCE_DO_CMD,$(filter-out $|,$?))

# Helper that executes all postbuilds until one fails.
define do_postbuilds
  @E=0;\
  for p in $(POSTBUILDS); do\
    eval $$p;\
    E=$$?;\
    if [ $$E -ne 0 ]; then\
      break;\
    fi;\
  done;\
  if [ $$E -ne 0 ]; then\
    rm -rf "$@";\
    exit $$E;\
  fi
endef

# do_cmd: run a command via the above cmd_foo names, if necessary.
# Should always run for a given target to handle command-line changes.
# Second argument, if non-zero, makes it do asm/C/C++ dependency munging.
# Third argument, if non-zero, makes it do POSTBUILDS processing.
# Note: We intentionally do NOT call dirx for depfile, since it contains ? for
# spaces already and dirx strips the ? characters.
define do_cmd
$(if $(or $(command_changed),$(prereq_changed)),
  @$(call exact_echo,  $($(quiet)cmd_$(1)))
  @mkdir -p "$(call dirx,$@)" "$(dir $(depfile))"
  $(if $(findstring flock,$(word 1,$(cmd_$1))),
    @$(cmd_$(1))
    @echo "  $(quiet_cmd_$(1)): Finished",
    @$(cmd_$(1))
  )
  @$(call exact_echo,$(call escape_vars,cmd_$(call replace_spaces,$@) := $(cmd_$(1)))) > $(depfile)
  @$(if $(2),$(fixup_dep))
  $(if $(and $(3), $(POSTBUILDS)),
    $(call do_postbuilds)
  )
)
endef

# Declare the "all" target first so it is the default,
# even though we don't have the deps yet.
.PHONY: all
all:

# make looks for ways to re-generate included makefiles, but in our case, we
# don't have a direct way. Explicitly telling make that it has nothing to do
# for them makes it go faster.
%.d: ;

# Use FORCE_DO_CMD to force a target to run.  Should be coupled with
# do_cmd.
.PHONY: FORCE_DO_CMD
FORCE_DO_CMD:

TOOLSET := host
# Suffix rules, putting all outputs into $(obj).
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

# Try building from generated source, too.
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

$(obj).$(TOOLSET)/%.o: $(obj)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

TOOLSET := target
# Suffix rules, putting all outputs into $(obj).
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(srcdir)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

# Try building from generated source, too.
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj).$(TOOLSET)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)

$(obj).$(TOOLSET)/%.o: $(obj)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cpp FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.cxx FORCE_DO_CMD
	@$(call do_cmd,cxx,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.S FORCE_DO_CMD
	@$(call do_cmd,cc,1)
$(obj).$(TOOLSET)/%.o: $(obj)/%.s FORCE_DO_CMD
	@$(call do_cmd,cc,1)


ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ash/ash_strings.target.mk)))),)
  include ash/ash_strings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/allocator/allocator.target.mk)))),)
  include base/allocator/allocator.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/allocator/allocator_extension_thunks.host.mk)))),)
  include base/allocator/allocator_extension_thunks.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/allocator/allocator_extension_thunks.target.mk)))),)
  include base/allocator/allocator_extension_thunks.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base.host.mk)))),)
  include base/base.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base.target.mk)))),)
  include base/base.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_i18n.target.mk)))),)
  include base/base_i18n.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_prefs.target.mk)))),)
  include base/base_prefs.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_prefs_test_support.target.mk)))),)
  include base/base_prefs_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_static.host.mk)))),)
  include base/base_static.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_static.target.mk)))),)
  include base/base_static.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_unittests.target.mk)))),)
  include base/base_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_unittests_run.target.mk)))),)
  include base/base_unittests_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/base_untrusted.target.mk)))),)
  include base/base_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/check_example.target.mk)))),)
  include base/check_example.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/run_all_unittests.target.mk)))),)
  include base/run_all_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/symbolize.host.mk)))),)
  include base/symbolize.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/symbolize.target.mk)))),)
  include base/symbolize.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/test_support_base.target.mk)))),)
  include base/test_support_base.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/test_support_perf.target.mk)))),)
  include base/test_support_perf.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/third_party/dynamic_annotations/dynamic_annotations.host.mk)))),)
  include base/third_party/dynamic_annotations/dynamic_annotations.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/third_party/dynamic_annotations/dynamic_annotations.target.mk)))),)
  include base/third_party/dynamic_annotations/dynamic_annotations.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/xdg_mime.host.mk)))),)
  include base/xdg_mime.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,base/xdg_mime.target.mk)))),)
  include base/xdg_mime.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/breakpad_client.target.mk)))),)
  include breakpad/breakpad_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/breakpad_processor_support.target.mk)))),)
  include breakpad/breakpad_processor_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/breakpad_unittests.target.mk)))),)
  include breakpad/breakpad_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/core-2-minidump.target.mk)))),)
  include breakpad/core-2-minidump.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/dump_syms.target.mk)))),)
  include breakpad/dump_syms.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/generate_test_dump.target.mk)))),)
  include breakpad/generate_test_dump.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/linux_dumper_unittest_helper.target.mk)))),)
  include breakpad/linux_dumper_unittest_helper.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/minidump-2-core.target.mk)))),)
  include breakpad/minidump-2-core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/minidump_dump.target.mk)))),)
  include breakpad/minidump_dump.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/minidump_stackwalk.target.mk)))),)
  include breakpad/minidump_stackwalk.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,breakpad/symupload.target.mk)))),)
  include breakpad/symupload.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/All.target.mk)))),)
  include build/All.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/All_syzygy.target.mk)))),)
  include build/All_syzygy.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_2010_builder_tests.target.mk)))),)
  include build/chromium_2010_builder_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_builder_nacl_win_integration.target.mk)))),)
  include build/chromium_builder_nacl_win_integration.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_builder_perf.target.mk)))),)
  include build/chromium_builder_perf.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_builder_perf_av.target.mk)))),)
  include build/chromium_builder_perf_av.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_builder_qa.target.mk)))),)
  include build/chromium_builder_qa.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_builder_tests.target.mk)))),)
  include build/chromium_builder_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_builder_webrtc.target.mk)))),)
  include build/chromium_builder_webrtc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_gpu_builder.target.mk)))),)
  include build/chromium_gpu_builder.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_gpu_debug_builder.target.mk)))),)
  include build/chromium_gpu_debug_builder.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/chromium_swarm_tests.target.mk)))),)
  include build/chromium_swarm_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/dbus.target.mk)))),)
  include build/linux/dbus.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/fontconfig.target.mk)))),)
  include build/linux/fontconfig.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/freetype2.target.mk)))),)
  include build/linux/freetype2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gconf.target.mk)))),)
  include build/linux/gconf.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gdk.target.mk)))),)
  include build/linux/gdk.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gio.target.mk)))),)
  include build/linux/gio.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/glib.host.mk)))),)
  include build/linux/glib.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/glib.target.mk)))),)
  include build/linux/glib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gnome_keyring.target.mk)))),)
  include build/linux/gnome_keyring.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gnome_keyring_direct.target.mk)))),)
  include build/linux/gnome_keyring_direct.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gtk.host.mk)))),)
  include build/linux/gtk.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gtk.target.mk)))),)
  include build/linux/gtk.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/gtkprint.target.mk)))),)
  include build/linux/gtkprint.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/libgcrypt.target.mk)))),)
  include build/linux/libgcrypt.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/libpci.target.mk)))),)
  include build/linux/libpci.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/libresolv.target.mk)))),)
  include build/linux/libresolv.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/libspeechd.target.mk)))),)
  include build/linux/libspeechd.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/pangocairo.host.mk)))),)
  include build/linux/pangocairo.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/pangocairo.target.mk)))),)
  include build/linux/pangocairo.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/selinux.target.mk)))),)
  include build/linux/selinux.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/ssl.target.mk)))),)
  include build/linux/ssl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/udev.target.mk)))),)
  include build/linux/udev.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/x11.host.mk)))),)
  include build/linux/x11.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/x11.target.mk)))),)
  include build/linux/x11.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/xext.target.mk)))),)
  include build/linux/xext.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/linux/xfixes.target.mk)))),)
  include build/linux/xfixes.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/some.target.mk)))),)
  include build/some.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,build/temp_gyp/googleurl.target.mk)))),)
  include build/temp_gyp/googleurl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cc/cc.target.mk)))),)
  include cc/cc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cc/cc_perftests.target.mk)))),)
  include cc/cc_perftests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cc/cc_test_support.target.mk)))),)
  include cc/cc_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cc/cc_test_utils.target.mk)))),)
  include cc/cc_test_utils.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cc/cc_unittests.target.mk)))),)
  include cc/cc_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/about_credits.target.mk)))),)
  include chrome/about_credits.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/app/policy/chrome_settings_proto_generated_compile.target.mk)))),)
  include chrome/app/policy/chrome_settings_proto_generated_compile.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/app/policy/cloud_policy_code_generate.target.mk)))),)
  include chrome/app/policy/cloud_policy_code_generate.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/app/policy/cloud_policy_proto_generated_compile.target.mk)))),)
  include chrome/app/policy/cloud_policy_proto_generated_compile.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/app/policy/policy.target.mk)))),)
  include chrome/app/policy/policy.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/app/policy/policy_test_support.target.mk)))),)
  include chrome/app/policy/policy_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/apps.target.mk)))),)
  include chrome/apps.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/automated_ui_tests.target.mk)))),)
  include chrome/automated_ui_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser.target.mk)))),)
  include chrome/browser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser/nacl_host/test/mock_nacl_gdb.target.mk)))),)
  include chrome/browser/nacl_host/test/mock_nacl_gdb.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser/performance_monitor/performance_monitor.target.mk)))),)
  include chrome/browser/performance_monitor/performance_monitor.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser/search_engines/prepopulated_engines.target.mk)))),)
  include chrome/browser/search_engines/prepopulated_engines.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser_extensions.target.mk)))),)
  include chrome/browser_extensions.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser_tests.target.mk)))),)
  include chrome/browser_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser_tests_run.target.mk)))),)
  include chrome/browser_tests_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser_ui.target.mk)))),)
  include chrome/browser_ui.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/browser_ui_views.target.mk)))),)
  include chrome/browser_ui_views.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/cert_logger_proto.target.mk)))),)
  include chrome/cert_logger_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome.target.mk)))),)
  include chrome/chrome.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome_app_unittests.target.mk)))),)
  include chrome/chrome_app_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome_devtools_lib.target.mk)))),)
  include chrome/chrome_devtools_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome_extra_resources.target.mk)))),)
  include chrome/chrome_extra_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome_resources.target.mk)))),)
  include chrome/chrome_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome_run.target.mk)))),)
  include chrome/chrome_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome_strings.target.mk)))),)
  include chrome/chrome_strings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chrome_unscaled_resources.target.mk)))),)
  include chrome/chrome_unscaled_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver.target.mk)))),)
  include chrome/chromedriver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver2.target.mk)))),)
  include chrome/chromedriver2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver2_lib.target.mk)))),)
  include chrome/chromedriver2_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver2_server.target.mk)))),)
  include chrome/chromedriver2_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver2_server_lib.target.mk)))),)
  include chrome/chromedriver2_server_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver2_tests.target.mk)))),)
  include chrome/chromedriver2_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver2_unittests.target.mk)))),)
  include chrome/chromedriver2_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver_lib.target.mk)))),)
  include chrome/chromedriver_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver_support.target.mk)))),)
  include chrome/chromedriver_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/chromedriver_unittests.target.mk)))),)
  include chrome/chromedriver_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/cloud_policy_proto.target.mk)))),)
  include chrome/cloud_policy_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/common.target.mk)))),)
  include chrome/common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/common/extensions/api/api.target.mk)))),)
  include chrome/common/extensions/api/api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/common_constants.target.mk)))),)
  include chrome/common_constants.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/common_net.target.mk)))),)
  include chrome/common_net.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/common_version.target.mk)))),)
  include chrome/common_version.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/convert_dict.target.mk)))),)
  include chrome/convert_dict.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/convert_dict_lib.target.mk)))),)
  include chrome/convert_dict_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/debugger.target.mk)))),)
  include chrome/debugger.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/default_extensions.target.mk)))),)
  include chrome/default_extensions.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/feedback_proto.target.mk)))),)
  include chrome/feedback_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/flush_cache.target.mk)))),)
  include chrome/flush_cache.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/generate_profile.target.mk)))),)
  include chrome/generate_profile.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/gpu_tests.target.mk)))),)
  include chrome/gpu_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/in_memory_url_index_cache_proto.target.mk)))),)
  include chrome/in_memory_url_index_cache_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/installer_util.target.mk)))),)
  include chrome/installer_util.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/interactive_ui_tests.target.mk)))),)
  include chrome/interactive_ui_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/interactive_ui_tests_run.target.mk)))),)
  include chrome/interactive_ui_tests_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/ipcfuzz.target.mk)))),)
  include chrome/ipcfuzz.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/ipclist.target.mk)))),)
  include chrome/ipclist.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/linux_symbols.target.mk)))),)
  include chrome/linux_symbols.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/metrics_proto.target.mk)))),)
  include chrome/metrics_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/nacl.target.mk)))),)
  include chrome/nacl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/nacl_helper.target.mk)))),)
  include chrome/nacl_helper.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/packed_extra_resources.target.mk)))),)
  include chrome/packed_extra_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/packed_resources.target.mk)))),)
  include chrome/packed_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/perf_tests.target.mk)))),)
  include chrome/perf_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/performance_browser_tests.target.mk)))),)
  include chrome/performance_browser_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/performance_ui_tests.target.mk)))),)
  include chrome/performance_ui_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/platform_locale_settings.target.mk)))),)
  include chrome/platform_locale_settings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/plugin.target.mk)))),)
  include chrome/plugin.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/policy_templates.target.mk)))),)
  include chrome/policy_templates.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/reliability_tests.target.mk)))),)
  include chrome/reliability_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/renderer.target.mk)))),)
  include chrome/renderer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/safe_browsing_proto.target.mk)))),)
  include chrome/safe_browsing_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/safe_browsing_report_proto.target.mk)))),)
  include chrome/safe_browsing_report_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/service.target.mk)))),)
  include chrome/service.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/soft_gpu_tests.target.mk)))),)
  include chrome/soft_gpu_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/sync_file_system_proto.target.mk)))),)
  include chrome/sync_file_system_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/sync_integration_tests.target.mk)))),)
  include chrome/sync_integration_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/sync_integration_tests_run.target.mk)))),)
  include chrome/sync_integration_tests_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/sync_performance_tests.target.mk)))),)
  include chrome/sync_performance_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/exit_status_test.target.mk)))),)
  include chrome/test/data/nacl/exit_status_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/pnacl_dyncode_syscall_disabled_test.target.mk)))),)
  include chrome/test/data/nacl/pnacl_dyncode_syscall_disabled_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/pnacl_error_handling_test.target.mk)))),)
  include chrome/test/data/nacl/pnacl_error_handling_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/pnacl_options_test.target.mk)))),)
  include chrome/test/data/nacl/pnacl_options_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/ppapi_ppb_core.target.mk)))),)
  include chrome/test/data/nacl/ppapi_ppb_core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/ppapi_progress_events.target.mk)))),)
  include chrome/test/data/nacl/ppapi_progress_events.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/ppapi_test_lib.target.mk)))),)
  include chrome/test/data/nacl/ppapi_test_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/shared_test_files.target.mk)))),)
  include chrome/test/data/nacl/shared_test_files.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test/data/nacl/simple_test.target.mk)))),)
  include chrome/test/data/nacl/simple_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test_support_common.target.mk)))),)
  include chrome/test_support_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test_support_ui.target.mk)))),)
  include chrome/test_support_ui.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/test_support_unit.target.mk)))),)
  include chrome/test_support_unit.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/theme_resources.target.mk)))),)
  include chrome/theme_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/theme_resources_gen.target.mk)))),)
  include chrome/theme_resources_gen.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/unit_tests.target.mk)))),)
  include chrome/unit_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/unit_tests_run.target.mk)))),)
  include chrome/unit_tests_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/utility.target.mk)))),)
  include chrome/utility.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,chrome/variations_seed_proto.target.mk)))),)
  include chrome/variations_seed_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/cloud_print.target.mk)))),)
  include cloud_print/cloud_print.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/cloud_print_unittests.target.mk)))),)
  include cloud_print/cloud_print_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/cloud_print_version_resources.target.mk)))),)
  include cloud_print/cloud_print_version_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/common/cloud_print_install_lib.target.mk)))),)
  include cloud_print/common/cloud_print_install_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/service/cloud_print_service.target.mk)))),)
  include cloud_print/service/cloud_print_service.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/service/cloud_print_service_config.target.mk)))),)
  include cloud_print/service/cloud_print_service_config.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/service/cloud_print_service_lib.target.mk)))),)
  include cloud_print/service/cloud_print_service_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/service/cloud_print_service_setup.target.mk)))),)
  include cloud_print/service/cloud_print_service_setup.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,cloud_print/service/service_resources.target.mk)))),)
  include cloud_print/service/service_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/auto_login_parser.target.mk)))),)
  include components/auto_login_parser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/autofill_browser.target.mk)))),)
  include components/autofill_browser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/autofill_common.target.mk)))),)
  include components/autofill_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/autofill_regexes.target.mk)))),)
  include components/autofill_regexes.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/autofill_renderer.target.mk)))),)
  include components/autofill_renderer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/autofill_risk_proto.target.mk)))),)
  include components/autofill_risk_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/components_perftests.target.mk)))),)
  include components/components_perftests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/components_unittests.target.mk)))),)
  include components/components_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/encryptor.target.mk)))),)
  include components/encryptor.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/navigation_interception.target.mk)))),)
  include components/navigation_interception.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/sessions.target.mk)))),)
  include components/sessions.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/sessions_test_support.target.mk)))),)
  include components/sessions_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/tracing.target.mk)))),)
  include components/tracing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/tracing_untrusted.target.mk)))),)
  include components/tracing_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/user_prefs.target.mk)))),)
  include components/user_prefs.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/visitedlink_browser.target.mk)))),)
  include components/visitedlink_browser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/visitedlink_common.target.mk)))),)
  include components/visitedlink_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/visitedlink_renderer.target.mk)))),)
  include components/visitedlink_renderer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/webdata_common.target.mk)))),)
  include components/webdata_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,components/zip.target.mk)))),)
  include components/zip.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/browser/devtools/devtools_resources.target.mk)))),)
  include content/browser/devtools/devtools_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/browser/speech/proto/speech_proto.target.mk)))),)
  include content/browser/speech/proto/speech_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content.target.mk)))),)
  include content/content.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_app.target.mk)))),)
  include content/content_app.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_browser.target.mk)))),)
  include content/content_browser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_browsertests.target.mk)))),)
  include content/content_browsertests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_common.target.mk)))),)
  include content/content_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_gpu.target.mk)))),)
  include content/content_gpu.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_plugin.target.mk)))),)
  include content/content_plugin.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_ppapi_plugin.target.mk)))),)
  include content/content_ppapi_plugin.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_renderer.target.mk)))),)
  include content/content_renderer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_resources.target.mk)))),)
  include content/content_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_shell.target.mk)))),)
  include content/content_shell.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_shell_builder.target.mk)))),)
  include content/content_shell_builder.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_shell_lib.target.mk)))),)
  include content/content_shell_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_shell_pak.target.mk)))),)
  include content/content_shell_pak.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_shell_resources.target.mk)))),)
  include content/content_shell_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_unittests.target.mk)))),)
  include content/content_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_utility.target.mk)))),)
  include content/content_utility.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/content_worker.target.mk)))),)
  include content/content_worker.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/generate_content_resources.target.mk)))),)
  include content/generate_content_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/generate_content_shell_resources.target.mk)))),)
  include content/generate_content_shell_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/h264_parser_unittest.target.mk)))),)
  include content/h264_parser_unittest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,content/test_support_content.target.mk)))),)
  include content/test_support_content.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,courgette/courgette.target.mk)))),)
  include courgette/courgette.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,courgette/courgette_fuzz.target.mk)))),)
  include courgette/courgette_fuzz.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,courgette/courgette_lib.target.mk)))),)
  include courgette/courgette_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,courgette/courgette_minimal_tool.target.mk)))),)
  include courgette/courgette_minimal_tool.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,courgette/courgette_unittests.target.mk)))),)
  include courgette/courgette_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,crypto/crypto.target.mk)))),)
  include crypto/crypto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,crypto/crypto_unittests.target.mk)))),)
  include crypto/crypto_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,dbus/dbus.target.mk)))),)
  include dbus/dbus.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,dbus/dbus_test_proto.target.mk)))),)
  include dbus/dbus_test_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,dbus/dbus_test_server.target.mk)))),)
  include dbus/dbus_test_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,dbus/dbus_test_support.target.mk)))),)
  include dbus/dbus_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,dbus/dbus_unittests.target.mk)))),)
  include dbus/dbus_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/device_bluetooth.target.mk)))),)
  include device/device_bluetooth.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/device_bluetooth_mocks.target.mk)))),)
  include device/device_bluetooth_mocks.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/device_bluetooth_strings.target.mk)))),)
  include device/device_bluetooth_strings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/device_media_transfer_protocol.target.mk)))),)
  include device/device_media_transfer_protocol.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/device_unittests.target.mk)))),)
  include device/device_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/device_usb.target.mk)))),)
  include device/device_usb.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/mtp_file_entry_proto.target.mk)))),)
  include device/mtp_file_entry_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,device/mtp_storage_info_proto.target.mk)))),)
  include device/mtp_storage_info_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,google_apis/google_apis.target.mk)))),)
  include google_apis/google_apis.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/command_buffer/gles2_utils.target.mk)))),)
  include gpu/command_buffer/gles2_utils.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/command_buffer/gles2_utils_untrusted.target.mk)))),)
  include gpu/command_buffer/gles2_utils_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/command_buffer_client.target.mk)))),)
  include gpu/command_buffer_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/command_buffer_client_untrusted.target.mk)))),)
  include gpu/command_buffer_client_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/command_buffer_common.target.mk)))),)
  include gpu/command_buffer_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/command_buffer_common_untrusted.target.mk)))),)
  include gpu/command_buffer_common_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/command_buffer_service.target.mk)))),)
  include gpu/command_buffer_service.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/disk_cache_proto.target.mk)))),)
  include gpu/disk_cache_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gl_tests.target.mk)))),)
  include gpu/gl_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_c_lib.target.mk)))),)
  include gpu/gles2_c_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_c_lib_nocheck.target.mk)))),)
  include gpu/gles2_c_lib_nocheck.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_cmd_helper.target.mk)))),)
  include gpu/gles2_cmd_helper.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_cmd_helper_untrusted.target.mk)))),)
  include gpu/gles2_cmd_helper_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_conform_support/egl_main_native.target.mk)))),)
  include gpu/gles2_conform_support/egl_main_native.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_conform_support/egl_main_windowless.target.mk)))),)
  include gpu/gles2_conform_support/egl_main_windowless.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_conform_support/egl_native.target.mk)))),)
  include gpu/gles2_conform_support/egl_native.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_conform_support/gles2_conform_support.target.mk)))),)
  include gpu/gles2_conform_support/gles2_conform_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_implementation.target.mk)))),)
  include gpu/gles2_implementation.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_implementation_client_side_arrays.target.mk)))),)
  include gpu/gles2_implementation_client_side_arrays.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_implementation_client_side_arrays_no_check.target.mk)))),)
  include gpu/gles2_implementation_client_side_arrays_no_check.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gles2_implementation_untrusted.target.mk)))),)
  include gpu/gles2_implementation_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gpu.target.mk)))),)
  include gpu/gpu.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gpu_ipc.target.mk)))),)
  include gpu/gpu_ipc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gpu_ipc_untrusted.target.mk)))),)
  include gpu/gpu_ipc_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gpu_unittest_utils.target.mk)))),)
  include gpu/gpu_unittest_utils.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/gpu_unittests.target.mk)))),)
  include gpu/gpu_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,gpu/tools/compositor_model_bench.target.mk)))),)
  include gpu/tools/compositor_model_bench.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ipc/ipc.target.mk)))),)
  include ipc/ipc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ipc/ipc_perftests.target.mk)))),)
  include ipc/ipc_perftests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ipc/ipc_tests.target.mk)))),)
  include ipc/ipc_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ipc/ipc_untrusted.target.mk)))),)
  include ipc/ipc_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ipc/test_support_ipc.target.mk)))),)
  include ipc/test_support_ipc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,jingle/jingle_glue.target.mk)))),)
  include jingle/jingle_glue.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,jingle/jingle_glue_test_util.target.mk)))),)
  include jingle/jingle_glue_test_util.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,jingle/jingle_unittests.target.mk)))),)
  include jingle/jingle_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,jingle/notifier.target.mk)))),)
  include jingle/notifier.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,jingle/notifier_test_util.target.mk)))),)
  include jingle/notifier_test_util.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/demuxer_bench.target.mk)))),)
  include media/demuxer_bench.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/differ_block_sse2.target.mk)))),)
  include media/differ_block_sse2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/ffmpeg_regression_tests.target.mk)))),)
  include media/ffmpeg_regression_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/ffmpeg_tests.target.mk)))),)
  include media/ffmpeg_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/ffmpeg_unittests.target.mk)))),)
  include media/ffmpeg_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/media.target.mk)))),)
  include media/media.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/media_bench.target.mk)))),)
  include media/media_bench.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/media_sse.target.mk)))),)
  include media/media_sse.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/media_test_support.target.mk)))),)
  include media/media_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/media_unittests.target.mk)))),)
  include media/media_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/player_x11.target.mk)))),)
  include media/player_x11.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/seek_tester.target.mk)))),)
  include media/seek_tester.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/shader_bench.target.mk)))),)
  include media/shader_bench.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/shared_memory_support.target.mk)))),)
  include media/shared_memory_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/shared_memory_support_untrusted.target.mk)))),)
  include media/shared_memory_support_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/yuv_convert.target.mk)))),)
  include media/yuv_convert.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/yuv_convert_simd_c.target.mk)))),)
  include media/yuv_convert_simd_c.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,media/yuv_convert_simd_x86.target.mk)))),)
  include media/yuv_convert_simd_x86.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/crt_fini_32.target.mk)))),)
  include native_client/crt_fini_32.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/crt_fini_64.target.mk)))),)
  include native_client/crt_fini_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/crt_init_32.target.mk)))),)
  include native_client/crt_init_32.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/crt_init_64.target.mk)))),)
  include native_client/crt_init_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/prep_nacl_sdk.target.mk)))),)
  include native_client/prep_nacl_sdk.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/prep_toolchain.target.mk)))),)
  include native_client/prep_toolchain.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/gio/gio.target.mk)))),)
  include native_client/src/shared/gio/gio.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/gio/gio_lib.target.mk)))),)
  include native_client/src/shared/gio/gio_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/imc/imc.target.mk)))),)
  include native_client/src/shared/imc/imc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/imc/imc_lib.target.mk)))),)
  include native_client/src/shared/imc/imc_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/imc/sigpipe_test.target.mk)))),)
  include native_client/src/shared/imc/sigpipe_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/platform/platform.target.mk)))),)
  include native_client/src/shared/platform/platform.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/platform/platform_lib.target.mk)))),)
  include native_client/src/shared/platform/platform_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/platform/platform_tests.target.mk)))),)
  include native_client/src/shared/platform/platform_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/serialization/serialization.target.mk)))),)
  include native_client/src/shared/serialization/serialization.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/srpc/nonnacl_srpc.target.mk)))),)
  include native_client/src/shared/srpc/nonnacl_srpc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/shared/srpc/srpc_lib.target.mk)))),)
  include native_client/src/shared/srpc/srpc_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/cpu_features/cpu_features.target.mk)))),)
  include native_client/src/trusted/cpu_features/cpu_features.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/debug_stub/debug_stub.target.mk)))),)
  include native_client/src/trusted/debug_stub/debug_stub.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/debug_stub/gdb_rsp_test.target.mk)))),)
  include native_client/src/trusted/debug_stub/gdb_rsp_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/desc/desc_wrapper.target.mk)))),)
  include native_client/src/trusted/desc/desc_wrapper.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/desc/nrd_xfer.target.mk)))),)
  include native_client/src/trusted/desc/nrd_xfer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/fault_injection/nacl_fault_inject.target.mk)))),)
  include native_client/src/trusted/fault_injection/nacl_fault_inject.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/gio/gio_wrapped_desc.target.mk)))),)
  include native_client/src/trusted/gio/gio_wrapped_desc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/interval_multiset/nacl_interval.target.mk)))),)
  include native_client/src/trusted/interval_multiset/nacl_interval.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/manifest_name_service_proxy/manifest_proxy.target.mk)))),)
  include native_client/src/trusted/manifest_name_service_proxy/manifest_proxy.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/nacl_base/nacl_base.target.mk)))),)
  include native_client/src/trusted/nacl_base/nacl_base.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/nonnacl_util/nonnacl_util.target.mk)))),)
  include native_client/src/trusted/nonnacl_util/nonnacl_util.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/nonnacl_util/sel_ldr_launcher.target.mk)))),)
  include native_client/src/trusted/nonnacl_util/sel_ldr_launcher.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/perf_counter/nacl_perf_counter.target.mk)))),)
  include native_client/src/trusted/perf_counter/nacl_perf_counter.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/platform_qualify/platform_qual_lib.target.mk)))),)
  include native_client/src/trusted/platform_qualify/platform_qual_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/reverse_service/reverse_service.target.mk)))),)
  include native_client/src/trusted/reverse_service/reverse_service.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/arch/x86/service_runtime_x86_common.target.mk)))),)
  include native_client/src/trusted/service_runtime/arch/x86/service_runtime_x86_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/arch/x86_64/service_runtime_x86_64.target.mk)))),)
  include native_client/src/trusted/service_runtime/arch/x86_64/service_runtime_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/env_cleanser.target.mk)))),)
  include native_client/src/trusted/service_runtime/env_cleanser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/linux/nacl_bootstrap_lib.target.mk)))),)
  include native_client/src/trusted/service_runtime/linux/nacl_bootstrap_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/linux/nacl_bootstrap_munge_phdr.host.mk)))),)
  include native_client/src/trusted/service_runtime/linux/nacl_bootstrap_munge_phdr.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/linux/nacl_bootstrap_raw.target.mk)))),)
  include native_client/src/trusted/service_runtime/linux/nacl_bootstrap_raw.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/linux/nacl_helper_bootstrap.target.mk)))),)
  include native_client/src/trusted/service_runtime/linux/nacl_helper_bootstrap.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/nacl_error_code.target.mk)))),)
  include native_client/src/trusted/service_runtime/nacl_error_code.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/sel.target.mk)))),)
  include native_client/src/trusted/service_runtime/sel.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/sel_ldr.target.mk)))),)
  include native_client/src/trusted/service_runtime/sel_ldr.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/sel_main.target.mk)))),)
  include native_client/src/trusted/service_runtime/sel_main.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/service_runtime/sel_main_chrome.target.mk)))),)
  include native_client/src/trusted/service_runtime/sel_main_chrome.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/simple_service/simple_service.target.mk)))),)
  include native_client/src/trusted/simple_service/simple_service.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/threading/thread_interface.target.mk)))),)
  include native_client/src/trusted/threading/thread_interface.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/validation_cache.target.mk)))),)
  include native_client/src/trusted/validator/validation_cache.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/validators.target.mk)))),)
  include native_client/src/trusted/validator/validators.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/64/ncvalidate_verbose_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/64/ncvalidate_verbose_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/64/ncvalidate_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/64/ncvalidate_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/decoder/nc_decoder_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/decoder/nc_decoder_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/decoder/nc_opcode_modeling_verbose_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/decoder/nc_opcode_modeling_verbose_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/decoder/nc_opcode_modeling_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/decoder/nc_opcode_modeling_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/ncval_base_verbose_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/ncval_base_verbose_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/ncval_base_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/ncval_base_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi_verbose_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi_verbose_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi_x86_64.target.mk)))),)
  include native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator_ragel/dfa_validate_x86_64.target.mk)))),)
  include native_client/src/trusted/validator_ragel/dfa_validate_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/validator_x86/nccopy_x86_64.target.mk)))),)
  include native_client/src/trusted/validator_x86/nccopy_x86_64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/weak_ref/weak_ref.target.mk)))),)
  include native_client/src/trusted/weak_ref/weak_ref.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/trusted/weak_ref/weak_ref_lib.target.mk)))),)
  include native_client/src/trusted/weak_ref/weak_ref_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/irt/irt_browser_lib.target.mk)))),)
  include native_client/src/untrusted/irt/irt_browser_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/irt/irt_core_nexe.target.mk)))),)
  include native_client/src/untrusted/irt/irt_core_nexe.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/imc_syscalls_lib.target.mk)))),)
  include native_client/src/untrusted/nacl/imc_syscalls_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/nacl_dynacode_lib.target.mk)))),)
  include native_client/src/untrusted/nacl/nacl_dynacode_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/nacl_dynacode_private_lib.target.mk)))),)
  include native_client/src/untrusted/nacl/nacl_dynacode_private_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/nacl_lib.target.mk)))),)
  include native_client/src/untrusted/nacl/nacl_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/nacl_lib_glibc.target.mk)))),)
  include native_client/src/untrusted/nacl/nacl_lib_glibc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/nacl_lib_newlib.target.mk)))),)
  include native_client/src/untrusted/nacl/nacl_lib_newlib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/nacl_list_mappings_lib.target.mk)))),)
  include native_client/src/untrusted/nacl/nacl_list_mappings_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/nacl/nacl_list_mappings_private_lib.target.mk)))),)
  include native_client/src/untrusted/nacl/nacl_list_mappings_private_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/pthread/pthread_lib.target.mk)))),)
  include native_client/src/untrusted/pthread/pthread_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/src/untrusted/pthread/pthread_private_lib.target.mk)))),)
  include native_client/src/untrusted/pthread/pthread_private_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,native_client/untar_toolchains.target.mk)))),)
  include native_client/untar_toolchains.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/crash_cache.target.mk)))),)
  include net/crash_cache.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/crl_set_dump.target.mk)))),)
  include net/crl_set_dump.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/dns_fuzz_stub.target.mk)))),)
  include net/dns_fuzz_stub.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/dump_cache.target.mk)))),)
  include net/dump_cache.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/fetch_client.target.mk)))),)
  include net/fetch_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/fetch_server.target.mk)))),)
  include net/fetch_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/flip_balsa_and_epoll_library.target.mk)))),)
  include net/flip_balsa_and_epoll_library.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/flip_in_mem_edsm_server.target.mk)))),)
  include net/flip_in_mem_edsm_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/gdig.target.mk)))),)
  include net/gdig.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/get_server_time.target.mk)))),)
  include net/get_server_time.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/http_server.target.mk)))),)
  include net/http_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net.target.mk)))),)
  include net/net.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net_perftests.target.mk)))),)
  include net/net_perftests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net_resources.target.mk)))),)
  include net/net_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net_test_support.target.mk)))),)
  include net/net_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net_unittests.target.mk)))),)
  include net/net_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net_unittests_run.target.mk)))),)
  include net/net_unittests_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net_watcher.target.mk)))),)
  include net/net_watcher.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/net_with_v8.target.mk)))),)
  include net/net_with_v8.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/quic_client.target.mk)))),)
  include net/quic_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/quic_library.target.mk)))),)
  include net/quic_library.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/quic_server.target.mk)))),)
  include net/quic_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/quic_unittests.target.mk)))),)
  include net/quic_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/run_testserver.target.mk)))),)
  include net/run_testserver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/stress_cache.target.mk)))),)
  include net/stress_cache.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/third_party/nss/libssl.target.mk)))),)
  include net/third_party/nss/libssl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,net/tld_cleanup.target.mk)))),)
  include net/tld_cleanup.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/native_client/nacl_irt.target.mk)))),)
  include ppapi/native_client/nacl_irt.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/native_client/ppapi_lib.target.mk)))),)
  include ppapi/native_client/ppapi_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/native_client/src/trusted/plugin/ppGoogleNaClPluginChrome.target.mk)))),)
  include ppapi/native_client/src/trusted/plugin/ppGoogleNaClPluginChrome.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/native_client/src/untrusted/irt_stub/ppapi_stub_lib.target.mk)))),)
  include ppapi/native_client/src/untrusted/irt_stub/ppapi_stub_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.target.mk)))),)
  include ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/native_client/src/untrusted/pnacl_support_extension/pnacl_support_extension.target.mk)))),)
  include ppapi/native_client/src/untrusted/pnacl_support_extension/pnacl_support_extension.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_c.target.mk)))),)
  include ppapi/ppapi_c.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_cpp.target.mk)))),)
  include ppapi/ppapi_cpp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_cpp_lib.target.mk)))),)
  include ppapi/ppapi_cpp_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_cpp_objects.target.mk)))),)
  include ppapi/ppapi_cpp_objects.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_egl.target.mk)))),)
  include ppapi/ppapi_egl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_audio.target.mk)))),)
  include ppapi/ppapi_example_audio.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_audio_input.target.mk)))),)
  include ppapi/ppapi_example_audio_input.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_c_stub.target.mk)))),)
  include ppapi/ppapi_example_c_stub.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_cc_stub.target.mk)))),)
  include ppapi/ppapi_example_cc_stub.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_enumerate_devices.target.mk)))),)
  include ppapi/ppapi_example_enumerate_devices.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_file_chooser.target.mk)))),)
  include ppapi/ppapi_example_file_chooser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_flash_topmost.target.mk)))),)
  include ppapi/ppapi_example_flash_topmost.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_gamepad.target.mk)))),)
  include ppapi/ppapi_example_gamepad.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_gles2.target.mk)))),)
  include ppapi/ppapi_example_gles2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_graphics_2d.target.mk)))),)
  include ppapi/ppapi_example_graphics_2d.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_ime.target.mk)))),)
  include ppapi/ppapi_example_ime.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_input.target.mk)))),)
  include ppapi/ppapi_example_input.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_mouse_cursor.target.mk)))),)
  include ppapi/ppapi_example_mouse_cursor.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_mouse_lock.target.mk)))),)
  include ppapi/ppapi_example_mouse_lock.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_paint_manager.target.mk)))),)
  include ppapi/ppapi_example_paint_manager.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_post_message.target.mk)))),)
  include ppapi/ppapi_example_post_message.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_printing.target.mk)))),)
  include ppapi/ppapi_example_printing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_scaling.target.mk)))),)
  include ppapi/ppapi_example_scaling.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_scroll.target.mk)))),)
  include ppapi/ppapi_example_scroll.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_simple_font.target.mk)))),)
  include ppapi/ppapi_example_simple_font.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_skeleton.target.mk)))),)
  include ppapi/ppapi_example_skeleton.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_url_loader.target.mk)))),)
  include ppapi/ppapi_example_url_loader.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_url_loader_file.target.mk)))),)
  include ppapi/ppapi_example_url_loader_file.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_vc.target.mk)))),)
  include ppapi/ppapi_example_vc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_example_video_decode.target.mk)))),)
  include ppapi/ppapi_example_video_decode.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_gles2.target.mk)))),)
  include ppapi/ppapi_gles2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_gles2_lib.target.mk)))),)
  include ppapi/ppapi_gles2_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_host.target.mk)))),)
  include ppapi/ppapi_host.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_ipc.target.mk)))),)
  include ppapi/ppapi_ipc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_ipc_untrusted.target.mk)))),)
  include ppapi/ppapi_ipc_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_nacl_tests.target.mk)))),)
  include ppapi/ppapi_nacl_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_perftests.target.mk)))),)
  include ppapi/ppapi_perftests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_proxy.target.mk)))),)
  include ppapi/ppapi_proxy.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_proxy_untrusted.target.mk)))),)
  include ppapi/ppapi_proxy_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_shared.target.mk)))),)
  include ppapi/ppapi_shared.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_shared_untrusted.target.mk)))),)
  include ppapi/ppapi_shared_untrusted.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_tests.target.mk)))),)
  include ppapi/ppapi_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_unittest_shared.target.mk)))),)
  include ppapi/ppapi_unittest_shared.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ppapi/ppapi_unittests.target.mk)))),)
  include ppapi/ppapi_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,printing/cups.target.mk)))),)
  include printing/cups.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,printing/printing.target.mk)))),)
  include printing/printing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,printing/printing_unittests.target.mk)))),)
  include printing/printing_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/proto/chromotocol_proto_lib.target.mk)))),)
  include remoting/proto/chromotocol_proto_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_base.target.mk)))),)
  include remoting/remoting_base.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_breakpad.target.mk)))),)
  include remoting/remoting_breakpad.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_client.target.mk)))),)
  include remoting/remoting_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_client_plugin.target.mk)))),)
  include remoting/remoting_client_plugin.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_host.target.mk)))),)
  include remoting/remoting_host.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_host_event_logger.target.mk)))),)
  include remoting/remoting_host_event_logger.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_host_keygen.target.mk)))),)
  include remoting/remoting_host_keygen.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_host_logging.target.mk)))),)
  include remoting/remoting_host_logging.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_host_plugin.target.mk)))),)
  include remoting/remoting_host_plugin.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_host_setup_base.target.mk)))),)
  include remoting/remoting_host_setup_base.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_jingle_glue.target.mk)))),)
  include remoting/remoting_jingle_glue.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_linux_symbols.target.mk)))),)
  include remoting/remoting_linux_symbols.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_me2me_host.target.mk)))),)
  include remoting/remoting_me2me_host.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_me2me_host_static.target.mk)))),)
  include remoting/remoting_me2me_host_static.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_protocol.target.mk)))),)
  include remoting/remoting_protocol.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_resources.target.mk)))),)
  include remoting/remoting_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_start_host.target.mk)))),)
  include remoting/remoting_start_host.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_unittests.target.mk)))),)
  include remoting/remoting_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,remoting/remoting_webapp.target.mk)))),)
  include remoting/remoting_webapp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/chrome_sandbox.target.mk)))),)
  include sandbox/chrome_sandbox.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/libc_urandom_override.target.mk)))),)
  include sandbox/libc_urandom_override.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/sandbox.target.mk)))),)
  include sandbox/sandbox.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/sandbox_linux_jni_unittests.target.mk)))),)
  include sandbox/sandbox_linux_jni_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/sandbox_linux_unittests.target.mk)))),)
  include sandbox/sandbox_linux_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/sandbox_services.target.mk)))),)
  include sandbox/sandbox_services.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/sandbox_services_headers.target.mk)))),)
  include sandbox/sandbox_services_headers.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/seccomp_bpf.target.mk)))),)
  include sandbox/seccomp_bpf.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sandbox/suid_sandbox_client.target.mk)))),)
  include sandbox/suid_sandbox_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sdch/sdch.target.mk)))),)
  include sdch/sdch.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,skia/image_operations_bench.target.mk)))),)
  include skia/image_operations_bench.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,skia/skia.target.mk)))),)
  include skia/skia.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,skia/skia_opts.target.mk)))),)
  include skia/skia_opts.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,skia/skia_opts_ssse3.target.mk)))),)
  include skia/skia_opts_ssse3.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sql/sql.target.mk)))),)
  include sql/sql.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sql/sql_unittests.target.mk)))),)
  include sql/sql_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/run_sync_testserver.target.mk)))),)
  include sync/run_sync_testserver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync.target.mk)))),)
  include sync/sync.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_api.target.mk)))),)
  include sync/sync_api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_api_tests.target.mk)))),)
  include sync/sync_api_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_client.target.mk)))),)
  include sync/sync_client.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_core.target.mk)))),)
  include sync/sync_core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_core_tests.target.mk)))),)
  include sync/sync_core_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_internal_api.target.mk)))),)
  include sync/sync_internal_api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_internal_api_tests.target.mk)))),)
  include sync/sync_internal_api_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_listen_notifications.target.mk)))),)
  include sync/sync_listen_notifications.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_notifier.target.mk)))),)
  include sync/sync_notifier.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_notifier_tests.target.mk)))),)
  include sync/sync_notifier_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_proto.target.mk)))),)
  include sync/sync_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_tools_helper.target.mk)))),)
  include sync/sync_tools_helper.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/sync_unit_tests.target.mk)))),)
  include sync/sync_unit_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/test_support_sync_api.target.mk)))),)
  include sync/test_support_sync_api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/test_support_sync_core.target.mk)))),)
  include sync/test_support_sync_core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/test_support_sync_internal_api.target.mk)))),)
  include sync/test_support_sync_internal_api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/test_support_sync_notifier.target.mk)))),)
  include sync/test_support_sync_notifier.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,sync/test_support_sync_testserver.target.mk)))),)
  include sync/test_support_sync_testserver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gmock.target.mk)))),)
  include testing/gmock.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gmock_main.target.mk)))),)
  include testing/gmock_main.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest.target.mk)))),)
  include testing/gtest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest_main.target.mk)))),)
  include testing/gtest_main.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest_prod.host.mk)))),)
  include testing/gtest_prod.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,testing/gtest_prod.target.mk)))),)
  include testing/gtest_prod.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/Platform/Platform.gyp/webkit_platform.target.mk)))),)
  include third_party/WebKit/Source/Platform/Platform.gyp/webkit_platform.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/WebKit/chromium/all_webkit.target.mk)))),)
  include third_party/WebKit/Source/WebKit/chromium/all_webkit.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/WebKit/chromium/skia_webkit.target.mk)))),)
  include third_party/WebKit/Source/WebKit/chromium/skia_webkit.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/WebKit/chromium/webkit.target.mk)))),)
  include third_party/WebKit/Source/WebKit/chromium/webkit.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/WebKit/chromium/webkit_test_support.target.mk)))),)
  include third_party/WebKit/Source/WebKit/chromium/webkit_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/WebKit/chromium/webkit_unit_tests.target.mk)))),)
  include third_party/WebKit/Source/WebKit/chromium/webkit_unit_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/WebKit/chromium/webkit_wtf_support.target.mk)))),)
  include third_party/WebKit/Source/WebKit/chromium/webkit_wtf_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/bindings/bindings_derived_sources.target.mk)))),)
  include third_party/WebKit/Source/bindings/bindings_derived_sources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/bindings/supplemental_dependencies.target.mk)))),)
  include third_party/WebKit/Source/bindings/supplemental_dependencies.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/config.target.mk)))),)
  include third_party/WebKit/Source/config.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/debugger_script_source.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/debugger_script_source.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/generate_inspector_protocol_version.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/generate_inspector_protocol_version.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/generate_settings.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/generate_settings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/injected_canvas_script_source.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/injected_canvas_script_source.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/injected_script_source.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/injected_script_source.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/inspector_overlay_page.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/inspector_overlay_page.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/inspector_protocol_sources.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/inspector_protocol_sources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/make_derived_sources.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/make_derived_sources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_arm_neon.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_arm_neon.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_derived.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_derived.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_dom.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_dom.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_html.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_html.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_platform.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_platform.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_platform_geometry.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_platform_geometry.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_prerequisites.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_prerequisites.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_remaining.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_remaining.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_rendering.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_rendering.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_svg.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_svg.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/core/core.gyp/webcore_test_support.target.mk)))),)
  include third_party/WebKit/Source/core/core.gyp/webcore_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_audits_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_audits_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_codemirror_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_codemirror_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_css.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_css.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_elements_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_elements_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_network_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_network_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_profiles_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_profiles_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_resources_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_resources_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_scripts_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_scripts_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_devtools_timeline_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_devtools_timeline_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_heap_snapshot_worker_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_heap_snapshot_worker_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/concatenated_script_formatter_worker_js.target.mk)))),)
  include third_party/WebKit/Source/devtools/concatenated_script_formatter_worker_js.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/devtools_extension_api.target.mk)))),)
  include third_party/WebKit/Source/devtools/devtools_extension_api.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/devtools_frontend_resources.target.mk)))),)
  include third_party/WebKit/Source/devtools/devtools_frontend_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/devtools_html.target.mk)))),)
  include third_party/WebKit/Source/devtools/devtools_html.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/frontend_protocol_sources.target.mk)))),)
  include third_party/WebKit/Source/devtools/frontend_protocol_sources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/devtools/generate_devtools_grd.target.mk)))),)
  include third_party/WebKit/Source/devtools/generate_devtools_grd.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/modules/modules.target.mk)))),)
  include third_party/WebKit/Source/modules/modules.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/wtf/wtf.target.mk)))),)
  include third_party/WebKit/Source/wtf/wtf.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Source/wtf/wtf_config.target.mk)))),)
  include third_party/WebKit/Source/wtf/wtf_config.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.target.mk)))),)
  include third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree_resources.target.mk)))),)
  include third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/ImageDiff.target.mk)))),)
  include third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/ImageDiff.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/TestNetscapePlugIn.target.mk)))),)
  include third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/TestNetscapePlugIn.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/TestRunner.target.mk)))),)
  include third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/TestRunner.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/TestRunner_resources.target.mk)))),)
  include third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/TestRunner_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/copy_TestNetscapePlugIn.target.mk)))),)
  include third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/copy_TestNetscapePlugIn.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/adobe/flash/flapper_binaries.target.mk)))),)
  include third_party/adobe/flash/flapper_binaries.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/adobe/flash/flapper_version_h.target.mk)))),)
  include third_party/adobe/flash/flapper_version_h.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/angle/src/preprocessor.target.mk)))),)
  include third_party/angle/src/preprocessor.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/angle/src/translator_common.target.mk)))),)
  include third_party/angle/src/translator_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/angle/src/translator_glsl.target.mk)))),)
  include third_party/angle/src/translator_glsl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/bzip2/bzip2.target.mk)))),)
  include third_party/bzip2/bzip2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/cacheinvalidation/cacheinvalidation.target.mk)))),)
  include third_party/cacheinvalidation/cacheinvalidation.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/cacheinvalidation/cacheinvalidation_proto_cpp.target.mk)))),)
  include third_party/cacheinvalidation/cacheinvalidation_proto_cpp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/cacheinvalidation/cacheinvalidation_unittests.target.mk)))),)
  include third_party/cacheinvalidation/cacheinvalidation_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/cacheinvalidation/cacheinvalidation_unittests_run.target.mk)))),)
  include third_party/cacheinvalidation/cacheinvalidation_unittests_run.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/cld/cld.target.mk)))),)
  include third_party/cld/cld.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/codesighs/codesighs.target.mk)))),)
  include third_party/codesighs/codesighs.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/codesighs/maptsvdifftool.target.mk)))),)
  include third_party/codesighs/maptsvdifftool.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/codesighs/nm2tsv.target.mk)))),)
  include third_party/codesighs/nm2tsv.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/cros_dbus_cplusplus/dbus_cplusplus.target.mk)))),)
  include third_party/cros_dbus_cplusplus/dbus_cplusplus.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/expat/expat.target.mk)))),)
  include third_party/expat/expat.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/ffmpeg/ffmpeg.target.mk)))),)
  include third_party/ffmpeg/ffmpeg.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/ffmpeg/ffmpeg_yasm.target.mk)))),)
  include third_party/ffmpeg/ffmpeg_yasm.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/ffmpeg/ffmpegsumo.target.mk)))),)
  include third_party/ffmpeg/ffmpegsumo.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/flac/libflac.target.mk)))),)
  include third_party/flac/libflac.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/freetype2/freetype2.target.mk)))),)
  include third_party/freetype2/freetype2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/harfbuzz-ng/harfbuzz-ng.target.mk)))),)
  include third_party/harfbuzz-ng/harfbuzz-ng.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/hunspell/hunspell.target.mk)))),)
  include third_party/hunspell/hunspell.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/hyphen/hyphen.target.mk)))),)
  include third_party/hyphen/hyphen.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/iccjpeg/iccjpeg.target.mk)))),)
  include third_party/iccjpeg/iccjpeg.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/icu/icudata.target.mk)))),)
  include third_party/icu/icudata.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/icu/icui18n.target.mk)))),)
  include third_party/icu/icui18n.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/icu/icuuc.target.mk)))),)
  include third_party/icu/icuuc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/jsoncpp/jsoncpp.target.mk)))),)
  include third_party/jsoncpp/jsoncpp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/khronos/khronos_headers.target.mk)))),)
  include third_party/khronos/khronos_headers.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldatabase.target.mk)))),)
  include third_party/leveldatabase/leveldatabase.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_arena_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_arena_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_bloom_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_bloom_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_cache_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_cache_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_coding_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_coding_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_corruption_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_corruption_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_crc32c_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_crc32c_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_db_bench.target.mk)))),)
  include third_party/leveldatabase/leveldb_db_bench.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_db_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_db_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_dbformat_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_dbformat_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_env_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_env_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_filename_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_filename_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_filter_block_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_filter_block_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_log_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_log_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_skiplist_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_skiplist_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_table_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_table_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_testutil.target.mk)))),)
  include third_party/leveldatabase/leveldb_testutil.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_version_edit_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_version_edit_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/leveldatabase/leveldb_write_batch_test.target.mk)))),)
  include third_party/leveldatabase/leveldb_write_batch_test.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libXNVCtrl/libXNVCtrl.target.mk)))),)
  include third_party/libXNVCtrl/libXNVCtrl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libevent/libevent.host.mk)))),)
  include third_party/libevent/libevent.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libevent/libevent.target.mk)))),)
  include third_party/libevent/libevent.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/libjingle.target.mk)))),)
  include third_party/libjingle/libjingle.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/libjingle_p2p_constants.target.mk)))),)
  include third_party/libjingle/libjingle_p2p_constants.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/libjingle_webrtc.target.mk)))),)
  include third_party/libjingle/libjingle_webrtc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/libpeerconnection.target.mk)))),)
  include third_party/libjingle/libpeerconnection.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjingle/peerconnection_server.target.mk)))),)
  include third_party/libjingle/peerconnection_server.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libjpeg_turbo/libjpeg.target.mk)))),)
  include third_party/libjpeg_turbo/libjpeg.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libmtp/libmtp.target.mk)))),)
  include third_party/libmtp/libmtp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libphonenumber/libphonenumber.target.mk)))),)
  include third_party/libphonenumber/libphonenumber.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libphonenumber/libphonenumber_unittests.target.mk)))),)
  include third_party/libphonenumber/libphonenumber_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libphonenumber/libphonenumber_without_metadata.target.mk)))),)
  include third_party/libphonenumber/libphonenumber_without_metadata.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libpng/libpng.target.mk)))),)
  include third_party/libpng/libpng.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/libsrtp.target.mk)))),)
  include third_party/libsrtp/libsrtp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/rdbx_driver.target.mk)))),)
  include third_party/libsrtp/rdbx_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/replay_driver.target.mk)))),)
  include third_party/libsrtp/replay_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/roc_driver.target.mk)))),)
  include third_party/libsrtp/roc_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/rtpw.target.mk)))),)
  include third_party/libsrtp/rtpw.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_driver.target.mk)))),)
  include third_party/libsrtp/srtp_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_runtest.target.mk)))),)
  include third_party/libsrtp/srtp_runtest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_aes_calc.target.mk)))),)
  include third_party/libsrtp/srtp_test_aes_calc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_cipher_driver.target.mk)))),)
  include third_party/libsrtp/srtp_test_cipher_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_datatypes_driver.target.mk)))),)
  include third_party/libsrtp/srtp_test_datatypes_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_env.target.mk)))),)
  include third_party/libsrtp/srtp_test_env.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_kernel_driver.target.mk)))),)
  include third_party/libsrtp/srtp_test_kernel_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_rand_gen.target.mk)))),)
  include third_party/libsrtp/srtp_test_rand_gen.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_sha1_driver.target.mk)))),)
  include third_party/libsrtp/srtp_test_sha1_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libsrtp/srtp_test_stat_driver.target.mk)))),)
  include third_party/libsrtp/srtp_test_stat_driver.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libusb/libusb.target.mk)))),)
  include third_party/libusb/libusb.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/gen_asm_offsets_vp8.target.mk)))),)
  include third_party/libvpx/gen_asm_offsets_vp8.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/gen_asm_offsets_vpx_scale.target.mk)))),)
  include third_party/libvpx/gen_asm_offsets_vpx_scale.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx.target.mk)))),)
  include third_party/libvpx/libvpx.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx_asm_offsets_vp8.target.mk)))),)
  include third_party/libvpx/libvpx_asm_offsets_vp8.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx_asm_offsets_vpx_scale.target.mk)))),)
  include third_party/libvpx/libvpx_asm_offsets_vpx_scale.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx_intrinsics_mmx.target.mk)))),)
  include third_party/libvpx/libvpx_intrinsics_mmx.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx_intrinsics_sse2.target.mk)))),)
  include third_party/libvpx/libvpx_intrinsics_sse2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx_intrinsics_ssse3.target.mk)))),)
  include third_party/libvpx/libvpx_intrinsics_ssse3.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/libvpx_obj_int_extract.host.mk)))),)
  include third_party/libvpx/libvpx_obj_int_extract.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/simple_decoder.target.mk)))),)
  include third_party/libvpx/simple_decoder.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libvpx/simple_encoder.target.mk)))),)
  include third_party/libvpx/simple_encoder.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libwebp/libwebp.target.mk)))),)
  include third_party/libwebp/libwebp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libwebp/libwebp_dec.target.mk)))),)
  include third_party/libwebp/libwebp_dec.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libwebp/libwebp_demux.target.mk)))),)
  include third_party/libwebp/libwebp_demux.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libwebp/libwebp_dsp.target.mk)))),)
  include third_party/libwebp/libwebp_dsp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libwebp/libwebp_dsp_neon.target.mk)))),)
  include third_party/libwebp/libwebp_dsp_neon.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libwebp/libwebp_enc.target.mk)))),)
  include third_party/libwebp/libwebp_enc.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libwebp/libwebp_utils.target.mk)))),)
  include third_party/libwebp/libwebp_utils.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libxml/libxml.target.mk)))),)
  include third_party/libxml/libxml.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libxslt/libxslt.target.mk)))),)
  include third_party/libxslt/libxslt.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/libyuv/libyuv.target.mk)))),)
  include third_party/libyuv/libyuv.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/lzma_sdk/lzma_sdk.target.mk)))),)
  include third_party/lzma_sdk/lzma_sdk.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/mesa/mesa.target.mk)))),)
  include third_party/mesa/mesa.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/mesa/mesa_headers.target.mk)))),)
  include third_party/mesa/mesa_headers.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/mesa/osmesa.target.mk)))),)
  include third_party/mesa/osmesa.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/modp_b64/modp_b64.host.mk)))),)
  include third_party/modp_b64/modp_b64.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/modp_b64/modp_b64.target.mk)))),)
  include third_party/modp_b64/modp_b64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/mt19937ar/mt19937ar.target.mk)))),)
  include third_party/mt19937ar/mt19937ar.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/mtpd/mtpd.target.mk)))),)
  include third_party/mtpd/mtpd.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/npapi/npapi.target.mk)))),)
  include third_party/npapi/npapi.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/openssl/openssl.target.mk)))),)
  include third_party/openssl/openssl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/opus/opus.target.mk)))),)
  include third_party/opus/opus.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/opus/opus_demo.target.mk)))),)
  include third_party/opus/opus_demo.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/ots/ots.target.mk)))),)
  include third_party/ots/ots.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_full_do_not_use.host.mk)))),)
  include third_party/protobuf/protobuf_full_do_not_use.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_full_do_not_use.target.mk)))),)
  include third_party/protobuf/protobuf_full_do_not_use.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_lite.host.mk)))),)
  include third_party/protobuf/protobuf_lite.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protobuf_lite.target.mk)))),)
  include third_party/protobuf/protobuf_lite.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/protoc.host.mk)))),)
  include third_party/protobuf/protoc.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/protobuf/py_proto.target.mk)))),)
  include third_party/protobuf/py_proto.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/qcms/qcms.target.mk)))),)
  include third_party/qcms/qcms.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/re2/re2.target.mk)))),)
  include third_party/re2/re2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/safe_browsing/safe_browsing.target.mk)))),)
  include third_party/safe_browsing/safe_browsing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/sfntly/sfntly.target.mk)))),)
  include third_party/sfntly/sfntly.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/smhasher/cityhash.target.mk)))),)
  include third_party/smhasher/cityhash.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/smhasher/murmurhash3.target.mk)))),)
  include third_party/smhasher/murmurhash3.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/smhasher/pmurhash.target.mk)))),)
  include third_party/smhasher/pmurhash.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/snappy/snappy.target.mk)))),)
  include third_party/snappy/snappy.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/snappy/snappy_unittest.target.mk)))),)
  include third_party/snappy/snappy_unittest.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/speex/libspeex.target.mk)))),)
  include third_party/speex/libspeex.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/sqlite/sqlite.target.mk)))),)
  include third_party/sqlite/sqlite.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/sqlite/sqlite_shell.target.mk)))),)
  include third_party/sqlite/sqlite_shell.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/undoview/undoview.target.mk)))),)
  include third_party/undoview/undoview.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/v8-i18n/build/api2c.host.mk)))),)
  include third_party/v8-i18n/build/api2c.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/v8-i18n/build/v8-i18n.target.mk)))),)
  include third_party/v8-i18n/build/v8-i18n.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/common_audio/resampler.target.mk)))),)
  include third_party/webrtc/common_audio/resampler.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/common_audio/signal_processing.target.mk)))),)
  include third_party/webrtc/common_audio/signal_processing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/common_audio/vad.target.mk)))),)
  include third_party/webrtc/common_audio/vad.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/common_video/common_video.target.mk)))),)
  include third_party/webrtc/common_video/common_video.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/CNG.target.mk)))),)
  include third_party/webrtc/modules/CNG.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/G711.target.mk)))),)
  include third_party/webrtc/modules/G711.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/G722.target.mk)))),)
  include third_party/webrtc/modules/G722.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/NetEq.target.mk)))),)
  include third_party/webrtc/modules/NetEq.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/NetEq4.target.mk)))),)
  include third_party/webrtc/modules/NetEq4.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/PCM16B.target.mk)))),)
  include third_party/webrtc/modules/PCM16B.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/audio_coding_module.target.mk)))),)
  include third_party/webrtc/modules/audio_coding_module.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/audio_conference_mixer.target.mk)))),)
  include third_party/webrtc/modules/audio_conference_mixer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/audio_device.target.mk)))),)
  include third_party/webrtc/modules/audio_device.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/audio_processing.target.mk)))),)
  include third_party/webrtc/modules/audio_processing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/audio_processing_sse2.target.mk)))),)
  include third_party/webrtc/modules/audio_processing_sse2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/bitrate_controller.target.mk)))),)
  include third_party/webrtc/modules/bitrate_controller.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/iLBC.target.mk)))),)
  include third_party/webrtc/modules/iLBC.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/iSAC.target.mk)))),)
  include third_party/webrtc/modules/iSAC.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/iSACFix.target.mk)))),)
  include third_party/webrtc/modules/iSACFix.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/media_file.target.mk)))),)
  include third_party/webrtc/modules/media_file.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/paced_sender.target.mk)))),)
  include third_party/webrtc/modules/paced_sender.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/remote_bitrate_estimator.target.mk)))),)
  include third_party/webrtc/modules/remote_bitrate_estimator.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/rtp_rtcp.target.mk)))),)
  include third_party/webrtc/modules/rtp_rtcp.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/video_capture_module.target.mk)))),)
  include third_party/webrtc/modules/video_capture_module.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/video_coding/codecs/vp8/webrtc_vp8.target.mk)))),)
  include third_party/webrtc/modules/video_coding/codecs/vp8/webrtc_vp8.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/video_coding/utility/video_coding_utility.target.mk)))),)
  include third_party/webrtc/modules/video_coding/utility/video_coding_utility.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/video_processing.target.mk)))),)
  include third_party/webrtc/modules/video_processing.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/video_processing_sse2.target.mk)))),)
  include third_party/webrtc/modules/video_processing_sse2.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/video_render_module.target.mk)))),)
  include third_party/webrtc/modules/video_render_module.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/webrtc_i420.target.mk)))),)
  include third_party/webrtc/modules/webrtc_i420.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/webrtc_opus.target.mk)))),)
  include third_party/webrtc/modules/webrtc_opus.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/webrtc_utility.target.mk)))),)
  include third_party/webrtc/modules/webrtc_utility.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/modules/webrtc_video_coding.target.mk)))),)
  include third_party/webrtc/modules/webrtc_video_coding.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/system_wrappers/source/system_wrappers.target.mk)))),)
  include third_party/webrtc/system_wrappers/source/system_wrappers.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/tools/command_line_parser.target.mk)))),)
  include third_party/webrtc/tools/command_line_parser.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/tools/frame_analyzer.target.mk)))),)
  include third_party/webrtc/tools/frame_analyzer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/tools/frame_editing_lib.target.mk)))),)
  include third_party/webrtc/tools/frame_editing_lib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/tools/frame_editor.target.mk)))),)
  include third_party/webrtc/tools/frame_editor.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/tools/psnr_ssim_analyzer.target.mk)))),)
  include third_party/webrtc/tools/psnr_ssim_analyzer.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/tools/rgba_to_i420_converter.target.mk)))),)
  include third_party/webrtc/tools/rgba_to_i420_converter.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/tools/video_quality_analysis.target.mk)))),)
  include third_party/webrtc/tools/video_quality_analysis.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/video_engine/video_engine_core.target.mk)))),)
  include third_party/webrtc/video_engine/video_engine_core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/webrtc/voice_engine/voice_engine_core.target.mk)))),)
  include third_party/webrtc/voice_engine/voice_engine_core.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/widevine/cdm/widevine_cdm_binaries.target.mk)))),)
  include third_party/widevine/cdm/widevine_cdm_binaries.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/widevine/cdm/widevine_cdm_version_h.target.mk)))),)
  include third_party/widevine/cdm/widevine_cdm_version_h.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/widevine/cdm/widevinecdmadapter.target.mk)))),)
  include third_party/widevine/cdm/widevinecdmadapter.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/config_sources.host.mk)))),)
  include third_party/yasm/config_sources.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/generate_files.host.mk)))),)
  include third_party/yasm/generate_files.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genmacro.host.mk)))),)
  include third_party/yasm/genmacro.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genmodule.host.mk)))),)
  include third_party/yasm/genmodule.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genperf.host.mk)))),)
  include third_party/yasm/genperf.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genperf_libs.host.mk)))),)
  include third_party/yasm/genperf_libs.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genstring.host.mk)))),)
  include third_party/yasm/genstring.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/genversion.host.mk)))),)
  include third_party/yasm/genversion.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/re2c.host.mk)))),)
  include third_party/yasm/re2c.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/yasm/yasm.host.mk)))),)
  include third_party/yasm/yasm.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/zlib/minizip.target.mk)))),)
  include third_party/zlib/minizip.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,third_party/zlib/zlib.target.mk)))),)
  include third_party/zlib/zlib.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,tools/gtk_clipboard_dump/gtk_clipboard_dump.target.mk)))),)
  include tools/gtk_clipboard_dump/gtk_clipboard_dump.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,tools/imagediff/image_diff.target.mk)))),)
  include tools/imagediff/image_diff.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,tools/json_schema_compiler/api_gen_util.target.mk)))),)
  include tools/json_schema_compiler/api_gen_util.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,tools/json_schema_compiler/test/json_schema_compiler_tests.target.mk)))),)
  include tools/json_schema_compiler/test/json_schema_compiler_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,tools/xdisplaycheck/xdisplaycheck.target.mk)))),)
  include tools/xdisplaycheck/xdisplaycheck.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/base/strings/ui_strings.target.mk)))),)
  include ui/base/strings/ui_strings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/base/strings/ui_unittest_strings.target.mk)))),)
  include ui/base/strings/ui_unittest_strings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/compositor/compositor.target.mk)))),)
  include ui/compositor/compositor.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/compositor/compositor_test_support.target.mk)))),)
  include ui/compositor/compositor_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/compositor/compositor_unittests.target.mk)))),)
  include ui/compositor/compositor_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/gl/gl.target.mk)))),)
  include ui/gl/gl.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/gl/gl_unittest_utils.target.mk)))),)
  include ui/gl/gl_unittest_utils.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/native_theme/native_theme.target.mk)))),)
  include ui/native_theme/native_theme.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/run_ui_unittests.target.mk)))),)
  include ui/run_ui_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/shell_dialogs.target.mk)))),)
  include ui/shell_dialogs.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/snapshot/snapshot.target.mk)))),)
  include ui/snapshot/snapshot.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/snapshot/snapshot_unittests.target.mk)))),)
  include ui/snapshot/snapshot_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/surface/surface.target.mk)))),)
  include ui/surface/surface.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/ui.target.mk)))),)
  include ui/ui.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/ui_resources.target.mk)))),)
  include ui/ui_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/ui_test_support.target.mk)))),)
  include ui/ui_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/ui_unittests.target.mk)))),)
  include ui/ui_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/web_dialogs/web_dialogs.target.mk)))),)
  include ui/web_dialogs/web_dialogs.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,ui/web_dialogs/web_dialogs_test_support.target.mk)))),)
  include ui/web_dialogs/web_dialogs_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,url/googleurl_unittests.target.mk)))),)
  include url/googleurl_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,url/url.target.mk)))),)
  include url/url.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,url/url_unittests.target.mk)))),)
  include url/url_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/js2c.host.mk)))),)
  include v8/tools/gyp/js2c.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/mksnapshot.x64.host.mk)))),)
  include v8/tools/gyp/mksnapshot.x64.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/postmortem-metadata.target.mk)))),)
  include v8/tools/gyp/postmortem-metadata.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8.host.mk)))),)
  include v8/tools/gyp/v8.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8.target.mk)))),)
  include v8/tools/gyp/v8.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8_base.x64.host.mk)))),)
  include v8/tools/gyp/v8_base.x64.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8_base.x64.target.mk)))),)
  include v8/tools/gyp/v8_base.x64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8_nosnapshot.x64.host.mk)))),)
  include v8/tools/gyp/v8_nosnapshot.x64.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8_nosnapshot.x64.target.mk)))),)
  include v8/tools/gyp/v8_nosnapshot.x64.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8_shell.host.mk)))),)
  include v8/tools/gyp/v8_shell.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8_snapshot.host.mk)))),)
  include v8/tools/gyp/v8_snapshot.host.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,v8/tools/gyp/v8_snapshot.target.mk)))),)
  include v8/tools/gyp/v8_snapshot.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/compositor_bindings/webkit_compositor_bindings.target.mk)))),)
  include webkit/compositor_bindings/webkit_compositor_bindings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/compositor_bindings/webkit_compositor_bindings_unittests.target.mk)))),)
  include webkit/compositor_bindings/webkit_compositor_bindings_unittests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/compositor_bindings/webkit_compositor_support.target.mk)))),)
  include webkit/compositor_bindings/webkit_compositor_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/copy_npapi_test_plugin.target.mk)))),)
  include webkit/copy_npapi_test_plugin.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/gpu/webkit_gpu.target.mk)))),)
  include webkit/gpu/webkit_gpu.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/npapi_test_common.target.mk)))),)
  include webkit/npapi_test_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/npapi_test_plugin.target.mk)))),)
  include webkit/npapi_test_plugin.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/pull_in_DumpRenderTree.target.mk)))),)
  include webkit/pull_in_DumpRenderTree.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/pull_in_copy_TestNetscapePlugIn.target.mk)))),)
  include webkit/pull_in_copy_TestNetscapePlugIn.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/pull_in_webkit_unit_tests.target.mk)))),)
  include webkit/pull_in_webkit_unit_tests.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/clearkeycdm.target.mk)))),)
  include webkit/support/clearkeycdm.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/clearkeycdmadapter.target.mk)))),)
  include webkit/support/clearkeycdmadapter.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/dump_file_system.target.mk)))),)
  include webkit/support/dump_file_system.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/glue.target.mk)))),)
  include webkit/support/glue.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/user_agent.target.mk)))),)
  include webkit/support/user_agent.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_base.target.mk)))),)
  include webkit/support/webkit_base.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_media.target.mk)))),)
  include webkit/support/webkit_media.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_resources.target.mk)))),)
  include webkit/support/webkit_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_storage.target.mk)))),)
  include webkit/support/webkit_storage.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_strings.target.mk)))),)
  include webkit/support/webkit_strings.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_support.target.mk)))),)
  include webkit/support/webkit_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_support_common.target.mk)))),)
  include webkit/support/webkit_support_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_support_gfx.target.mk)))),)
  include webkit/support/webkit_support_gfx.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/support/webkit_version.target.mk)))),)
  include webkit/support/webkit_version.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/test_shell.target.mk)))),)
  include webkit/test_shell.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/test_shell_common.target.mk)))),)
  include webkit/test_shell_common.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/test_shell_pak.target.mk)))),)
  include webkit/test_shell_pak.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/test_shell_resources.target.mk)))),)
  include webkit/test_shell_resources.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/test_shell_test_support.target.mk)))),)
  include webkit/test_shell_test_support.target.mk
endif
ifeq ($(strip $(foreach prefix,$(NO_LOAD),\
    $(findstring $(join ^,$(prefix)),\
                 $(join ^,webkit/test_shell_tests.target.mk)))),)
  include webkit/test_shell_tests.target.mk
endif

quiet_cmd_regen_makefile = ACTION Regenerating $@
cmd_regen_makefile = ./build/gyp_chromium -fmake --ignore-environment "--toplevel-dir=/home/david/Projects/spa/examples/chromium/src" -I/home/david/Projects/spa/examples/chromium/src/build/common.gypi "--depth=/home/david/Projects/spa/examples/chromium/src" "-Dclang=1" build/all.gyp
Makefile: third_party/icu/icu.gyp third_party/cacheinvalidation/cacheinvalidation_unittests.isolate third_party/flac/flac.gyp chrome/browser/nacl_host/test/mock_nacl_gdb.gyp third_party/expat/expat.gyp ppapi/ppapi_tests.gypi third_party/webrtc/modules/utility/source/utility.gypi components/webdata.gypi native_client/src/trusted/interval_multiset/interval_multiset.gyp chrome/chrome_nibs.gypi courgette/courgette.gyp sync/sync_tests.gypi content/browser/devtools/devtools_resources.gyp third_party/webrtc/voice_engine/test/voice_engine_tests.gypi third_party/libvpx/libvpx_srcs_arm_neon_cpu_detect.gypi native_client/src/shared/gio/gio.gyp components/zip.gypi printing/printing.gyp third_party/libusb/libusb.gyp third_party/webrtc/modules/audio_coding/codecs/isac/main/source/isac.gypi device/device.gyp third_party/mesa/mesa.gypi third_party/libvpx/libvpx_srcs_x86.gypi third_party/opus/opus.gyp build/uiautomator_test.gypi third_party/undoview/undoview.gyp chrome/chrome_tests_unit.gypi webkit/dom_storage/webkit_dom_storage.gypi third_party/sqlite/sqlite.gyp third_party/yasm/yasm.gyp ui/base/ime/ime.gypi third_party/webrtc/modules/rtp_rtcp/test/testFec/test_fec.gypi chrome/common_constants.gyp cc/cc_tests.gyp cloud_print/service/service.gyp chrome/chrome_dll.gypi content/content_jni.gypi chrome/test/gpu/test_support_gpu.gypi third_party/webrtc/modules/video_coding/utility/video_coding_utility.gyp third_party/webrtc/modules/audio_coding/codecs/isac/fix/source/isacfix.gypi chrome/chrome_installer_util.gypi third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.gyp build/apk_test.gypi third_party/mt19937ar/mt19937ar.gyp native_client/src/trusted/service_runtime/arch/x86/service_runtime_x86.gyp webkit/compositor_bindings/compositor_bindings_tests.gyp native_client/src/shared/serialization/serialization.gyp chrome/chrome_tests.gypi third_party/npapi/npapi.gyp native_client/build/external_code.gypi chrome/app/policy/cloud_policy_codegen.gyp ui/web_dialogs/web_dialogs.gyp third_party/webrtc/modules/audio_processing/audio_processing_tests.gypi ppapi/native_client/src/untrusted/irt_stub/irt_stub.gyp third_party/protobuf/protobuf_lite.gypi third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gypi third_party/webrtc/modules/pacing/pacing.gypi third_party/webrtc/common_audio/common_audio.gyp native_client/src/trusted/desc/desc.gyp breakpad/breakpad_tools.gypi chrome/nacl.gypi native_client/src/trusted/service_runtime/service_runtime.gyp content/content_tests.gypi third_party/WebKit/Source/core/core.gyp/core.gyp native_client/src/shared/srpc/srpc.gyp webkit/quota/webkit_quota.gypi third_party/webrtc/modules/video_render/video_render.gypi ui/compositor/compositor.gyp gpu/command_buffer/command_buffer.gyp webkit/gpu/webkit_gpu.gyp components/tracing.gyp cloud_print/cloud_print.gyp chrome/browser/search_engines/prepopulated_engines.gyp gpu/command_buffer_service.gypi third_party/libvpx/libvpx_srcs_arm_neon.gypi ui/snapshot/snapshot.gyp third_party/webrtc/common_video/common_video.gyp breakpad/breakpad_sender.gypi media/shared_memory_support.gypi sync/sync_internal_api.gypi content/content_browser.gypi sync/sync_notifier.gypi third_party/webrtc/modules/audio_coding/codecs/opus/opus.gypi native_client/src/trusted/nonnacl_util/nonnacl_util.gyp base/base_untrusted.gyp third_party/WebKit/Source/WebKit/chromium/WebKitUnitTests.gyp chrome/test/data/nacl/nacl_test_data.gyp build/android/write_ordered_libraries.gypi sandbox/linux/sandbox_linux.gypi device/device_bluetooth_strings.gyp third_party/WebKit/Source/Platform/Platform.gypi components/autofill.gypi skia/skia.gyp ppapi/ppapi_shared.gypi components/sessions.gypi third_party/WebKit/Source/WebKit/chromium/All.gyp tools/xdisplaycheck/xdisplaycheck.gyp chrome/chrome_android_paks.gypi third_party/webrtc/modules/bitrate_controller/bitrate_controller.gypi apps/apps.gypi chrome/chrome_repack_chrome_200_percent.gypi gpu/tools/tools.gyp native_client/src/trusted/cpu_features/cpu_features.gyp third_party/webrtc/modules/audio_coding/codecs/isac/isacfix_test.gypi chrome/common/extensions/api/api.gyp tools/imagediff/image_diff.gyp third_party/widevine/cdm/widevine_cdm.gyp sandbox/sandbox.gyp chrome/js_unittest_vars.gypi third_party/cros_dbus_cplusplus/cros_dbus_cplusplus.gyp third_party/webrtc/modules/media_file/source/media_file.gypi ppapi/ppapi.gyp third_party/hunspell/hunspell.gyp third_party/webrtc/video_engine/video_engine.gyp third_party/webrtc/modules/audio_processing/audio_processing.gypi webkit/support/webkit_support.gyp build/android/strip_native_libraries.gypi native_client/src/untrusted/irt/irt.gyp chrome/chrome_repack_chrome_touch_140_percent.gypi net/third_party/nss/ssl.gyp ui/native_theme/native_theme.gyp build/json_to_struct.gypi webkit/blob/webkit_blob.gypi ppapi/ppapi_proxy_untrusted.gyp chrome/chrome_installer.gypi chrome/chrome.isolate gpu/gpu.gyp chrome/app/policy/policy_templates.gypi ppapi/native_client/native_client.gyp third_party/libevent/libevent.gyp native_client/tools.gyp third_party/libvpx/libvpx_srcs_mips.gypi native_client/src/trusted/weak_ref/weak_ref.gyp build/win/importlibs/create_import_lib.gypi chrome/chrome_dll_bundle.gypi third_party/WebKit/Source/WebKit/chromium/WinPrecompile.gypi content/content_resources.gyp chrome/chrome_repack_chrome.gypi third_party/webrtc/modules/rtp_rtcp/source/rtp_rtcp_tests.gypi third_party/WebKit/Source/core/core.gyp/core_derived_sources.gyp sdch/sdch.gyp build/apk_fake_jar.gypi components/user_prefs.gypi third_party/modp_b64/modp_b64.gyp third_party/zlib/zlib.gyp ppapi/ppapi_ipc.gypi sync/sync_core.gypi build/isolate.gypi chrome/chrome.gyp crypto/crypto.gyp ui/base/ime/ime_test_support.gypi third_party/webrtc/modules/audio_coding/main/source/audio_coding_module.gypi native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp chrome/chrome_browser_chromeos.gypi build/common_untrusted.gypi third_party/cacheinvalidation/cacheinvalidation.gyp native_client/src/trusted/validator_ragel/dfa_validator_x86_64.gyp components/auto_login_parser.gypi native_client/src/trusted/platform_qualify/platform_qualify.gyp third_party/skia/gyp/effects.gypi content/content_gpu.gypi third_party/webrtc/modules/modules.gyp remoting/remoting.gyp third_party/webrtc/modules/audio_coding/codecs/g722/g722.gypi third_party/mtpd/mtpd.gyp third_party/speex/speex.gyp chrome/browser_tests.isolate webkit/tools/test_shell/test_shell.gypi third_party/webrtc/system_wrappers/source/system_wrappers.gyp third_party/safe_browsing/safe_browsing.gyp third_party/webrtc/modules/audio_coding/neteq4/neteq_tests.gypi third_party/libxslt/libxslt.gyp third_party/webrtc/modules/video_coding/codecs/vp8/vp8.gyp third_party/webrtc/modules/audio_coding/codecs/cng/cng.gypi third_party/libwebp/libwebp.gyp third_party/webrtc/common_audio/signal_processing/signal_processing.gypi sandbox/win/sandbox_win.gypi net/net_unittests.isolate chrome/chrome_repack_locales.gypi build/temp_gyp/googleurl.gyp webkit/database/webkit_database.gypi third_party/harfbuzz-ng/harfbuzz.gyp tools/gtk_clipboard_dump/gtk_clipboard_dump.gyp gpu/gles2_conform_support/gles2_conform_support.gyp native_client/src/trusted/validator/x86/64/validator_x86_64.gyp build/java_apk.gypi native_client/src/trusted/nacl_base/nacl_base.gyp build/some.gyp third_party/webrtc/modules/audio_coding/neteq4/neteq.gypi chrome/browser/performance_monitor/performance_monitor.gyp ui/surface/surface.gyp third_party/libvpx/libvpx_srcs_arm.gypi sync/sync_proto.gypi third_party/jsoncpp/jsoncpp.gyp third_party/hyphen/hyphen.gyp native_client/build/untrusted.gypi native_client/src/trusted/validator_x86/validator_x86.gyp breakpad/breakpad_handler.gypi third_party/snappy/snappy.gyp content/content_common.gypi native_client/src/trusted/debug_stub/debug_stub.gyp third_party/webrtc/modules/audio_conference_mixer/source/audio_conference_mixer.gypi webkit/base/webkit_base.gypi ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp chrome/chrome_common.gypi third_party/re2/re2.gyp webkit/appcache/webkit_appcache.gypi webkit/fileapi/webkit_fileapi.gypi third_party/openssl/openssl.gypi third_party/WebKit/Source/devtools/devtools.gyp third_party/libphonenumber/libphonenumber.gyp tools/json_schema_compiler/test/json_schema_compiler_tests.gyp gpu/command_buffer_common.gypi gpu/gpu_ipc.gypi third_party/openssl/openssl.gyp content/content_shell.gypi third_party/cld/cld.gyp ui/base/strings/ui_strings.gyp third_party/smhasher/smhasher.gyp third_party/libjingle/libjingle.gyp build/ios/mac_build.gypi third_party/ots/ots.gyp jingle/jingle.gyp ppapi/ppapi_cpp.gypi ppapi/native_client/src/trusted/plugin/plugin.gypi chrome/version_resource_rules.gypi third_party/skia/gyp/core.gypi build/json_schema_compile.gypi third_party/libyuv/libyuv.gyp ipc/ipc_untrusted.gyp native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_reg_sfi.gyp components/navigation_interception.gypi sandbox/linux/sandbox_linux_test_sources.gypi third_party/ffmpeg/ffmpeg_generated.gypi content/content_ppapi_plugin.gypi url/url.gyp third_party/libjpeg_turbo/libjpeg.gyp third_party/sfntly/sfntly.gyp third_party/webrtc/build/common.gypi third_party/WebKit/Source/modules/modules.gyp native_client/src/trusted/validator/x86/validate_x86.gyp chrome/chrome_renderer.gypi build/android/push_libraries.gypi chrome/chrome_browser_ui_views.gyp third_party/angle/src/build_angle.gypi chrome/chrome_exe.gypi content/browser/gpu/test_support_gpu.gypi native_client/src/trusted/fault_injection/fault_injection.gyp third_party/leveldatabase/leveldatabase.gyp build/protoc_java.gypi third_party/WebKit/Source/core/core.gypi third_party/yasm/yasm_compile.gypi chrome/chrome_repack_chrome_100_percent.gypi build/release.gypi sql/sql.gyp native_client/src/trusted/threading/threading.gyp base/allocator/allocator.gyp ipc/ipc.gyp native_client/src/trusted/reverse_service/reverse_service.gyp ppapi/ppapi_sources.gypi third_party/WebKit/Source/modules/modules.gypi gpu/command_buffer/command_buffer_untrusted.gyp third_party/WebKit/Source/core/features.gypi third_party/WebKit/Source/bindings/bindings.gypi third_party/ffmpeg/ffmpeg.gyp components/components.gyp build/grit_action.gypi build/all.gyp third_party/WebKit/Source/bindings/derived_sources.gyp base/third_party/dynamic_annotations/dynamic_annotations.gyp chrome/chrome_resources.gyp third_party/mesa/mesa.gyp v8/tools/gyp/v8.gyp components/visitedlink.gypi ppapi/native_client/src/untrusted/pnacl_support_extension/pnacl_support_extension.gyp ppapi/native_client/src/trusted/plugin/plugin.gyp ppapi/ppapi_untrusted.gyp base/base_unittests.isolate gpu/command_buffer_client.gypi webkit/storage/webkit_storage.gypi third_party/iccjpeg/iccjpeg.gyp third_party/webrtc/modules/video_coding/main/source/video_coding.gypi third_party/webrtc/modules/video_capture/video_capture.gypi native_client/src/untrusted/pthread/pthread.gyp third_party/libvpx/libvpx.gyp ppapi/ppapi_host.gypi cloud_print/service/win/service_resources.gypi third_party/WebKit/Source/WebKit/chromium/WebKit.gypi third_party/WebKit/Source/config.gyp build/java_aidl.gypi google_apis/google_apis.gyp remoting/proto/chromotocol.gyp build/filename_rules.gypi third_party/libmtp/libmtp.gyp native_client/src/trusted/simple_service/simple_service.gyp gpu/gpu_untrusted.gyp third_party/webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator.gypi tools/json_schema_compiler/api_gen_util.gyp third_party/webrtc/voice_engine/voice_engine.gyp build/internal/release_impl.gypi content/content_renderer.gypi third_party/bzip2/bzip2.gyp chrome/chrome_repack_resources.gypi third_party/libvpx/libvpx_srcs_x86_64.gypi third_party/webrtc/build/generate_asm_header.gypi webkit/glue/webkit_glue.gypi native_client/src/trusted/validator/x86/decoder/ncval_x86_decoder.gyp build/android/cpufeatures.gypi third_party/webrtc/modules/video_coding/main/source/video_coding_test.gypi build/jni_generator.gypi chrome/chrome_android.gypi third_party/webrtc/build/arm_neon.gypi ash/ash_strings.gyp chrome/chrome_repack_chrome_touch_100_percent.gypi testing/gtest.gyp media/media.gyp third_party/webrtc/modules/video_coding/codecs/test/video_codecs_test_framework.gypi build/linux/system.gyp webkit/media/webkit_media.gypi third_party/webrtc/modules/audio_coding/neteq/neteq.gypi media/media_untrusted.gyp dbus/dbus.gyp ui/base/ime/ime_unittests.gypi third_party/webrtc/common_audio/vad/vad.gypi third_party/webrtc/modules/video_processing/main/source/video_processing.gypi third_party/webrtc/modules/audio_coding/codecs/ilbc/ilbc.gypi third_party/libvpx/libvpx_srcs_x86_intrinsics.gypi components/tracing_untrusted.gyp third_party/webrtc/modules/audio_coding/codecs/pcm16b/pcm16b.gypi native_client/src/trusted/validator/validator.gyp build/internal/release_impl_official.gypi ui/ui_unittests.gypi third_party/webrtc/common_audio/resampler/resampler.gypi build/json_schema_bundle_compile.gypi ppapi/ppapi_gl.gypi third_party/adobe/flash/flash_player.gyp build/win_precompile.gypi build/jar_file_jni_generator.gypi ppapi/ppapi_ipc_untrusted.gyp cc/cc.gyp net/net.gyp chrome/chrome_browser.gypi chrome/js_unittest_rules.gypi native_client/src/shared/platform/platform.gyp breakpad/breakpad.gyp base/base.gyp third_party/webrtc/modules/video_processing/main/test/vpm_tests.gypi third_party/webrtc/voice_engine/voice_engine_core.gypi gpu/command_buffer/command_buffer.gypi third_party/WebKit/Source/Platform/Platform.gyp/Platform.gyp sync/sync_android.gypi third_party/libpng/libpng.gyp third_party/webrtc/video_engine/test/android/android_video_demo.gypi third_party/WebKit/Source/WebKit/chromium/WebKit.gyp content/content_utility.gypi third_party/webrtc/build/protoc.gypi chrome/chrome_repack_chrome_touch_180_percent.gypi third_party/ots/ots-common.gypi build/android/java_cpp_template.gypi chrome/chrome_browser_ui.gypi ppapi/ppapi_proxy.gypi chrome/version.gypi third_party/lzma_sdk/lzma_sdk.gyp chrome/chrome_ios_bundle_resources.gypi native_client/build/common.gypi native_client/src/trusted/manifest_name_service_proxy/manifest_name_service_proxy.gyp content/content_plugin.gypi ppapi/ppapi_shared_untrusted.gyp third_party/WebKit/Source/wtf/wtf.gyp cloud_print/common/common.gyp content/browser/speech/proto/speech_proto.gyp third_party/webrtc/modules/video_coding/codecs/test_framework/test_framework.gypi chrome/sync_integration_tests.isolate chrome/chrome_repack_pseudo_locales.gypi sync/sync_api.gypi third_party/webrtc/video_engine/test/libvietest/libvietest.gypi third_party/qcms/qcms.gyp gpu/gles2_cmd_helper.gypi third_party/webrtc/modules/video_coding/codecs/i420/main/source/i420.gypi third_party/webrtc/video_engine/test/auto_test/vie_auto_test.gypi ui/ui_resources.gypi native_client/src/trusted/nonnacl_util/nonnacl_util.gypi third_party/protobuf/protobuf.gyp third_party/WebKit/Source/WebKit/chromium/skia_webkit.gyp third_party/libvpx/libvpx_srcs_x86_64_intrinsics.gypi build/internal/release_defaults.gypi third_party/webrtc/modules/rtp_rtcp/source/rtp_rtcp.gypi build/shim_headers.gypi gpu/gpu_common.gypi third_party/webrtc/tools/tools.gyp third_party/libsrtp/libsrtp.gyp components/components_tests.gypi build/java.gypi testing/gmock.gyp native_client/src/trusted/perf_counter/perf_counter.gyp v8/build/common.gypi webkit/compositor_bindings/compositor_bindings.gyp sync/sync.gyp ui/gl/gl.gyp third_party/webrtc/video_engine/video_engine_core.gypi third_party/libXNVCtrl/libXNVCtrl.gyp third_party/webrtc/modules/audio_coding/codecs/g711/g711.gypi chrome/interactive_ui_tests.isolate third_party/khronos/khronos.gyp native_client/src/trusted/gio/gio_wrapped_desc.gyp build/grit_target.gypi third_party/freetype2/freetype2.gyp ui/ui.gyp third_party/codesighs/codesighs.gyp build/nocompile.gypi components/web_contents_delegate_android.gypi third_party/webrtc/modules/video_coding/codecs/tools/video_codecs_tools.gypi chrome/test/data/nacl/nacl_browser_test.gypi third_party/v8-i18n/build/all.gyp build/protoc.gypi third_party/libxml/libxml.gyp content/content_app.gypi chrome/unit_tests.isolate build/copy_test_data_ios.gypi third_party/webrtc/modules/audio_coding/codecs/isac/isac_test.gypi base/base.gypi native_client/src/shared/imc/imc.gyp native_client/src/untrusted/nacl/nacl.gyp third_party/WebKit/Source/core/core.gyp/ConvertFileToHeaderWithCharacterArray.gypi webkit/support/webkit_support.gypi chrome/chrome_browser_extensions.gypi ppapi/ppapi_internal.gyp ui/shell_dialogs.gypi webkit/user_agent/webkit_user_agent.gypi build/common.gypi third_party/WebKit/Source/wtf/wtf.gypi third_party/skia/gyp/gpu.gypi native_client/src/trusted/service_runtime/arch/x86_64/service_runtime_x86_64.gyp third_party/angle/src/build_angle.gyp ipc/ipc.gypi content/content.gyp third_party/webrtc/modules/audio_device/audio_device.gypi webkit/webkit.gyp content/content_worker.gypi
	$(call do_cmd,regen_makefile)

# "all" is a concatenation of the "all" targets from all the included
# sub-makefiles. This is just here to clarify.
all:

# Add in dependency-tracking rules.  $(all_deps) is the list of every single
# target in our tree. Only consider the ones with .d (dependency) info:
d_files := $(wildcard $(foreach f,$(all_deps),$(depsdir)/$(f).d))
ifneq ($(d_files),)
  include $(d_files)
endif
