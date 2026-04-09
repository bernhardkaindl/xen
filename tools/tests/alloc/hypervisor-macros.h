/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common macros and definitions for building host-side unit tests
 * the Xen hypervisor.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef _TEST_ALLOC_XEN_MACROS_
#define _TEST_ALLOC_XEN_MACROS_

/*
 * In Xen, STATIC_IF(x) and config_enabled(x) are defined in kconfig.h
 * which we cannot include, so we need to define the necessary macros.
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
 * We include common-macros.h to reuse the Xen-tools macros, which are
 * not necessarily the same as the Xen hypervisor macros, but are close
 * enough for the test context.
 */
#include <xen-tools/common-macros.h>

/*
 * Define the header guards of the Xen headers that the Xen hypervisor
 * variants of the definitions in common-macros.h and bitops.h to prevent
 * conflicting definitions from those headers that prevent clean compilation.
 */
#define __XEN_CONST_H__
#define __MACROS_H__

/*
 * We also define the Xen hypervisor macros that are used by page_alloc.c
 * but not defined by common-macros.h, but needed to build hypervisor code
 * in the test context, such as IS_ALIGNED() and the ffsl/flsl macros.
 */
#define IS_ALIGNED(x, a) (!((x) & ((a) - 1)))

/*
 * Inclde the Xen-tools bitops.h to reuse the bitops from the tools side.
 * They are not necessarily the same as the Xen hypervisor bitops, but are
 * close enough for the test context.
 */
#include <xen-tools/bitops.h>
/*
 * Afer including the Xen-tools bitops.h, we need to redefine the ffsl and flsl
 * macros to match the behavior of the Xen hypervisor's ffsl and flsl, which
 * return unsigned int and are relevant for signed/unsigned conversion checking
 * and type hints in the test context.
 * And we need to undefine conflicting macros defined by xen-tools headers.
 */
#undef BITS_PER_LONG
#undef __LITTLE_ENDIAN
#undef __BIG_ENDIAN
/* Xen ffsl returns 1-based position of lowest set bit as unsigned int */
#undef ffsl /* tools/include/xen-tools/bitops.h returns signed int */
#define ffsl(x) ((unsigned int)__builtin_ffsl(x))
/* Xen flsl returns 1-based position of highest set bit as unsigned int */
#define flsl(x) ((unsigned int)((x) ? BITS_PER_LONG - __builtin_clzl(x) : 0))

/* Common assertion and logging macros */
#define BUG()                     assert(false)
#define domain_crash(d)           ((void)(d))
#define PRI_mfn                   "lu"
#define PRI_stime                 "lld"
#define printk                    printf
#define dprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define gdprintk(level, fmt, ...) printk(fmt, ##__VA_ARGS__)
#define gprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define panic(fmt, ...)                      \
    do                                       \
    {                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        abort();                             \
    } while ( 0 )

/* Support including xen/sections.h and other function attributes */
#define __initdata
#define __init        __used
#define __initcall(f) static int __used (*f##_ptr)(void) = (f)

#endif /* _TEST_ALLOC_XEN_MACROS_ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
