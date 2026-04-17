/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common macros and definitions for building host-side unit tests
 * for the Xen hypervisor.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_ALLOC_HYPERVISOR_MACROS_H
#define TOOLS_TESTS_ALLOC_HYPERVISOR_MACROS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*
 * In Xen, STATIC_IF(x) and config_enabled(x) are defined in kconfig.h,
 * which cannot be included here. Define the required subset locally.
 */
#define STATIC_IF(option)        static_if(option)
#define static_if(value)         _static_if(__ARG_PLACEHOLDER_##value)
#define _static_if(arg1_or_junk) ___config_enabled(arg1_or_junk static, )
#define __ARG_PLACEHOLDER_1      0,
#define config_enabled(cfg)      _config_enabled(cfg)
#define _config_enabled(value)   __config_enabled(__ARG_PLACEHOLDER_##value)

#define __config_enabled(arg1_or_junk) ___config_enabled(arg1_or_junk 1, 0)

#define ___config_enabled(__ignored, val, ...) val

/*
 * Reuse the Xen-tools macros from common-macros.h. They are not
 * necessarily identical to the hypervisor variants, but they are close
 * enough for this test environment.
 */
#include <xen-tools/common-macros.h>

/*
 * Predefine the include guards for Xen headers whose hypervisor
 * variants would otherwise conflict with definitions from
 * common-macros.h and bitops.h.
 */
#define __XEN_CONST_H__
#define __MACROS_H__

/*
 * Provide Xen hypervisor macros used by xen/common/page_alloc.c that
 * common-macros.h does not supply, or redefine them where the test
 * build must match hypervisor behavior.
 */
#define IS_ALIGNED(x, a) (!((x) & ((a) - 1)))

/*
 * Reuse the Xen-tools bit operations from bitops.h. They are not
 * necessarily identical to the hypervisor versions, but they are close
 * enough for this test environment.
 */
#include <xen-tools/bitops.h>
/*
 * After including Xen-tools bitops.h, redefine ffsl and flsl to match
 * Xen hypervisor behavior. Here they return unsigned int, which matters
 * for signed/unsigned conversion checks and type expectations. Also
 * undefine conflicting macros from the Xen-tools headers.
 */
#undef BITS_PER_LONG
#undef __LITTLE_ENDIAN
#undef __BIG_ENDIAN
/* Xen's ffsl returns 1-based position of lowest set bit as unsigned int */
#undef ffsl /* (tools/include/xen-tools/bitops.h returns signed int) */
#define ffsl(x) ((unsigned int)__builtin_ffsl(x))
/* Xen's flsl returns 1-based position of highest set bit as unsigned int */
#define flsl(x) ((unsigned int)((x) ? BITS_PER_LONG - __builtin_clzl(x) : 0))

static int testcase_assert_expect_to_hit_bug = 1;
static bool testcase_assert_expected_to_fail;
__attribute__((format(printf, 5, 6)))
static void testcase_assert(bool condition, const char *file, int line,
                            const char *func, const char *fmt, ...);

static void instrumented_bug(const char *file, int line, const char *func)
{
    if ( testcase_assert_expect_to_hit_bug )
    {
        testcase_assert_expect_to_hit_bug--;
        testcase_assert_expected_to_fail = true;
    }
    testcase_assert(false, file, line, func,
                    "\n  ========> XEN BUG in %s(), line %d <========\n", func,
                    line);
    testcase_assert_expected_to_fail = false;
}

__attribute__((format(printf, 1, 2)))
int printk(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if ( strncmp(fmt, "<0>", 3) == 0 )
    {
        fprintf(stderr, "\n  ========> XENLOG_ERR: ");
        fmt += 3;
    }
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    return 0;
}

/* Assertion and logging helpers shared across the tests. */
#define BUG()                     instrumented_bug(__FILE__, __LINE__, __func__)
#define domain_crash(d)           ((void)(d))
#define PRI_mfn                   "lu"
#define PRI_stime                 "lld"
#define dprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define gdprintk(level, fmt, ...) printk(fmt, ##__VA_ARGS__)
#define gprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define panic(fmt, ...)                          \
        do                                       \
        {                                        \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
            abort();                             \
        } while ( 0 )

/* Support including xen/sections.h and other function attributes */
#define __initdata
#define __init        __used
#define __initcall(f) static int __used (*f##_ptr)(void) = (f)

#endif /* TOOLS_TESTS_ALLOC_HYPERVISOR_MACROS_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
