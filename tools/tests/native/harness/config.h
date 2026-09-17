/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Configuration for natively compiled unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>

#define CONFIG_DEBUG
#define CONFIG_NR_NUMA_NODES 64
#define CONFIG_NR_CPUS 256
#define CONFIG_MMU
#ifdef __arm__
#define CONFIG_PADDR_BITS 40
#endif
#ifdef __aarch64__
#define CONFIG_PADDR_BITS 48
#endif
#ifdef __riscv
#define CONFIG_RISCV_64
#endif

#define __XEN_KCONFIG_H
#define __XEN_PDX_H__
#define __XEN_FRAME_NUM_H__
#include <xen/config.h>
#include <xen/pfn.h>
#include <xen/sections.h>
#include <xen/types.h>
#pragma GCC visibility pop

#define printk(...)     (fflush(stdout), fprintf(stderr, __VA_ARGS__))
#define panic(fmt, ...) (printk(fmt, ##__VA_ARGS__), abort())

#define __initdata
#define __init __used
