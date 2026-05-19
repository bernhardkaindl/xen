/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Provide basic macros expected by Xen hypervisor code for native compilation.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_XEN_MACROS_H
#define TOOLS_TESTS_NATIVE_HARNESS_XEN_MACROS_H

/*
 * Central list if glibc headers to include in the test context, to ensure
 * that the Xen-specific versions of certain functions are used.
 *
 * Standard ffs/ffsl and gcc's built-ins are signed, Xen's are unsigned
 */
#define ffs glibc_ffs
#define ffsl glibc_ffsl
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#undef ffs
#undef ffsl
/* xen/config.h defines these without value, glibc defines them with value */
#undef __LITTLE_ENDIAN
#undef __BIG_ENDIAN

/*
 * In Xen, STATIC_IF(x) and config_enabled(x) are defined in kconfig.h,
 * which cannot be included here. Define the required subset locally.
 */
#define STATIC_IF(option)        static_if(option)
#define static_if(value)         _static_if(__ARG_PLACEHOLDER_##value)
#define _static_if(arg1_or_junk) ___config_enabled(arg1_or_junk static, )
#define __ARG_PLACEHOLDER_1      0,
#define IS_ENABLED(opt)          config_enabled(opt)
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

/* Undefine conflicting macros from the Xen-tools headers. */
#undef BUILD_BUG_ON
#undef container_of
#undef MASK_INSR
#undef MASK_EXTR
#undef ROUNDUP
#undef ARRAY_SIZE
#undef min
#undef max
#undef min_t
#undef max_t
#undef __nonnull
#undef offsetof
#undef __AC
#undef _AC

/* Include Xen headers for compiler, macros, and page size definitions. */
#include <xen/compiler.h>
#include <xen/macros.h>
#include <xen/page-size.h>

/* Define Xen-specific macros and functions for the test environment. */
#define domain_crash(d)           ((void)(d))
#define PRI_mfn                   "05lx"
#define PRI_stime                 "lld"
#define dprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define gdprintk(level, fmt, ...) printk(fmt, ##__VA_ARGS__)
#define gprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define panic(fmt, ...)          (printk(fmt, ##__VA_ARGS__), abort())
#define printk(...)              (fflush(stdout), fprintf(stderr, __VA_ARGS__))
#define ACCESS_ONCE(x)           (x)

__attribute__((format(printf, 5, 6)))
static void testcase_assert(bool condition, const char *file, int line,
                            const char *func, const char *fmt, ...);

static int testcase_assert_expect_to_hit_bug;

/* The BUG() macro for the test environment */
#define BUG() test_bug(__FILE__, __LINE__, __func__)

/* If the test expects to hit a bug, log it. Otherwise assert and fail. */
static void test_bug(const char *file, int line, const char *func)
{
    fflush(stdout);
    if ( testcase_assert_expect_to_hit_bug )
    {
        printk("\n%s:%d: WE INVOKED a XEN BUG in %s()\n\n", file, line, func);
        testcase_assert_expect_to_hit_bug = false;
        return;
    }
    testcase_assert(false, file, line, func, "Unexpected XEN BUG WAS INVOKED");
}

/* The test environment needs to define these as __used */
#define __initdata
#define __init        __used
#define __initconst   __used
#define __initsetup   __used
#define __initcall(f) static int __used (*f##_ptr)(void) = (f)
#endif
