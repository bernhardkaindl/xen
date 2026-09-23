/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal shim to include tools/libs/ctrl/xc_domain.c in native tests.
 *
 * This shim provides the small subset of libxc internals that xc_domain.c
 * needs so the allocator harness can exercise libxenctrl paths without
 * a live Xen instance.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_LIBXC_WRAPPER_H
#define TOOLS_TESTS_NATIVE_HARNESS_LIBXC_WRAPPER_H
#ifdef TEST_WRAP_TOOLS_INCLUDE_XENCTRL_H
#include <public/kexec.h>

/*
 * This file replaces the private APIs of xc_private.h. By doing this, we can
 * include xc_domain.c in the test context and test its functionality without
 * needing a live Xen instance. The functions defined here are minimal
 * implementations that allow the test cases to run without errors, while
 * still exercising the relevant code paths in xc_domain.c.
 *
 * This enables calling the Xen hypervisor code built into the test program
 * from the libxenctrl APIs inside the native test environment.
 */
struct xc_interface_core {
    void *xcall;
};
#define XC_PRIVATE_H
/* xenctrl.h conflicts with the Xen hypervisor define, it should be renamed */
#define XEN_INVALID_MFN _mfn(~0UL)
#undef INVALID_MFN
#define XEN_BARRIER_H /* riscv is missing in tools/include/xen-barrier.h */
#include <xenctrl.h>
#undef INVALID_MFN
#define INVALID_MFN XEN_INVALID_MFN

/* Provision a xc_interface handle for the test context */
static xc_interface test_xc_handle, *xch = &test_xc_handle;

enum {
    XC_HYPERCALL_BUFFER_BOUNCE_NONE = 0,
    XC_HYPERCALL_BUFFER_BOUNCE_IN   = 1,
    XC_HYPERCALL_BUFFER_BOUNCE_OUT  = 2,
    XC_HYPERCALL_BUFFER_BOUNCE_BOTH = 3,
};

xc_hypercall_buffer_t XC__HYPERCALL_BUFFER_NAME(HYPERCALL_BUFFER_NULL) = { };
#define DECLARE_NAMED_HYPERCALL_BOUNCE(_name, _ubuf, _sz, _dir)    \
        xc_hypercall_buffer_t XC__HYPERCALL_BUFFER_NAME(_name) = { \
            .dir = (_dir),                                         \
            .ubuf = (_ubuf),                                       \
        }
#define DECLARE_HYPERCALL_BOUNCE(_ubuf, _sz, _dir) \
        DECLARE_NAMED_HYPERCALL_BOUNCE(_ubuf, _ubuf, _sz, _dir)
#define HYPERCALL_BOUNCE_SET_SIZE(b, s) ((HYPERCALL_BUFFER(b))->sz = (s))
#define xc__hypercall_buffer_alloc(xch, b, size) \
        (b->hbuf = calloc(size ? size : 1, 1), (b)->hbuf)
#define xc__hypercall_buffer_free(xch, b) (free((b)->hbuf))
#define xc__hypercall_buffer_alloc_pages(xch, b, nr_pages) \
        xc__hypercall_buffer_alloc(xch, b, (nr_pages) * XC_PAGE_SIZE)
#define xc__hypercall_buffer_free_pages(xch, b, nr_pages) \
        xc__hypercall_buffer_free(xch, b)
#define xc_hypercall_bounce_pre(_xch, _name) \
        xc__hypercall_bounce_pre(_xch, HYPERCALL_BUFFER(_name))
#define xc__hypercall_bounce_pre(xch, b) (b->hbuf = b->ubuf, 0)
#define xc_hypercall_bounce_post(_xch, _name)
#define xencall2(c, op, a1, a2)  (-1)
#define xc_get_cpumap_size(xch)  1
#define xc_get_nodemap_size(xch) sizeof(unsigned long)
#define xc_core_arch_auto_translated_physmap(info) false
#define PERROR(_m, _a ...) panic(_m, ##_a)
#define DPRINTF(_m, _a ...) printf(_m, ##_a)
#endif
#endif
