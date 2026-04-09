/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common test harness for page allocation unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef _TEST_HARNESS_
#define _TEST_HARNESS_

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <xen-tools/common-macros.h>
#include <xen-tools/bitops.h>
/* Undefine conflicting macros defined by xen-tools headers */
#undef BITS_PER_LONG
#undef __LITTLE_ENDIAN
#undef __BIG_ENDIAN

/* Enable debug mode to enable additional checks */
#define CONFIG_DEBUG

/* Include the common check_asserts library for test assertions */
#include "check_asserts.h"

#define BUG()                     assert(false)
#define domain_crash(d)           ((void)(d))
#define IS_ALIGNED(x, a)          (!((x) & ((a) - 1)))
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
#define __LINUX_COMPILER_H
#define __section(name) __attribute__((section(#name)))
#define __initdata
#define cf_check      __used
#define __init        __used
#define __initcall(f) static int __used (*f##_ptr)(void) = (f)

/* Common Xen types for the test context */
typedef uint8_t       u8;
typedef uint16_t      domid_t;
typedef uint64_t      paddr_t;
typedef unsigned long cpumask_t;
typedef unsigned long nodemask_t;
typedef long long     s_time_t;
typedef bool          spinlock_t;

static const char *parse_args(int argc, char *argv[])
{
    const char *program_name = argv[0];

    if ( argc != 1 )
    {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        return NULL;
    }
    program_name = strrchr(program_name, '/');
    if ( program_name )
        program_name++;
    else
        program_name = argv[0];

    return program_name;
}
/*
 * The hypervisor's ffsl and flsl return unsigned int, which is relevant
 * for signed/unsigned conversion checking and type hints.
 */

/* Xen ffsl: Returns 1-based position of lowest set bit as unsigned int */
#undef ffsl /* tools/include/xen-tools/bitops.h returns signed int */
#define ffsl(x) ((unsigned int)__builtin_ffsl(x))

/* Xen flsl: Return 1-based position of highest set bit as unsigned int */
#define flsl(x) ((unsigned int)((x) ? BITS_PER_LONG - __builtin_clzl(x) : 0))

#endif /* _TEST_HARNESS_H_ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
