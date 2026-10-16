/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Comprehensive validation the XSM calls
 * of XEN_DOMCTL_set_memory_claims and XEN_DOMCTL_get_memory_claims:
 * A hook in xsm_domctl() checks it is given the correct arguments.
 * Tests allowing calls and denying the calls with expected error codes.
 */
#define TEST_USES_LIBXENCTRL_DOMAIN_API
#include <native/init.h>

static struct domain *claim_xsm_target;
static unsigned int claim_xsm_command, claim_xsm_calls;
static int claim_xsm_set_result, claim_xsm_get_result;

static int claim_xsm_decision(struct domain *d, struct xen_domctl *op)
{
    /* Authorization must receive the requested command and target domain. */
    EQ(d, claim_xsm_target);
    EQ(op->domain, d->domain_id);
    EQ(op->cmd, claim_xsm_command);
    claim_xsm_calls++;

    switch ( op->cmd )
    {
    case XEN_DOMCTL_set_memory_claims:
        return claim_xsm_set_result;

    case XEN_DOMCTL_get_memory_claims:
        return claim_xsm_get_result;

    default:
        ENSURE(false, "Unexpected claims XSM command");
        return -EACCES;
    }
}

/* Exercise the domctl XSM gate with injected decisions, not FLASK policy. */
static void test_claim_xsm_denials(int start_mfn)
{
    struct domain *domains[] = { dom1, dom2 };
    xen_domctl_memory_claim_t request = {
        .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2
    };

    test_page_list_add_buddy(frame_table + start_mfn, order2);

    for ( unsigned int dom = 0; dom < ARRAY_SIZE(domains); ++dom )
    {
        struct domain *d = domains[dom];
        xen_domctl_memory_claim_t get[2], untouched[2];
        uint32_t count = ARRAY_SIZE(get);

        claim_xsm_target = d;
        claim_xsm_calls = 0;
        claim_xsm_set_result = -EACCES;
        claim_xsm_get_result = 0;
        claim_xsm_command = XEN_DOMCTL_set_memory_claims;
        native_xsm_domctl_hook = claim_xsm_decision;

        /* Denial must precede the NULL array access and install no claims. */
        XE(xc_domain_set_memory_claims(xch, d->domain_id, NULL, 1), -EACCES);
        EQ(claim_xsm_calls, 1);
        EQ(outstanding_claims, 0);

        /* Allowing the next install must recover without resetting state. */
        claim_xsm_set_result = 0;
        EQ(xc_domain_set_memory_claims(xch, d->domain_id, &request, 1), 0);
        EQ(claim_xsm_calls, 2);

        /* Release also requires authorization; denial preserves the claim. */
        claim_xsm_set_result = -EACCES;
        XE(xc_domain_set_memory_claims(xch, d->domain_id, NULL, 0), -EACCES);
        EQ(claim_xsm_calls, 3);
        EQ(outstanding_claims, 2);

        /* A denied read must leave both the output count and array intact. */
        claim_xsm_command = XEN_DOMCTL_get_memory_claims;
        memset(get, 0xa5, sizeof(get));
        memcpy(untouched, get, sizeof(get));
        claim_xsm_get_result = -EPERM;
        XE(xc_domain_get_memory_claims(xch, d->domain_id, get, &count),
           -EPERM);
        EQ(claim_xsm_calls, 4);
        EQ(count, ARRAY_SIZE(get));
        EQ(memcmp(get, untouched, sizeof(get)), 0);

        /* Count-only queries must not bypass the same authorization gate. */
        count = 0;
        XE(xc_domain_get_memory_claims(xch, d->domain_id, NULL, &count),
           -EPERM);
        EQ(claim_xsm_calls, 5);
        EQ(count, 0);

        /* Reads may succeed while set is denied, without changing claims. */
        claim_xsm_get_result = 0;
        count = ARRAY_SIZE(get);
        EQ(xc_domain_get_memory_claims(xch, d->domain_id, get, &count), 0);
        EQ(claim_xsm_calls, 6);
        EQ(count, 1);
        EQ(get[0].target, request.target);
        EQ(get[0].pages, request.pages);
        EQ(outstanding_claims, 2);

        /* Release may succeed while get is denied and must clear claims. */
        claim_xsm_get_result = -EPERM;
        claim_xsm_set_result = 0;
        claim_xsm_command = XEN_DOMCTL_set_memory_claims;
        EQ(xc_domain_set_memory_claims(xch, d->domain_id, NULL, 0), 0);
        EQ(claim_xsm_calls, 7);
        EQ(outstanding_claims, 0);

        native_xsm_domctl_hook = NULL;
    }

    printf("Claims XSM gate: 14 native decisions passed.\n");
}
