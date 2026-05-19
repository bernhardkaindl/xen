/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal shim to include tools/libs/ctrl/xc_domain.c in native tests.
 *
 * This shim provides the small subset of libxc internals that xc_domain.c
 * needs so the allocator harness can exercise libxc wrapper paths without a
 * live hypervisor.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_LIBXC_DOMAIN_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_LIBXC_DOMAIN_SHIM_H

#ifndef TEST_WRAP_TOOLS_LIBS_CTRL_XC_DOMAIN_C
#warning "Include this header only in integration tests using xc_domain.c"
#else

#ifndef TEST_WRAP_XEN_COMMON_DOMCTL_C
#error "xc_domain.c wrapper requires the domctl.c wrapper"
#endif

#include <errno.h>
#include <stdlib.h>

/* Include xenctrl.h into the test context for libxc integration testing */
#define __XEN_KEXEC_H__
#include <public/version.h>
#include <public/kexec.h>
#ifdef __riscv
#define XEN_BARRIER_H /* riscv code missing in tools/include/xen-barrier.h */
#endif
/* xencntrl.h conflicts with the Xen hypervisor define, it should be renamed */
#define XEN_INVALID_MFN _mfn(INVALID_MFN_RAW)
#undef INVALID_MFN
#include <xenctrl.h>
#undef INVALID_MFN
#define INVALID_MFN XEN_INVALID_MFN

#define XC_PRIVATE_H

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

struct xc_interface_core {
    void *xcall;
};

#define PERROR(_m, _a ...) ((void)0)
#define DPRINTF(_m, _a ...) ((void)0)

#define DECLARE_NAMED_HYPERCALL_BOUNCE(_name, _ubuf, _sz, _dir)    \
        xc_hypercall_buffer_t XC__HYPERCALL_BUFFER_NAME(_name) = { \
            .hbuf = NULL,                                          \
            .param_shadow = NULL,                                  \
            .sz = (_sz),                                           \
            .dir = (_dir),                                         \
            .ubuf = (_ubuf),                                       \
        }

#define DECLARE_HYPERCALL_BOUNCE(_ubuf, _sz, _dir) \
        DECLARE_NAMED_HYPERCALL_BOUNCE(_ubuf, _ubuf, _sz, _dir)

#define HYPERCALL_BOUNCE_SET_SIZE(_buf, _sz)      \
        do {                                      \
            (HYPERCALL_BUFFER(_buf))->sz = (_sz); \
        } while ( 0 )

void *xc__hypercall_buffer_alloc(xc_interface *xch,
                                 xc_hypercall_buffer_t *b,
                                 size_t size)
{
    (void)xch;

    b->hbuf = calloc(size ? size : 1, 1);

    return b->hbuf;
}

void xc__hypercall_buffer_free(xc_interface *xch,
                               xc_hypercall_buffer_t *b)
{
    (void)xch;

    free(b->hbuf);
    b->hbuf = NULL;
}

void *xc__hypercall_buffer_alloc_pages(xc_interface *xch,
                                       xc_hypercall_buffer_t *b,
                                       int nr_pages)
{
    return xc__hypercall_buffer_alloc(xch, b,
                                      (size_t)(nr_pages ? nr_pages : 1) *
                                      XC_PAGE_SIZE);
}

void xc__hypercall_buffer_free_pages(xc_interface *xch,
                                     xc_hypercall_buffer_t *b,
                                     int nr_pages)
{
    (void)nr_pages;
    xc__hypercall_buffer_free(xch, b);
}

static inline int xc__hypercall_bounce_pre(xc_interface *xch,
                                           xc_hypercall_buffer_t *b)
{
    (void)xch;

    if ( b->ubuf == (void *)-1 || b->dir == XC_HYPERCALL_BUFFER_BOUNCE_NONE )
        abort();

    b->hbuf = b->ubuf;

    return 0;
}

static inline void xc__hypercall_bounce_post(xc_interface *xch,
                                             xc_hypercall_buffer_t *b)
{
    (void)xch;

    if ( b->ubuf == (void *)-1 || b->dir == XC_HYPERCALL_BUFFER_BOUNCE_NONE )
        abort();

    b->hbuf = NULL;
}

#define xc_hypercall_bounce_pre(_xch, _name) \
        xc__hypercall_bounce_pre(_xch, HYPERCALL_BUFFER(_name))

#define xc_hypercall_bounce_post(_xch, _name) \
        xc__hypercall_bounce_post(_xch, HYPERCALL_BUFFER(_name))

int xc_sysctl(xc_interface *xch, struct xen_sysctl *sysctl)
{
    (void)xch;
    (void)sysctl;
    errno = EOPNOTSUPP;
    return -1;
}

static inline int xencall2(void *xcall, unsigned int op,
                           unsigned long arg1, unsigned long arg2)
{
    (void)xcall;
    (void)op;
    (void)arg1;
    (void)arg2;
    errno = EOPNOTSUPP;
    return -1;
}

int xc_get_cpumap_size(xc_interface *xch)
{
    (void)xch;
    return 1;
}

int xc_get_nodemap_size(xc_interface *xch)
{
    (void)xch;
    return sizeof(unsigned long);
}

static inline bool xc_core_arch_auto_translated_physmap(
    const xc_domaininfo_t *info)
{
    (void)info;
    return false;
}

#endif

#endif