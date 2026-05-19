# SPDX-License-Identifier: GPL-2.0-only

# Makefile for building and running the native tests.
XEN_ROOT := $(abspath $(CURDIR)/../../..)
obj      := .
src      := .
srctree  := $(XEN_ROOT)/xen
objtree  := $(XEN_ROOT)/xen
export srctree objtree

# Load Per-architecture CFLAGS
define LOAD_ARCH_CFLAGS
	CFLAGS :=
	include $(XEN_ROOT)/config/$(1).mk
	ARCH_CFLAGS_$(1) := $$(CFLAGS)
endef
$(foreach arch,$(TEST_ARCHS), \
	$(eval $(call LOAD_ARCH_CFLAGS,$(arch))))

# Include Xen rules and Kbuild infrastructure
include $(XEN_ROOT)/tools/Rules.mk
include $(XEN_ROOT)/xen/scripts/Kbuild.include

# For recursive make calls, don't print the Entering/Leaving messages here.
MAKEFLAGS += --no-print-directory

# Define SRCARCH and SUBPATH for the Kbuild infrastructure
define srcarch
$(subst _,,$(subst 32,,$(subst 64,,$(1))))
endef
SRCARCH := $(call srcarch,$(XEN_TARGET_ARCH))
SUBPATH := $(subst $(XEN_ROOT)/,,$(CURDIR))

# Common CFLAGS for building the tests
# filter-out -m64
COMMON_CFLAGS := $(filter-out $(ARCH_CFLAGS_$(XEN_TARGET_ARCH)),$(CFLAGS))
COMMON_CFLAGS += -D__XEN_TOOLS__ $(APPEND_CFLAGS)
COMMON_CFLAGS += -D_SUBPATH_=\"$(SUBPATH)\"
COMMON_CFLAGS += -I$(srctree)/include
COMMON_CFLAGS += $(CFLAGS_xeninclude)

ifdef TARGETS
# make test TARGETS="host-claims another-test"
TEST_SOURCES := $(addsuffix .c,$(TARGETS))
else
TEST_SOURCES := $(notdir $(wildcard *.c))
endif

# Requested architectures
TEST_ARCHS := x86_64 arm64 arm32 riscv64

# GNU target triples for clang and gcc
TARGET_TRIPLE_arm64   := aarch64-linux-gnu
TARGET_TRIPLE_arm32   := arm-linux-gnueabihf
TARGET_TRIPLE_x86_64  := x86_64-linux-gnu
TARGET_TRIPLE_riscv64 := riscv64-linux-gnu

ifeq ($(cross),y)
ifneq ($(filter test run,$(MAKECMDGOALS)),)
BINFMT_SUPP := $(if $(wildcard /etc/init.d/binfmt-support),1,0)
QEMU_BINFMT := $(if $(wildcard /usr/libexec/qemu-binfmt),1,0)
ifneq ($(BINFMT_SUPP)$(QEMU_BINFMT),11)
$(error "Cross-testing requires binfmt-support and qemu-user-static")
endif
endif
endif

# Detect and select the found cross compilers if enabled with cross=y
ifneq ($(cross),y)
 SUPPORTED_ARCHS := $(XEN_TARGET_ARCH)
else
 ifneq ($(clang),y)
  define HAVE_GCC
   $(if $(shell command -v $(TARGET_TRIPLE_$(1))-gcc 2>/dev/null),$(1))
  endef
 endif
 define HAVE_CLANG
  $(if $(shell echo 'int main(){}' | clang --target=$(TARGET_TRIPLE_$(1)) -x \
               c - -o /dev/null >/dev/null 2>&1 && echo y),$(1)-clang)
 endef
 SUPPORTED_ARCHS := $(foreach arch,$(TEST_ARCHS),$(call HAVE_GCC,$(arch))) \
		    $(foreach arch,$(TEST_ARCHS),$(call HAVE_CLANG,$(arch)))
endif
$(SUPPORTED_ARCHS):
	@$(MAKE) XEN_TARGET_ARCH=$(patsubst %-clang,%,$@) build-$@

$(addprefix make-output-,$(SUPPORTED_ARCHS)):
	@$(MAKE) XEN_TARGET_ARCH=$(patsubst %-clang,%,$(@:make-output-%=%)) \
		 output-$(@:make-output-%=%)

define ARCH_template
ARCH_$(1) := $$(patsubst %-clang,%,$(1))
SRCARCH_$(1) := $$(call srcarch,$$(ARCH_$(1)))
TEST_SOURCES_$(1) := $$(TEST_SOURCES)
ifneq ($$(ARCH_$(1)),x86_64)
# node-claims.c relies on NUMA (only available on x86_64 for now)
TEST_SOURCES_$(1) := $$(filter-out node-claims.c,$$(TEST_SOURCES_$(1)))
endif

OBJ_$(1) := $$(addprefix $(obj)/,$$(TEST_SOURCES_$(1):.c=.$(1).o))
BIN_$(1) := $$(addprefix $(obj)/,$$(TEST_SOURCES_$(1):.c=.$(1)))
OUTPUT_$(1) := $$(addprefix $(obj)/,$$(TEST_SOURCES_$(1):.c=.$(1).out))
ALL_BINS += $$(BIN_$(1))
ALL_OUTPUTS += $$(OUTPUT_$(1))

