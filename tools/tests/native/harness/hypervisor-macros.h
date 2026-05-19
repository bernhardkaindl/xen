/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common macros and definitions for building host-side unit tests
 * for the Xen hypervisor.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_HYPERVISOR_MACROS_H
#define TOOLS_TESTS_NATIVE_HARNESS_HYPERVISOR_MACROS_H

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

#include <xen/compiler.h>
#include <xen/macros.h>
#include <xen/page-size.h>

/* Undefine conflicting xen/xen-tools macros. */
#undef __LITTLE_ENDIAN
#undef __BIG_ENDIAN

#define BUG()                     test_bug(__FILE__, __LINE__, __func__)
#define domain_crash(d)           ((void)(d))
#define PRI_mfn                   "05lx"
#define PRI_stime                 "lld"
#define dprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define gdprintk(level, fmt, ...) printk(fmt, ##__VA_ARGS__)
#define gprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define panic(fmt, ...)          (printk(fmt, ##__VA_ARGS__), abort())
#define printk(...)              (fflush(stdout), fprintf(stderr, __VA_ARGS__))

__attribute__((format(printf, 5, 6)))
static void testcase_assert(bool condition, const char *file, int line,
                            const char *func, const char *fmt, ...);

static int testcase_assert_expect_to_hit_bug;
/* Implement the BUG() macro for the test environment */
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

#define __initdata
#define __init        __used
#define __initconst   __used
#define __initsetup   __used
#define __initcall(f) static int __used (*f##_ptr)(void) = (f)

#define ACCESS_ONCE(x) (x)

#define __X86_64_SYSTEM_H__
#define ASM_RISCV_SYSTEM_H

#ifndef __riscv
/* Minimal atomic_t and operations to avoid asm includes in the tests */
#define __ARCH_ARM_ATOMIC__
typedef struct { int counter; } atomic_t;
#define ATOMIC_INIT(i)         { (i) }
#define atomic_read(v)         ((v)->counter)
#define atomic_set(v, i)       ((v)->counter = (i))
#define _atomic_read(v)        ((v).counter)
#define _atomic_set(v, i)      ((v).counter = (i))
#define atomic_inc(v)          ((void)(v)->counter++)
#define atomic_dec(v)          ((void)(v)->counter--)
#define atomic_add(i, v)       ((void)((v)->counter += (i)))
#define atomic_sub(i, v)       ((void)((v)->counter -= (i)))
#define atomic_sub_return(i, v) ((v)->counter -= (i))
#define atomic_add_return(i, v) ((v)->counter += (i))
#define atomic_inc_return(v)   (++(v)->counter)
#define atomic_dec_return(v)   (--(v)->counter)
#define atomic_sub_and_test(i, v) (!((v)->counter -= (i)))
#define atomic_inc_and_test(v) (!(++(v)->counter))
#define atomic_dec_and_test(v) (!(--(v)->counter))
#define atomic_add_unless(v, a, u) \
        ({ int __r = (v)->counter; \
           if ( __r != (u) ) (v)->counter += (a); __r; })
#define atomic_inc_not_zero(v)     \
        ({ int __r = (v)->counter; \
           if ( __r ) { (v)->counter++; } __r; })
#define atomic_cmpxchg(v, old, new) \
        ({ int __r = (v)->counter;  \
           if ( __r == (old) ) { (v)->counter = (new); } __r; })
#define read_atomic(p)       (*(p))
#define write_atomic(p, v)   (*(p) = (v))
#endif
#endif
