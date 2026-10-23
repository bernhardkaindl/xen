XEN_ROOT := $(abspath $(CURDIR)/../../..)

NUMA_TESTS := node-claims.c

# NUMA tests depend on NUMA_H, so exclude them if NUMA_H is not present.
NUMA_H = arch/$(call get-srcarch,$(patsubst %-clang,%,$(1)))/include/asm/numa.h
define ARCH_template_extra
ifeq ($(wildcard $(srctree)/$(NUMA_H)),)
TEST_SOURCES_$(1) := $$(filter-out $(NUMA_TESTS),$$(TEST_SOURCES_$(1)))
endif
endef

include ../include/native/Rules.mk
