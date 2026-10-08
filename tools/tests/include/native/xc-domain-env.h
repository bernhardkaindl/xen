/* SPDX-License-Identifier: GPL-2.0-only */
/* Pass libxc's do_domctl() calls through to the hypervisor's do_domctl() */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_XC_DOMAIN_ENV_H
#define TOOLS_TESTS_NATIVE_HARNESS_XC_DOMAIN_ENV_H
#ifdef TEST_USES_LIBXENCTRL_DOMAIN_API

#define TEST_WRAP_XEN_COMMON_DOMCTL_C
#include "domctl-wrapper.h"

#define TEST_WRAP_TOOLS_INCLUDE_XENCTRL_H
#include "libxc-wrapper.h"

/* Map libs/ctrl/xc_domain.c:do_domctl() to xen/common/domctl.c:do_domctl() */
static inline int domctl_hypercall(xc_interface *xch,
                                   struct xen_domctl *domctl)
{
    /* Map libxc's struct xen_domctl to the hypervisor's xen_domctl_t handle */
    union {
        XEN_GUEST_HANDLE_PARAM(xen_domctl_t) handle;
        struct xen_domctl *ptr;
    } u = { .ptr = domctl };

    domctl->interface_version = XEN_DOMCTL_INTERFACE_VERSION;
    return do_domctl(u.handle); /* Call xen/common/domctl.c's do_domctl() */
}

/* Following libxc do_domctl() calls use the hypercall passthrough function */
#define do_domctl domctl_hypercall

/* Include the real tools/libs/ctrl/xc_domain.c */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <libs/ctrl/xc_domain.c>
#pragma GCC diagnostic pop

/* Assert xc_domain_get_memory_claims() matches expected claim set */
#define EQ_CLAIMS(d, expected)                                            \
        do {                                                              \
            uint32_t _entries = ARRAY_SIZE(expected);                     \
            uint32_t _got = _entries;                                     \
            xen_domctl_memory_claim_t _set[_entries];                     \
            int _ret = xc_domain_get_memory_claims((xch), (d)->domain_id, \
                                                    _set, &_got);         \
            assert(_ret == 0);                                            \
            assert(_got == _entries);                                     \
            for ( uint32_t _i = 0; _i < _entries; _i++ )                  \
            assert(_set[_i].pages == (expected)[_i].pages &&        \
                   _set[_i].target == (expected)[_i].target);       \
        } while ( 0 )
#endif
#endif
