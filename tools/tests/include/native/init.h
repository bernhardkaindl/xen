/* SPDX-License-Identifier: GPL-2.0-only */
/* Main header of the native test harness */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H
#define TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H

#include "common.h"
void __aligned(PAGE_SIZE) *test_bss_start;

#ifdef TEST_ENABLE_XC_DOMAIN_C
#define TEST_WRAP_XEN_COMMON_SYSCTL_C
#endif

#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C
#define CONFIG_SYSCTL 1
#endif

#define TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#include "page-alloc-shim.h"
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
