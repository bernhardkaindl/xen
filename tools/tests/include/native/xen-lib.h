/* SPDX-License-Identifier: GPL-2.0-only */
/* Host bit operation setup for native Xen tests.*/

#ifndef TOOLS_TESTS_INCLUDE_NATIVE_XEN_LIB_H
#define TOOLS_TESTS_INCLUDE_NATIVE_XEN_LIB_H

#define __LIB_H__
#define __XEN_ERRNO_H__
#define printk printf

/* Make stdout for printf line-buffered for all tests */
__attribute__((constructor)) static void setup_stdout(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
}

#define __XEN_BUG_H__
#define ASSERT(condition) assert(condition)
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

static unsigned char native_invalid_guest_buffer;

static inline unsigned long native_guest_copy(void *to, const void *from,
                                              unsigned long len,
                                              const void *guest)
{
    if ( !len )
        return 0;

    if ( !guest || guest == &native_invalid_guest_buffer )
        return len;

    __builtin_memcpy(to, from, len);
    return 0;
}

static const void *native_copy_to_guest_fault;
static unsigned long native_copy_to_guest_prefix;

static inline unsigned long native_guest_copy_to(void *to, const void *from,
                                                 unsigned long len)
{
    if ( native_copy_to_guest_fault && len &&
         to == native_copy_to_guest_fault )
    {
        unsigned long copied = native_copy_to_guest_prefix;

        ASSERT(copied < len);
        native_copy_to_guest_fault = NULL;

        if ( copied )
            __builtin_memcpy(to, from, copied);

        return len - copied;
    }

    return native_guest_copy(to, from, len, to);
}

#define raw_copy_to_guest(t, f, l) native_guest_copy_to(t, f, l)
#define raw_copy_from_guest(t, f, l) native_guest_copy(t, f, l, f)

#define _LINUX_INIT_H
#define __initdata
#define __init __used

#endif /* TOOLS_TESTS_INCLUDE_NATIVE_XEN_LIB_H */
