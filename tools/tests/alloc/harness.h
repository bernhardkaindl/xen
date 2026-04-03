/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common test harness support.
 *
 * Copyright (C) 2025 Cloud Software Group
 */

#ifndef _TEST_HARNESS_
#define _TEST_HARNESS_

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xen-tools/common-macros.h>
#include <xen-tools/bitops.h>

typedef bool spinlock_t;
typedef uint16_t domid_t;

#define BUG_ON(x)            assert(!(x))
#define ASSERT(x)            assert(x)
#define printk               printf
#define DOMID_INVALID        101
#define ASSERT_UNREACHABLE() assert(0)

static inline unsigned int find_next(const unsigned long *addr,
                                     unsigned int size, unsigned int off,
                                     bool value)
{
    unsigned int i;

    ASSERT(size <= BITS_PER_LONG);

    for ( i = off; i < size; i++ )
        if ( !!(*addr & (1UL << i)) == value )
            return i;

    return size;
}

static inline int parse_bool(const char *s, const char **e)
{
    (void)e;

    if ( !strcmp(s, "1") || !strcmp(s, "yes") || !strcmp(s, "true") )
        return 1;
    if ( !strcmp(s, "0") || !strcmp(s, "no") || !strcmp(s, "false") )
        return 0;

    return -1;
}

#define find_next_zero_bit(a, s, o) find_next(a, s, o, false)
#define find_next_bit(a, s, o)      find_next(a, s, o, true)

#define flsl(x) ((x) ? BITS_PER_LONG - __builtin_clzl(x) : 0)
#define ffsl(x) __builtin_ffsl(x)

#define SWAP(a, b)          \
    do                      \
    {                       \
        typeof(a) t_ = (a); \
        (a)          = (b); \
        (b)          = t_;  \
    } while ( 0 )

#define sort(elem, nr, size, cmp, swp) \
    ({                                 \
        (void)(swp);                   \
        qsort(elem, nr, size, cmp);    \
    })

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
