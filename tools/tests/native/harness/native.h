/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Main header of the test environment.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H
#define TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H

#include "common.h"

/*
 * The test environment needs the virtual address of the direct map which
 * the test environment emulates with the bss symbol directmap_virt_start
 * defined below.  In the real hypervisor, this is a fixed virtual address,
 * but in the test environment, it is allocated in BSS, so replace the fixed
 * virtual address definition with a dynamic one.
 *
 * For example, page_set_owner() and page_get_owner() are used to get and set
 * the virtual address of the owner domain of a page. They use pdx_to_virt()
 * and virt_to_pdx() which use DIRECTMAP_VIRT_START to translate between the
 * page_info struct's pdx-encoded owner field and the virtual address of
 * the owner domain, so setting DIRECTMAP_VIRT_START to the address of a
 * an early page-aligned symbol in the test binary (e.g. test_bss_start)
 * allows the test code to set up and get page ownership.
 */
void __aligned(PAGE_SIZE) *test_bss_start;

#ifdef TEST_ENABLE_XC_DOMAIN_C
#define TEST_WRAP_XEN_COMMON_SYSCTL_C
#endif

/* If sysctl.c is included, enable CONFIG_SYSCTL for it dependencies */
#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C
#define CONFIG_SYSCTL 1
#endif

/*
 * The test environment uses the real Xen page allocator, so include the
 * page-alloc-shim which provides the necessary supporting structures and
 * stubs for it to function correctly in the test environment.
 */
#define TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#include "page-alloc-shim.h"

#define hweightl(value) __builtin_popcountl(value)
#undef LITTLE_ENDIAN /* bitmap.c relies on __LITTLE_ENDIAN */
#include <common/bitmap.c>
#undef __XEN_TOOLS__
#include <lib/find-next-bit.c>
#define __XEN_TOOLS__
#include <lib/generic-ffsl.c>
#include <lib/generic-flsl.c>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include <common/page_alloc.c>
#pragma GCC diagnostic pop

#ifdef TEST_WRAP_XEN_COMMON_DOMCTL_C
#include "domctl-wrapper.h"
#endif

#ifdef TEST_ENABLE_XC_DOMAIN_C
#include "xc-domain-env.h"
#define TEST_WRAP_XEN_COMMON_MEMORY_C
#endif

#ifdef TEST_WRAP_XEN_COMMON_MEMORY_C
#include "memory-wrapper.h"
#endif

#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C
#include "sysctl-wrapper.h"
#endif

#ifdef TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#include "page-alloc-env.h"
#endif

#endif
