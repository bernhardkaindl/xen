XEN_ROOT = $(CURDIR)/../../../..
OCAML_TOPLEVEL = $(XEN_ROOT)/tools/ocaml

include $(OCAML_TOPLEVEL)/common.make

OCAMLINCLUDE += -I $(OCAML_TOPLEVEL)/libs/xc

# Keep this test binary free of debug info by default so split-debuginfo
# output is empty unless debug flags are explicitly re-added.
OCAMLOPTFLAGS := $(filter-out -g,$(OCAMLOPTFLAGS))
OCAMLCFLAGS := $(filter-out -g,$(OCAMLCFLAGS))
CFLAGS := $(filter-out -g,$(CFLAGS))

TARGET := test-ocaml-claim-memory

$(TARGET)_OBJS = test_ocaml_claim_memory
$(TARGET)_LIBS = \
	unix.cmxa \
	-ccopt -L -ccopt $(OCAML_TOPLEVEL)/libs/xc $(OCAML_TOPLEVEL)/libs/xc/xenctrl.cmxa \
	-ccopt -L -ccopt $(XEN_ROOT)/tools/libs/ctrl \
	-ccopt -L -ccopt $(XEN_ROOT)/tools/libs/guest

OCAML_PROGRAM = $(TARGET)
PROGRAMS = $(TARGET)

.PHONY: build-ocaml-xc
build-ocaml-xc:
	$(MAKE) -C $(OCAML_TOPLEVEL)/libs

.PHONY: all
all: build-ocaml-xc $(TARGET)

$(TARGET): | build-ocaml-xc

.PHONY: install
install: all
	$(INSTALL_DIR) $(DESTDIR)$(LIBEXEC)/tests
	$(INSTALL_PROG) -s $(TARGET) $(DESTDIR)$(LIBEXEC)/tests

.PHONY: uninstall
uninstall:
	$(RM) -- $(DESTDIR)$(LIBEXEC)/tests/$(TARGET)

include $(OCAML_TOPLEVEL)/Makefile.rules
