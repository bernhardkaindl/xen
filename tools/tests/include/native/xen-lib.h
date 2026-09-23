/* SPDX-License-Identifier: GPL-2.0-only */
/* Host bit operation setup for native Xen tests.*/

#ifndef TOOLS_TESTS_INCLUDE_NATIVE_XEN_LIB_H
#define TOOLS_TESTS_INCLUDE_NATIVE_XEN_LIB_H

#define __LIB_H__
#define __XEN_ERRNO_H__
#include <stdlib.h>
#define printk    printf

#define __XEN_BUG_H__
#define ASSERT(condition) assert(condition)
#define EQ(a, b) do { \
    unsigned long _a = (unsigned long)(a), _b = (unsigned long)(b); \
    fprintf(stderr, "Checking: %lu == %lu (%s == %s)\n",\
        _a, _b, #a, #b);\
    if (_a != _b) {\
        fprintf(stderr, "Assertion failed: %lu == %lu (%s == %s)\n",\
        _a, _b, #a, #b);\
        assert(_a == _b);\
} } while (0)
#define ASSERT_UNREACHABLE() assert(0)
#define BUG_ON(condition) assert(!(condition))

#define __XMALLOC_H__
#define _xmalloc(size, align) ((void)(align), calloc(1, (size)))
#define _xzalloc(size, align) ((void)(align), calloc(1, (size)))
#define xmalloc_array(type, nr) calloc((nr), sizeof(type))
#define xzalloc_array(type, nr) calloc((nr) ? (nr) : 1, sizeof(type))
#define xmalloc(type)           calloc(1, sizeof(type))
#define xvzalloc_array(t, nr)   calloc((nr), sizeof(t))
#define xvmalloc_array(t, ...)  __xvmimpl(t, __VA_ARGS__, 1)
#define __xvmimpl(t, a, b, ...) calloc((a) * (b), sizeof(t))
#define xzalloc(type)           calloc(1, sizeof(type))
#define xfree(ptr)              free(ptr)
#define xvfree(ptr)             free(ptr)

#define raw_copy_to_guest(t, f, l) (__builtin_memcpy(t, f, l), 0UL)
#define raw_copy_from_guest(t, f, l) (__builtin_memcpy(t, f, l), 0UL)

#define _LINUX_INIT_H
#define __initdata
#define __init __used

#endif /* TOOLS_TESTS_INCLUDE_NATIVE_XEN_LIB_H */
