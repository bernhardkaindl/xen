/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Wrapper around xen/common/memory.c for the allocator test environment.
 * It also provides the pass-through implementation of the xc_memory_op()
 * function, which is used by the memory-related hypercalls implemented in
 * xc_domain.c to call the memory operation handler in xen/common/memory.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_MEMORY_WRAPPER_H
#define TOOLS_TESTS_NATIVE_HARNESS_MEMORY_WRAPPER_H
#ifdef TEST_WRAP_XEN_COMMON_MEMORY_C
#include "memory-shim.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../../xen/common/memory.c"
#pragma GCC diagnostic pop

/*
 * Pass libxenctrl's xc_memory_op() calls to the Xen hypervisor's
 * do_memory_op() handler in xen/common/memory.c to run the memory
 * operations in the test environment.
 */
long xc_memory_op(xc_interface *xch, unsigned int cmd,
                  void *arg, size_t arg_size)
{
    union {
        XEN_GUEST_HANDLE_PARAM(void) handle;
        void *ptr;
    } u = { .ptr = arg };

    ASSERT(xch == &test_xc_handle);
    (void)arg_size;

    /* Call xen/common/memory.c's do_memory_op() handler */
    return do_memory_op(cmd, u.handle);
}

#endif
#endif
