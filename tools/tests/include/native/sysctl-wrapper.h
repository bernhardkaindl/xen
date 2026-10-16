/* SPDX-License-Identifier: GPL-2.0-only */
/* Wrapper around xen/common/sysctl.c */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_WRAPPER_H
#define TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_WRAPPER_H
#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C

#include "sysctl-shim.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#include <common/sysctl.c>
#pragma GCC diagnostic pop
unsigned long __read_mostly cpu_khz;

#define TEST_WRAP_TOOLS_INCLUDE_XENCTRL_H
#include "libxc-wrapper.h"

/* Map libxc's xc_sysctl() to xen/common/sysctl.c's do_sysctl() */
int xc_sysctl(xc_interface *xch, struct xen_sysctl *sysctl)
{
    union {
        XEN_GUEST_HANDLE_PARAM(xen_sysctl_t) handle;
        struct xen_sysctl *ptr;
    } u = { .ptr = sysctl };

    ASSERT(xch == &test_xc_handle);
    sysctl->interface_version = XEN_SYSCTL_INTERFACE_VERSION;
    long ret = do_sysctl(u.handle);
    if ( ret < 0 )
    {
        errno = -ret;
        return -1;
    }
    return ret;
}
#endif
#endif
