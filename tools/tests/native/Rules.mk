# SPDX-License-Identifier: GPL-2.0-only

# Rules.mk for native tests.
include $(XEN_ROOT)/tools/Rules.mk
srctree = $(XEN_ROOT)/xen

XEN_INCLUDE_ARCH = $(subst _,,$(subst 32,,$(subst 64,,$(XEN_TARGET_ARCH))))
CFLAGS_xeninclude_arch := -I$(srctree)/arch/$(XEN_INCLUDE_ARCH)/include
CFLAGS_xeninclude_xen = -I$(srctree)/include $(CFLAGS_xeninclude_arch)
CFLAGS_xeninclude_xen += -I$(srctree)
CFLAGS_tests_native += -I$(XEN_ROOT)/tools/tests/include
