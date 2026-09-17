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
#ifndef TOOLS_TESTS_NATIVE_HARNESS_XENCTRL_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_XENCTRL_SHIM_H
#ifdef TEST_WRAP_TOOLS_INCLUDE_XENCTRL_H
#include "common.h"

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
#define XC_PRIVATE_H

/* Include xenctrl.h into the test context for libxc integration testing */
#define __XEN_KEXEC_H__
#include <public/version.h>
#include <public/kexec.h>

/* xenctrl.h conflicts with the Xen hypervisor define, it should be renamed */
#define XEN_INVALID_MFN _mfn(~0UL)
#undef INVALID_MFN
#define XEN_BARRIER_H /* riscv is missing in tools/include/xen-barrier.h */
#include <xenctrl.h>
#undef INVALID_MFN
#define INVALID_MFN XEN_INVALID_MFN

struct xc_interface_core {
    void *xcall;
};

/* Provision a xc_interface handle for the test context */
static xc_interface test_xc_handle, *xch = &test_xc_handle;

enum {
    XC_HYPERCALL_BUFFER_BOUNCE_NONE = 0,
    XC_HYPERCALL_BUFFER_BOUNCE_IN   = 1,
    XC_HYPERCALL_BUFFER_BOUNCE_OUT  = 2,
    XC_HYPERCALL_BUFFER_BOUNCE_BOTH = 3,
};

xc_hypercall_buffer_t XC__HYPERCALL_BUFFER_NAME(HYPERCALL_BUFFER_NULL) =
{
    .hbuf = NULL,
    .param_shadow = NULL,
    HYPERCALL_BUFFER_INIT_NO_BOUNCE
};

#define PERROR(_m, _a ...) ((void)0)
#define DPRINTF(_m, _a ...) ((void)0)

#define DECLARE_NAMED_HYPERCALL_BOUNCE(_name, _ubuf, _sz, _dir)    \
        xc_hypercall_buffer_t XC__HYPERCALL_BUFFER_NAME(_name) = { \
            .hbuf = NULL,                                          \
            .param_shadow = NULL,                                  \
            .dir = (_dir),                                         \
            .ubuf = (_ubuf),                                       \
        }

#define DECLARE_HYPERCALL_BOUNCE(_ubuf, _sz, _dir) \
        DECLARE_NAMED_HYPERCALL_BOUNCE(_ubuf, _ubuf, _sz, _dir)

#define HYPERCALL_BOUNCE_SET_SIZE(b, s) ((HYPERCALL_BUFFER(b))->sz = (s))

void *xc__hypercall_buffer_alloc(xc_interface *xch,
                                 xc_hypercall_buffer_t *b,
                                 size_t size)
{
    return b->hbuf;
}

void xc__hypercall_buffer_free(xc_interface *xch,
                               xc_hypercall_buffer_t *b)
{
}

void *xc__hypercall_buffer_alloc_pages(xc_interface *xch,
                                       xc_hypercall_buffer_t *b,
                                       int nr_pages)
{
    return xc__hypercall_buffer_alloc(xch, b,
                                      XC_PAGE_SIZE);
}

void xc__hypercall_buffer_free_pages(xc_interface *xch,
                                     xc_hypercall_buffer_t *b,
                                     int nr_pages)
{
}

static inline int xc__hypercall_bounce_pre(xc_interface *xch,
                                           xc_hypercall_buffer_t *b)
{
    if ( b->ubuf == (void *)-1 || b->dir == XC_HYPERCALL_BUFFER_BOUNCE_NONE )
        abort();
    b->hbuf = b->ubuf;
    return 0;
}

static inline void xc__hypercall_bounce_post(xc_interface *xch,
                                             xc_hypercall_buffer_t *b)
{
        abort();
}

#define xc_hypercall_bounce_pre(_xch, _name) \
        xc__hypercall_bounce_pre(_xch, HYPERCALL_BUFFER(_name))

#define xc_hypercall_bounce_post(_xch, _name) \

static inline int xencall2(void *xcall, unsigned int op,
                           unsigned long arg1, unsigned long arg2)
{
    return -1;
}

int xc_get_cpumap_size(xc_interface *xch)
{
    return 1;
}

int xc_get_nodemap_size(xc_interface *xch)
{
    return sizeof(unsigned long);
}

static inline bool xc_core_arch_auto_translated_physmap(
    const xc_domaininfo_t *info)
{
    return false;
}
#endif
#endif
