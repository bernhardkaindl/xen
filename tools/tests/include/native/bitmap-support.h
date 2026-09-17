/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Shared support for native Xen bitmap tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_BITMAP_SUPPORT_H
#define TOOLS_TESTS_NATIVE_HARNESS_BITMAP_SUPPORT_H

#include "xen-macros.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Heap allocator stubs. */
#define __XMALLOC_H__
#define xmalloc(type)                calloc(1, sizeof(type))
#define xmalloc_array(type, nr)      calloc((nr), sizeof(type))
#define xzalloc_array(type, nr)      calloc((nr) ? (nr) : 1, sizeof(type))
#define xvzalloc_array(type, nr)     calloc((nr), sizeof(type))
#define xvmalloc_array(type, ...)     _xvmalloc_impl(type, __VA_ARGS__, 1)
#define _xvmalloc_impl(t, a, b, ...) calloc((a) * (b), sizeof(t))
#define xzalloc(type)                calloc(1, sizeof(type))
#define _xmalloc(size, align)        ((void)align, xmalloc(size))
#define _xzalloc(size, align)        ((void)align, xmalloc(size))
#define xfree(p)                     free(p)
#define xvfree(p)                    free(p)

#ifndef ASSERT
#define ASSERT(condition)            assert(condition)
#endif
#ifndef ASSERT_UNREACHABLE
#define ASSERT_UNREACHABLE()         assert(0)
#endif
#ifndef BUG_ON
#define BUG_ON(condition)            assert(!(condition))
#endif

/* Keep bitmap.c independent of Xen's full lib and guest-access implementations. */
#define __LIB_H__
#define _LINUX_INIT_H
#define XEN_SELF_TESTS_H
#define __X86_UACCESS_H__
#define __ASM_X86_GUEST_ACCESS_H__

static inline unsigned long raw_copy_to_guest(void *to, const void *from,
                                              unsigned int len)
{
    memcpy(to, from, len);
    return 0;
}
#define __raw_copy_to_guest raw_copy_to_guest

static inline unsigned long raw_copy_to_guest_flush_dcache(
    void *to, const void *from, unsigned int len)
{
    memcpy(to, from, len);
    return 0;
}

static inline unsigned long raw_copy_from_guest(void *to, const void *from,
                                                unsigned int len)
{
    memcpy(to, from, len);
    return 0;
}
#define __raw_copy_from_guest raw_copy_from_guest

static inline unsigned long raw_clear_guest(void *to, unsigned int len)
{
    memset(to, 0, len);
    return 0;
}

#endif
