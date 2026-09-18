/* SPDX-License-Identifier: GPL-2.0-only */
/* Configuration for natively compiled unit tests. */

#include <assert.h>
#include <errno.h>
#include <stdio.h>

#define CONFIG_DEBUG
#define CONFIG_NR_NUMA_NODES 64
#define CONFIG_NR_CPUS       128
#define CONFIG_MMU
#ifdef __arm__
#define CONFIG_ARM_32
#define CONFIG_PADDR_BITS 40
#endif
#ifdef __aarch64__
#define CONFIG_ARM_64
#define CONFIG_PADDR_BITS 48
#endif
#ifdef __riscv
#define CONFIG_RISCV_64
#define CONFIG_QEMU_PLATFORM
#endif

#define __XEN_KCONFIG_H
#define __XEN_PDX_H__
#include <xen/config.h>
#include <xen/mm-frame.h>
#include <xen/pfn.h>
#include <xen/sections.h>
#include <xen/types.h>
#pragma GCC visibility pop
