XEN_ROOT = $(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

TARGETS := test-mem-claim test-claim-memory test-offline-online

LIB_OBJ := mem-claim-lib.o

.PHONY: all
all: $(TARGETS)

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
CFLAGS += $(CFLAGS_libxenguest)
CFLAGS += $(APPEND_CFLAGS)

LDFLAGS += $(LDLIBS_libxenctrl)
LDFLAGS += $(LDLIBS_libxenguest)
LDFLAGS += $(APPEND_LDFLAGS)

%.o: Makefile


$(TARGETS): %: %.o $(LIB_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(DEPS_INCLUDE)
