XEN_ROOT = $(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

SUBDIRS-y := $(patsubst %/Makefile,%,$(wildcard */Makefile))

.NOTPARALLEL:

.PHONY: all build clean distclean install uninstall
all build: $(if $(SUBDIRS-y),build-ocaml-libs) subdirs-all
clean distclean install uninstall: %: subdirs-%

.PHONY: build-ocaml-libs
build-ocaml-libs:
	$(MAKE) -s -C $(XEN_ROOT)/tools/ocaml/libs