ifeq ($$(filter %-clang,$(1)),)
# GCC build
$$(OBJ_$(1)) $$(BIN_$(1)): CROSS_COMPILE := $$(TARGET_TRIPLE_$$(ARCH_$(1)))-
else
$$(OBJ_$(1)) $$(BIN_$(1)): CC := clang
$$(OBJ_$(1)) $$(BIN_$(1)): CLANG_FLAGS := -fdiagnostics-absolute-paths \
	--target=$$(TARGET_TRIPLE_$$(ARCH_$(1)))
endif

DEPS += $(subst ./,.,$(OBJ_$(1):.o=.o.d))
O_CFLAGS_$$(ARCH_$(1)) := $$(COMMON_CFLAGS) $$(ARCH_CFLAGS_$$(ARCH_$(1)))
O_CFLAGS_$$(ARCH_$(1)) += -I$(srctree)/arch/$$(SRCARCH_$(1))/include
O_CFLAGS_$$(ARCH_$(1)) += -I$(objtree)/arch/$$(SRCARCH_$(1))/include/generated

ifeq ($$(ARCH_$(1)),$(XEN_COMPILE_ARCH))
O_CFLAGS_$$(ARCH_$(1)) += -fsanitize=address -fno-omit-frame-pointer
else
O_CFLAGS_$$(ARCH_$(1)) += -static
endif

$(obj)/%.$(1).o: $(src)/%.c
	$(Q)$$(CC) $$(CLANG_FLAGS) -c -o $$@ $$< $$(O_CFLAGS_$$(ARCH_$(1)))

$(obj)/%.$(1): $(obj)/%.$(1).o
	$(Q)$$(CC) $$(CLANG_FLAGS) -o $$@ $$< \
		$$(LDFLAGS) $$(O_CFLAGS_$$(ARCH_$(1)))
	@echo "Built $(SUBPATH)/$$@" >> $$(obj)/$(1).result

# Create the .out files by running the test binaries and capturing their output.
$(obj)/%.$(1).out: $(obj)/%.$(1) FORCE
	@echo "\nRunning $$< $(RUN_ARGS)to generate $$@..."
	@./$$< $(RUN_ARGS) >$$@ 2>&1 || \
	 (sed 's/^/> /' $$@; echo "Test $$< failed"; exit 1)
	@echo "\n$(SUBPATH)/$$< was successful: $$< output:"
	@sed 's/^/| /' $$@
	@tail -1 $$@ >> $$(obj)/$(1).result
	@$(RM) $$< $$<.o*

# Can only be used with XEN_TARGET_ARCH set to the appropriate architecture:
# e.g. make XEN_TARGET_ARCH=arm64 build-arm64

.PHONY: $(objtree)/arch/$$(SRCARCH_$(1))/include/generated/asm
$(objtree)/arch/$$(SRCARCH_$(1))/include/generated/asm::
	$(Q)$(MAKE) $(asm-generic)=$$@ XEN_TARGET_ARCH=$$(ARCH_$(1))

.PHONY: setup-$(1)
setup-$(1): $(objtree)/arch/$$(SRCARCH_$(1))/include/generated/asm

.PHONY: build-$(1)
build-$(1): setup-$(1) $$(BIN_$(1))

.PHONY: output-$(1)
output-$(1): $$(OUTPUT_$(1))

.PHONY: clean-$(1)
clean-$(1): $(objtree)/arch/$$(SRCARCH_$(1))/include/generated/asm
	$(Q)$(RM) -- $$(OBJ_$(1)) $$(BIN_$(1)) $$(OUTPUT_$(1))
endef
$(foreach arch,$(SUPPORTED_ARCHS), $(eval $(call ARCH_template,$(arch))))

.PHONY: all run test
run test: all
	@:
# Allow the build to run in parallel with clean before and after, and reporting.
all: setup
	$(Q)$(MAKE) clean
	$(Q)$(MAKE) $(MFLAGS) $(SUPPORTED_ARCHS)
	@sort $(obj)/*.result
	@$(RM) $(obj)/*.result
# If the goal also has run or test in it, run the tests after building.
ifneq ($(filter run test,$(MAKECMDGOALS)),)
	$(Q)$(MAKE) $(addprefix make-output-,$(SUPPORTED_ARCHS))
	@sort $(obj)/*.result
	@$(RM) $(obj)/*.result ..d
endif

.PHONY: setup
setup: $(XEN_ROOT)/tools/include/xen/xen.h
$(XEN_ROOT)/tools/include/xen/xen.h:
	@$(MAKE) -C $(XEN_ROOT)/tools/include all

.PHONY: clean clean-archs
clean-archs: $(addprefix clean-,$(SUPPORTED_ARCHS))
clean: clean-archs
	@$(RM) -v -- *.o $(TARGETS) $(DEPS_RM)

.PHONY: postclean-test
postclean-test: test clean

.PHONY: distclean
distclean: clean
	$(Q)$(RM) -- *~

.PHONY: install
install: all
	@echo "install: No install; the source is tested, not another Xen host"

.PHONY: uninstall
uninstall:

-include $(DEPS)
