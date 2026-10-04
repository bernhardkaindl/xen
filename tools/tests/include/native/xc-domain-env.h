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
#define EQ_CLAIMS(d, expected)                                               \
        do {                                                                 \
            uint32_t _eq_entries = ARRAY_SIZE(expected);                     \
            uint32_t _eq_got = _eq_entries;                                  \
            xen_domctl_memory_claim_t _eq_set[_eq_entries];                  \
            int _eq_ret = xc_domain_get_memory_claims((xch), (d)->domain_id, \
                                                      &_eq_got, _eq_set);    \
            assert(_eq_ret == 0);                                            \
            assert(_eq_got == _eq_entries);                                  \
            for ( uint32_t _eq_i = 0; _eq_i < _eq_entries; _eq_i++ )         \
            assert(_eq_set[_eq_i].pages == (expected)[_eq_i].pages &&        \
                   _eq_set[_eq_i].target == (expected)[_eq_i].target);       \
        } while ( 0 )
#endif
#endif
