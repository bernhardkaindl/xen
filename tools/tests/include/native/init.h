/* SPDX-License-Identifier: GPL-2.0-only */
/* Main header of the native test harness */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H
#define TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H

#define CONFIG_NR_NUMA_NODES 64
#define CONFIG_NR_CPUS       128

#include "common.h"
static void *test_bss_start;

#define panic(fmt, ...)                      \
        do {                                 \
            printf(fmt "\n", ##__VA_ARGS__); \
            abort();                         \
        } while ( 0 )
#define ENSURE(cond, fmt, ...) do {                  \
            if ( !(cond)) panic(fmt, ##__VA_ARGS__); \
} while ( 0 )

#define EQ(actual, expected)                                            \
    ENSURE((actual) == (expected), "Assertion failed: %s == %s",         \
           #actual, #expected)
#define XE(xc_call, expected) EQ(native_xc_status(xc_call), expected)

static unsigned int test_functions_run;
static int test_complete(void)
{
    printf("Ran %u test cases.\n", test_functions_run);
    return 0;
}

#define TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#include "page-alloc-shim.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include <common/page_alloc.c>
#pragma GCC diagnostic pop

#ifdef CONFIG_NUMA
#include <native/numa-shim.h>
#include <common/numa.c>
#endif

#ifdef TEST_WRAP_XEN_COMMON_DOMCTL_C
#include "domctl-wrapper.h"
#endif

#ifdef TEST_USES_LIBXENCTRL_DOMAIN_API
#include "xc-domain-env.h"
#define TEST_WRAP_XEN_COMMON_MEMORY_C
#endif

#ifdef TEST_WRAP_XEN_COMMON_MEMORY_C
#include "memory-wrapper.h"
#endif

int xc_sysctl(xc_interface *xch, struct xen_sysctl *sysctl) { return 0; }

#ifdef TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#include "page-alloc-env.h"
#endif

#endif
