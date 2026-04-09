# SPDX-License-Identifier: GPL-2.0-only
# Makefile for tools/tests/alloc

XEN_ROOT := $(abspath $(CURDIR)/../../..)
include $(XEN_ROOT)/tools/Rules.mk
RELDIR := $(subst $(XEN_ROOT)/,,$(CURDIR))

TEST_SOURCES := $(notdir $(wildcard test-*.c))
TARGETS := $(TEST_SOURCES:.c=)

.PHONY: all
all: $(TARGETS)

define RUN_TESTS
	@echo "Build configuration:"
	@echo "CC=$(CC)"
	@echo "CFLAGS='$(CFLAGS)'"
	@for test in $? ; do \
		echo;echo "$(RELDIR): RUN_TESTS: $$test...";echo; \
		./$$test ; EXIT_CODE=$$? ; \
		if [ $$EXIT_CODE -ne 0 ]; then \
			echo "Test $$test failed with exit code $$EXIT_CODE"; \
			exit 1; \
		fi; \
	done
	@echo
	@echo "Tests executed successfully:"
	@for test in $? ; do \
		echo "  - $$test"; \
	done
endef

# Run the tests if possible, otherwise print a warning and skip them.
.PHONY: run
# Determine if the tests can be run on the build host. If CC and HOSTCC
# are the same, we can run the tests directly. If they differ, we check
# if binfmt-support and qemu-binfmt are available to run the tests under
# using binfmt-misc using qemu-user-static.
ifeq ($(CC),$(HOSTCC))
    TESTS_RUNNABLE=yes
else
    BINFMT_SUPP := $(if $(wildcard /etc/init.d/binfmt-support),1,0)
    QEMU_BINFMT := $(if $(wildcard /usr/libexec/qemu-binfmt),1,0)
    ifeq ($(BINFMT_SUPP)$(QEMU_BINFMT),11)
        # Running static binaries doesn't need extra setup besides qemu-binfmt
        CFLAGS += -static
        TESTS_RUNNABLE=yes
    else
        TESTS_RUNNABLE=no
    endif
endif

run: $(TARGETS)
ifeq ($(TESTS_RUNNABLE),yes)
	$(RUN_TESTS)
else
	$(warning HOSTCC != CC, and qemu-binfmt not detected, skipping alloc tests)
endif

# Run the tests  binfmt-misc set up
BINFMT_SUPP := $(if $(wildcard /etc/init.d/binfmt-support),1,0)
QEMU_BINFMT := $(if $(wildcard /usr/libexec/qemu-binfmt),1,0)
.PHONY: run-tests
run-tests: $(TARGETS)
ifeq ($(CC),$(HOSTCC))
	$(RUN_TESTS)
else ifeq ($(BINFMT_SUPP)$(QEMU_BINFMT),11)
	$(RUN_TESTS)
else
	$(warning Note: binfmt-support or qemu-user not found, skipping run-tests)
endif

#
# Build and run the tests for multiple architectures,
# skipping if the appropriate cross-compiler is not found.
# The default XEN_TARGET_ARCH is always built and tested as well.
# This is gcc-specific, but can be adapted for other toolchains.
#
ARCHS := arm64-aarch64-linux-gnu arm32-arm-linux-gnueabihf
ARCHS += x86_32-i686-linux-gnu x86_64-x86_64-linux-gnu
ARCHS += ppc64-powerpc64le-linux-gnu riscv64-riscv64-linux-gnu
.PHONY: run-archs
run-archs: $(TARGETS)
ifneq ($(CC),gcc)
	$(warning run-archs target is only supported with CC=gcc for now, skipping)
else
	@set -e;PASSES=;SKIPPED_ARCHS=; \
	MAKEFLAGS="$$MAKEFLAGS --no-print-directory"; \

	for t in $(ARCHS); do \
		A=$${t%%-*}; C=$${t#*-}; \
	    [ $$A != $(XEN_TARGET_ARCH) ] || continue; \
		if ! type "$${C}-gcc" >/dev/null 2>&1; then \
	        echo " $${C}-gcc not found, skipping $${A}"; \
			SKIPPED_ARCHS="$${SKIPPED_ARCHS} $${A}"; continue; \
	    fi; \
		if [ $${A} = $(XEN_TARGET_ARCH) ]; then C=$(CROSS_COMPILE); fi; \
		make XEN_TARGET_ARCH="$${A}" CROSS_COMPILE=$$C- clean run-tests; \
		PASSES="$${PASSES} $${A}"; \
	done;\
	echo "$@ successful for:$${PASSES} $(XEN_TARGET_ARCH)";\
	[ -z "$${SKIPPED_ARCHS}" ] || echo "Skipped architectures:$${SKIPPED_ARCHS}"
endif

.PHONY: clean
.NOTPARALLEL: clean
clean:
	$(RM) -- *.o $(TARGETS) $(DEPS_RM)

.PHONY: distclean
distclean: clean
	$(RM) -- *~

.PHONY: install
install: all
	$(INSTALL_DIR) $(DESTDIR)$(LIBEXEC)/tests
	$(INSTALL_PROG) $(TARGETS) $(DESTDIR)$(LIBEXEC)/tests

.PHONY: uninstall
uninstall:
	$(RM) -- $(patsubst %,$(DESTDIR)$(LIBEXEC)/tests/%,$(TARGETS))

# CFLAGS for building the tests
XEN_INCLUDE_ARCH := $(subst x86_64,x86,$(XEN_COMPILE_ARCH))
CFLAGS += -D__XEN_TOOLS__
CFLAGS += $(APPEND_CFLAGS)
CFLAGS += -I$(XEN_ROOT)/xen/include
CFLAGS += -I$(XEN_ROOT)/xen/arch/$(XEN_INCLUDE_ARCH)/include
CFLAGS += $(CFLAGS_xeninclude)

# Enable sanitizers to catch memory errors and undefined behavior in the code
# for x86_64. Other architectures do not support -fstatic with it.
ifeq ($(XEN_TARGET_ARCH),x86_64)
CFLAGS += -fsanitize=address -fsanitize=undefined -fno-common
endif

# Build rules for the tests
$(TARGETS): %: %.o $(LIB_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $(APPEND_CFLAGS)

-include $(DEPS_INCLUDE)
