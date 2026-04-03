XEN_ROOT=$(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

TARGETS := test-reserve_offline_page

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
	$(RM) -- *.o $(TARGETS) $(DEPS_RM) pdx.h

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

pdx.h: $(XEN_ROOT)/xen/include/xen/pdx.h
	sed -e '/^#[[:space:]]*include/d' <$< >$@

CFLAGS += -D__XEN_TOOLS__
CFLAGS += $(APPEND_CFLAGS)
CFLAGS += -I$(XEN_ROOT)/xen/include
CFLAGS += -I$(XEN_ROOT)/xen/arch/x86/include
CFLAGS += $(CFLAGS_xeninclude)

test-reserve_offline_page: test-reserve_offline_page.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-error -Wno-unused-variable \
	      -Wno-unused-function -Wno-maybe-uninitialized -o $@ $< \
	      $(APPEND_CFLAGS)

-include $(DEPS_INCLUDE)
