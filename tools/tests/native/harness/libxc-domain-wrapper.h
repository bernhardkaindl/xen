/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Wrapper around tools/libs/ctrl/xc_domain.c for the allocator test harness.
 *
 * The test framework includes the real xc_domain.c directly in its
 * translation unit, together with a small shim for the libxc internals it
 * uses and a proxy that redirects do_domctl(xch, domctl) to the wrapped Xen
 * domctl handler already present in this harness.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_LIBXC_DOMAIN_WRAPPER_H
#define TOOLS_TESTS_NATIVE_HARNESS_LIBXC_DOMAIN_WRAPPER_H
#ifdef TEST_WRAP_TOOLS_LIBS_CTRL_XC_DOMAIN_C
#define TEST_WRAP_XEN_COMMON_DOMCTL_C
#include "domctl-wrapper.h"
#include "libxc-domain-shim.h"
static xc_interface test_xc_handle, *xch = &test_xc_handle;

/* Hypercall passthrough of xc's do_domctl() to the hypervisor's do_domctl() */
static inline int do_domctl_hypervisor_passthrough(xc_interface *xch,
                                                   struct xen_domctl *domctl)
{
    /* Convert from domctl pointer to xen_domctl_t handle */
    union {
        XEN_GUEST_HANDLE_PARAM(xen_domctl_t) handle;
        struct xen_domctl *ptr;
    } u = { .ptr = domctl };

    domctl->interface_version = XEN_DOMCTL_INTERFACE_VERSION;
    ASSERT(xch == &test_xc_handle);
    return do_domctl(u.handle); /* Call xen/common/domctl.c's do_domctl() */
}

/* Make do_domctl() calls in libs/ctrl/xc_domain.c use the passthrough */
#define do_domctl do_domctl_hypervisor_passthrough

/* Include the real tools/libs/ctrl/xc_domain.c */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <../libs/ctrl/xc_domain.c>
#pragma GCC diagnostic pop
#undef do_domctl

/* Test helper for asserting expected claim sets via xc_domain_claim_memory */
void assert_claims_via_domctl(xc_interface *xch, struct domain *d,
                              uint32_t expected_entries,
                              xen_memory_claim_t *expected_claims,
                              const char *file, int line, const char *func)
{
    uint32_t entries = expected_entries;
    xen_memory_claim_t claim_set[expected_entries];
    int ret = xc_domain_claim_memory(xch, d->domain_id,
                                     XEN_DOMCTL_CLAIM_MEMORY_GET,
                                     &entries, claim_set);

    testcase_assert(ret == 0, file, line, func, "XEN_DOMCTL_CLAIM_MEMORY_GET");
    testcase_assert(entries == expected_entries, file, line, func,
                    "Got %u entries", entries);

    for ( uint32_t i = 0; i < expected_entries; i++ )
        testcase_assert(claim_set[i].pages == expected_claims[i].pages &&
                        claim_set[i].target == expected_claims[i].target,
                        file, line, func,
                        "Entry %u: node %u with %" PRIu64 " pages",
                        i, claim_set[i].target, claim_set[i].pages);
}
#define CLAIMS(d, claim_set)                                               \
        assert_claims_via_domctl(xch, d, ARRAY_SIZE(claim_set), claim_set, \
                                 __FILE__, __LINE__, __func__)
#endif
#endif