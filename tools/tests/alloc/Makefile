# SPDX-License-Identifier: GPL-2.0-only
# Makefile for tools/tests/alloc

XEN_ROOT=$(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

TEST_SOURCES := $(notdir $(wildcard test-*.c))
TARGETS := $(TEST_SOURCES:.c=)

.PHONY: all
all: $(TARGETS)

# Run the tests if possible, otherwise print a warning and skip them.
.PHONY: run
.NOTPARALLEL: run
run: $(TARGETS)
ifeq ($(CC),$(HOSTCC))
	@echo "Build configuration:"
	@echo "CC=$(CC)"
	@echo "CFLAGS='$(CFLAGS)'"
	@for test in $? ; do \
		echo "Running $(CURDIR)/$$test:"; \
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
else
	$(warning HOSTCC != CC, will not run test)
endif

.PHONY: clean
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
CFLAGS += $(CFLAGS_xeninclude) -Wunused-parameter

# Build rules for the tests
$(TARGETS): %: %.o $(LIB_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $(APPEND_CFLAGS)

-include $(DEPS_INCLUDE)
