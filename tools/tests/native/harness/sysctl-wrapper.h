/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Wrapper around xen/common/sysctl.c for the allocator test framework.
 *
 * Includes the real sysctl.c directly in its translation unit.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_WRAPPER_H
#define TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_WRAPPER_H
#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C

#include "sysctl-shim.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../../xen/common/sysctl.c"
#pragma GCC diagnostic pop

#define TEST_WRAP_TOOLS_INCLUDE_XENCTRL_H
#include "xenctrl-shim.h"

int xc_sysctl(xc_interface *xch, struct xen_sysctl *sysctl)
{
    /* Convert from sysctl pointer to xen_sysctl_t handle */
    union {
        XEN_GUEST_HANDLE_PARAM(xen_sysctl_t) handle;
        struct xen_sysctl *ptr;
    } u = { .ptr = sysctl };

    sysctl->interface_version = XEN_SYSCTL_INTERFACE_VERSION;
    ASSERT(xch == &test_xc_handle);
    return do_sysctl(u.handle); /* Call xen/common/sysctl.c's do_sysctl() */
}

#endif
#endif
