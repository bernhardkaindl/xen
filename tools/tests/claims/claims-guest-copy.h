/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Check claims-array copy faults through domctl dispatch and libxc wrappers.
 * Sentinel and NULL guest buffers fail without accessing invalid memory.
 * Failed installs must leave no claims; failed reads must preserve existing
 * claims. Verify -1/errno propagation, count-only ENOBUFS copy-back, an
 * untouched output buffer on ENOBUFS, and a successful read after faults.
 * These are synthetic complete-copy failures, not real page faults.
 */
#define TEST_USES_LIBXENCTRL_DOMAIN_API
#include <native/init.h>

static void test_guest_copy_faults(int start_mfn)
{
    xen_domctl_memory_claim_t claim = {
        .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2,
    };
    struct xen_domctl op = {
        .cmd = XEN_DOMCTL_set_memory_claims,
        .domain = dom1->domain_id,
        .u.memory_claims.nr_claims = 1,
    };
    xen_domctl_memory_claim_t output, untouched;
    uint32_t count = 0;

    test_page_list_add_buddy(frame_table + start_mfn, order2);

    /* Bypass array bouncing to present the sentinel to copy_from_guest(). */
    op.u.memory_claims.claims.p = (void *)&native_invalid_guest_buffer;
    errno = 0;
    EQ(domctl_hypercall(xch, &op), -1);
    EQ(errno, EFAULT);
    EQ(outstanding_claims, 0);
    EQ(dom1->claims, NULL);

    /* NULL input with a nonzero count must fail without installing claims. */
    errno = 0;
    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 1), -1);
    EQ(errno, EFAULT);
    EQ(outstanding_claims, 0);
    EQ(dom1->claims, NULL);

    /* A valid install after the failures supplies the claim for read tests. */
    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, &claim, 1), 0);

    /* Zero capacity queries the count without copying any claim entries. */
    errno = 0;
    EQ(xc_domain_get_memory_claims(xch, dom1->domain_id, NULL, &count), -1);
    EQ(errno, ENOBUFS);
    EQ(count, 1);

    /* The sentinel now faults copy_to_guest(), leaving the claim intact. */
    op.cmd = XEN_DOMCTL_get_memory_claims;
    errno = 0;
    EQ(domctl_hypercall(xch, &op), -1);
    EQ(errno, EFAULT);
    EQ(outstanding_claims, 2);

    /* Nonzero capacity requires a writable array. */
    count = 1;
    errno = 0;
    EQ(xc_domain_get_memory_claims(xch, dom1->domain_id, NULL, &count), -1);
    EQ(errno, EFAULT);
    EQ(count, 1);
    EQ(outstanding_claims, 2);

    /* Insufficient capacity returns the count without changing the array. */
    memset(&untouched, 0xa5, sizeof(untouched));
    output = untouched;
    errno = count = 0;
    EQ(xc_domain_get_memory_claims(xch, dom1->domain_id, &output, &count), -1);
    EQ(errno, ENOBUFS);
    EQ(count, 1);
    EQ(memcmp(&output, &untouched, sizeof(output)), 0);

    /* Releasing the claim must still succeed after all failed reads. */
    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0), 0);
}

/*
 * Fail final domctl copy-back after a get would succeed or return ENOBUFS.
 * Test ten cases: two capacities and five copied-prefix lengths, including
 * zero bytes and prefixes ending before, within, and after nr_claims.
 * EFAULT must override either result, leave the caller's claims array
 * untouched despite any earlier copy into its bounce buffer, and preserve
 * claim accounting. Only a zero-byte failure promises the input count is
 * unchanged; no valid count is assumed after partial copy-back. A subsequent
 * successful read checks that the one-shot fault does not affect later calls.
 */
static void test_domctl_copyback_faults(int start_mfn)
{
    xen_domctl_memory_claim_t claim = {
        .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2,
    };
    const unsigned long count_offset =
        offsetof(struct xen_domctl, u.memory_claims.nr_claims);
    const unsigned long prefixes[] = {
        0, count_offset, count_offset + 1,
        count_offset + sizeof(uint32_t), sizeof(struct xen_domctl) - 1,
    };

    test_page_list_add_buddy(frame_table + start_mfn, order2);
    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, &claim, 1), 0);

    /* Capacity 0 takes ENOBUFS; capacity 2 first copies the single claim. */
    for ( unsigned int capacity = 0; capacity <= 2; capacity += 2 )
        for ( unsigned int prefix = 0; prefix < ARRAY_SIZE(prefixes); ++prefix )
        {
            xen_domctl_memory_claim_t output[2], untouched[2];
            uint32_t count = capacity;

            memset(output, 0xa5, sizeof(output));
            memcpy(untouched, output, sizeof(output));

            /* Fault only the final domctl write, not its initial read. */
            native_domctl_copyback_prefix = prefixes[prefix];
            native_domctl_copyback_fault = true;
            errno = 0;

            /* Final copy-back EFAULT overrides success or ENOBUFS. */
            EQ(xc_domain_get_memory_claims(xch, dom1->domain_id,
                                           output, &count), -1);
            EQ(errno, EFAULT);

            /* Neither the pending request nor its copy target may linger. */
            EQ(native_domctl_copyback_fault, false);
            EQ(native_copy_to_guest_fault, NULL);

            /* Libxc must discard bounced entries on any hypercall failure. */
            EQ(memcmp(output, untouched, sizeof(output)), 0);

            /* Failure to report a snapshot must not change the claims. */
            EQ(outstanding_claims, 2);

            /* Only zero copied bytes guarantee an unchanged input count. */
            if ( !prefixes[prefix] )
                EQ(count, capacity);
        }

    /* A later read must succeed without rearming or clearing the fault. */
    {
        xen_domctl_memory_claim_t output;
        uint32_t count = 1;

        EQ(xc_domain_get_memory_claims(xch, dom1->domain_id,
                                       &output, &count), 0);
        EQ(count, 1);
        EQ(output.pages, claim.pages);
        EQ(output.target, claim.target);
    }

    /* Claim release must remain usable after copy-back faults. */
    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0), 0);
}
