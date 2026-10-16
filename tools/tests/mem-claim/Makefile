XEN_ROOT = $(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

TARGETS := test-mem-claim test-claim-memory

LIB_OBJ := libtestclaims.o

.PHONY: all
all: $(TARGETS)

#  Can also be called with "make run-tests-mem-claim" from the toplevel.
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

# The tests in this directory need to run in a privileged domain (Dom0)
# with libxenctrl and a matching the hypervisor to test running it.
# Therefore, they don't run with the native test target for build-time tests.
.PHONY: test
test:

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

CFLAGS += $(CFLAGS_xeninclude)
CFLAGS += $(CFLAGS_libxenctrl)
CFLAGS += $(APPEND_CFLAGS)

LDFLAGS += $(LDLIBS_libxenctrl)
LDFLAGS += $(APPEND_LDFLAGS)

%.o: Makefile


$(TARGETS): %: %.o $(LIB_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(DEPS_INCLUDE)
