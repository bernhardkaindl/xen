/* SPDX-License-Identifier: GPL-2.0-only */
/* Main header of the native test harness */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H
#define TOOLS_TESTS_NATIVE_HARNESS_NATIVE_H

#define CONFIG_NR_NUMA_NODES 64
#define CONFIG_NR_CPUS       128

#include "common.h"
void __aligned(PAGE_SIZE) *test_bss_start;

#ifdef TEST_USES_LIBXENCTRL_DOMAIN_API
#define TEST_WRAP_XEN_COMMON_SYSCTL_C
#endif

#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C
#define CONFIG_SYSCTL 1
#endif

#define panic(fmt, ...) \
    do { \
        printf(fmt "\n", ##__VA_ARGS__); \
        abort(); \
    } while ( 0 )
#define ENSURE(cond, fmt, ...) do { \
    if (!(cond)) panic(fmt, ##__VA_ARGS__); \
} while ( 0 )

static unsigned int test_functions_run;
static int test_complete(void)
{
    printf("Ran %u test functions.\n", test_functions_run);
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

#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C
#include "sysctl-wrapper.h"
#endif

#ifdef TEST_WRAP_XEN_COMMON_PAGE_ALLOC_C
#include "page-alloc-env.h"
#endif

#endif
