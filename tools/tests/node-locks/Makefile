XEN_ROOT = $(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

TARGET := test-node-locks

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	$(RM) -- *.o $(TARGET) $(DEPS_RM)

.PHONY: distclean
distclean: clean
	$(RM) -- *~

.PHONY: install
install: all
	$(INSTALL_DIR) $(DESTDIR)$(LIBEXEC)/tests
	$(INSTALL_PROG) $(TARGET) $(DESTDIR)$(LIBEXEC)/tests

.PHONY: uninstall
uninstall:
	$(RM) -- $(DESTDIR)$(LIBEXEC)/tests/$(TARGET)

CFLAGS += $(CFLAGS_xeninclude)
CFLAGS += $(CFLAGS_libxenctrl)
CFLAGS += $(APPEND_CFLAGS)

LDFLAGS += $(LDLIBS_libxenctrl)
LDFLAGS += -lpthread -lm
LDFLAGS += $(APPEND_LDFLAGS)

%.o: Makefile

$(TARGET): test-node-locks.o
	$(CC) -o $@ $< $(LDFLAGS)

-include $(DEPS_INCLUDE)
