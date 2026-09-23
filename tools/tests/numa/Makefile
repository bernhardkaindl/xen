XEN_ROOT := $(abspath $(CURDIR)/../../..)
srctree  := $(XEN_ROOT)/xen
include $(XEN_ROOT)/tools/Rules.mk

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
CFLAGS += $(CFLAGS_xeninclude)
$(eval $(call native-test-cflags,CFLAGS,$(XEN_TARGET_ARCH)))

HAS_NUMA_H := $(wildcard $(srctree)/arch/$(include-arch)/include/asm/numa.h)
test-numa: test-numa.c
ifeq ($(HAS_NUMA_H),)
	@echo "Skipping test-numa build for architecture without asm/numa.h header"
else
	$(CROSS_COMPILE)$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(APPEND_CFLAGS)
endif

-include $(DEPS_INCLUDE)
