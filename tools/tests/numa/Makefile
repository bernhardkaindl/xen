XEN_ROOT=$(CURDIR)/../../..
include $(XEN_ROOT)/tools/tests/native/Rules.mk

TARGETS := test-numa

.PHONY: all
all: $(TARGETS)

.PHONY: run
run: $(TARGETS)
ifeq ($(CC),$(HOSTCC))
	set -e;             \
	for test in $? ; do \
		./$$test ;  \
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

CFLAGS += -D__XEN_TOOLS__
CFLAGS += $(CFLAGS_xeninclude_xen)
CFLAGS += $(CFLAGS_xeninclude)
CFLAGS += $(CFLAGS_tests_native)

test-numa: test-numa.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(APPEND_CFLAGS)

-include $(DEPS_INCLUDE)
