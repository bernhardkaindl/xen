/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Integration tests for per-node memory claims with CONFIG_NUMA enabled.
 *
 * Glossary:
 * - Test oracle: a reference which determines the expected result, against
 *   which the result from the implementation under test is checked.
 * - Arithmetic oracle: a test oracle which calculates expected results from
 *   counts and rules, without calling the implementation under test to obtain
 *   them. Here, admission is predicted from free-page and claim counts, and
 *   redemption is predicted from claim counts and redemption priority.
 *
 * An oracle can contain mistakes too; passing tests establish agreement with
 * the reference model, not a proof that either is correct.
 */
#define CONFIG_NUMA 1
#include "claims-xsm-denials.h"
#include "claims-guest-copy.h"

static void test_redemption_basic(int start_mfn)
{
    xen_domctl_memory_claim_t claims[] = {
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2 },
        { .target = node0, .pages = 1 },
        { .target = node1, .pages = 3 },
    };

    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                   claims, ARRAY_SIZE(claims)), 0);
    EQ(total_avail_pages, 8);
    EQ(outstanding_claims, 6);
    EQ(node_claimed_pages[node0], 1);
    EQ(node_claimed_pages[node1], 3);

    ASSERT(alloc_domheap_pages(dom1, order2,
                               MEMF_node(node0) | MEMF_exact_node));
    EQ(total_avail_pages, 4);
    EQ(outstanding_claims, 2);
    EQ(node_claimed_pages[node0], 0);
    EQ(node_claimed_pages[node1], 2);

    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                   NULL, 0), 0);
    EQ(total_avail_pages, 4);
    EQ(outstanding_claims, 0);
    EQ(node_claimed_pages[node0], 0);
    EQ(node_claimed_pages[node1], 0);
}

static void test_xc_domain_set_memory_claims(int start_mfn)
{
    unsigned long expected_avail_pages = 8, expected_outstanding_claims = 6;
    xen_domctl_memory_claim_t set[3] = {
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };

    /* Prepare the free lists with the needed pages for the defined claims */
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    EQ(xc_domain_set_memory_claims(&test_xc_handle, dom1->domain_id, set,
                                   ARRAY_SIZE(set)), 0);
    EQ(total_avail_pages, expected_avail_pages);
    EQ(outstanding_claims, expected_outstanding_claims);
    EQ_CLAIMS(dom1, set);

    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node0)));
    EQ(total_avail_pages, --expected_avail_pages);
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(set[1].target, node0);
    set[1].pages--; /* Expect the allocation redeemed from node 0 */
    EQ_CLAIMS(dom1, set);

    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    EQ(total_avail_pages, --expected_avail_pages);
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(set[2].target, node1);
    set[2].pages--; /* Expect the allocation redeemed from node 1 */
    EQ_CLAIMS(dom1, set);

    ASSERT(alloc_domheap_pages(dom1, order1, MEMF_node(node1)));
    EQ(total_avail_pages, (expected_avail_pages -= 1 << order1));
    EQ(outstanding_claims, (expected_outstanding_claims -= 1 << order1));
    xen_domctl_memory_claim_t claim_set2[] = {
        set[0],
        set[1],
        /* The claim from node 1 is consumed */
    };
    claim_set2[0].pages--; /* The 2nd page is redeemed from host-wide claim */
    EQ_CLAIMS(dom1, claim_set2);

    /* An allocation on node 1 falls back to the host-wide claim */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(total_avail_pages, --expected_avail_pages);
    xen_domctl_memory_claim_t claim_set3[] = {
        set[1], /* The host claim is consumed */
    };
    EQ_CLAIMS(dom1, claim_set3);

    /* An allocation on node 1 falls back to node 0 */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(total_avail_pages, --expected_avail_pages);
    claim_set2[1].pages--; /* The node 0 claim consumed the allocation */
    EQ_CLAIMS(dom1, ((xen_domctl_memory_claim_t[]){}));
}

static void test_xc_domain_get_memory_claims(int start_mfn)
{
    xen_domctl_memory_claim_t get[3], set[3] = {
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };
    uint32_t expected_records = ARRAY_SIZE(set), nr_records = 0;

    /* Prepare the free lists with the needed pages for the defined claims */
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    /* Install the defined claim in dom1 */
    EQ(xc_domain_set_memory_claims(&test_xc_handle, dom1->domain_id,
                                   set, ARRAY_SIZE(set)), 0);
    EQ_CLAIMS(dom1, set);

    /* Test getting the count of claim entries */
    XE(xc_domain_get_memory_claims(&test_xc_handle, dom1->domain_id, NULL,
                                    &nr_records), -ENOBUFS);
    EQ(nr_records, expected_records);

    /* Test getting the claim entries */
    EQ(xc_domain_get_memory_claims(&test_xc_handle, dom1->domain_id, get,
                                   &nr_records), 0);
    EQ(nr_records, expected_records);
    EQ(memcmp(get, set, sizeof(set)), 0);
    EQ_CLAIMS(dom1, get);
}

static void test_set_claims_error_handling(int start_mfn)
{
    xen_domctl_memory_claim_t entries[2] = {
        { .target = 0, .pages = 2 },
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2 },
    };

    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    /* Rejected requests leave no claims and no claims array behind */
    entries[0].pages = 5;
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, entries, 2), -ENOMEM);
    entries[0].pages = 2;
    entries[1].target = 0;
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, entries, 2), -EINVAL);
    entries[1].target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST;
    entries[1].pad = 1;
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, entries, 2), -EINVAL);
    entries[1].pad = 0;
    entries[0].pages = UINT64_MAX;
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, entries, 2), -EINVAL);
    entries[0].pages = 2;
    entries[0].target = MAX_NUMNODES;
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, entries, 2), -ENOENT);
    ASSERT(!dom1->claims && !dom1->outstanding_pages);
    ASSERT(!node_claimed_pages[0] && !outstanding_claims);

    /* Claims can be set once, a valid request is rejected until released */
    entries[0].target = 0;
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, entries, 2), 0);
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, entries, 2), -EINVAL);
    ASSERT(dom1->claims && dom1->claims[0] == 2);
    ASSERT(dom1->node_claims == 2 && dom1->outstanding_pages == 4);
    ASSERT(node_claimed_pages[0] == 2 && outstanding_claims == 4);
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0), 0);
    ASSERT(!dom1->claims && !dom1->outstanding_pages);
    ASSERT(!node_claimed_pages[0] && !outstanding_claims);
}

/*
 * Test nine claim-request validation boundaries:
 * - Zero-page node and host entries.
 * - Duplicate host entries, an unknown target, and an offline in-range node.
 * - Combined claims exceeding max_pages.
 * - Claims exactly fitting the remaining allowance or exceeding it by one.
 * - max_pages reduced below already allocated pages.
 *
 * Each case checks the return code, both domains' accounting, stored claims,
 * and free-page counters. The page-limit cases use real allocations,
 * and rejected requests must preserve another domain's existing claim.
 */
static void test_claim_validation_boundaries(int start_mfn)
{
    for ( unsigned int scenario = 0; scenario < 9; ++scenario )
    {
        xen_domctl_memory_claim_t entries[2] = {
            { .target = node0, .pages = 2 },
            { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2 },
        };
        xen_domctl_memory_claim_t other = {
            .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 1,
        };
        unsigned int allocated = scenario >= 5 ? 2 : 0;
        bool success = scenario == 6;
        int expected_ret = scenario == 4 ? -ENOENT :
                           success ? 0 : -EINVAL;

        reset_page_alloc_state(0);
        test_page_list_add_node_buddy(node0, start_mfn, order3);
        test_page_list_add_node_buddy(node1, start_mfn, order3);

        if ( allocated )
            ENSURE(alloc_domheap_pages(dom1, order1,
                                       MEMF_node(node1) | MEMF_exact_node),
                   "Validation %u: initial allocation failed", scenario);

        EQ(domain_tot_pages(dom1), allocated);
        XE(xc_domain_set_memory_claims(xch, dom2->domain_id, &other, 1), 0);

        switch ( scenario )
        {
        case 0:
            entries[0].pages = 0; /* Invalid zero-page entry in 1st claim */
            break;
        case 1:
            entries[1].pages = 0; /* Invalid zero-page entry in 2nd claim */
            break;
        case 2:
            /* two claim entries targeting the host result in EINVAL */
            entries[0].target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST;
            break;
        case 3:
            /* Creates an invalid special target */
            entries[0].target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST | 1U;
            break;
        case 4:
            node_clear(node0, node_online_map); /* Fails with offline node */
            break;
        case 5:
            dom1->max_pages = 3; /* max_pages is below the requested claim */
            break;
        case 6:
            dom1->max_pages = 6; /* above the requested claim (success) */
            break;
        case 7:
            dom1->max_pages = 5; /* with allocation, claims exceed max_pages */
            break;
        case 8:
            dom1->max_pages = 1; /* allows one entry, but not both entries */
            entries[0].pages = entries[1].pages = 1;
            break;
        }

        XE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                        entries, scenario == 8 ? 1 : 2),
            expected_ret);

        node_set(node0, node_online_map);

        ENSURE(domain_tot_pages(dom1) == allocated &&
               dom1->outstanding_pages == (success ? 4 : 0) &&
               dom1->node_claims == (success ? 2 : 0) &&
               !!dom1->claims == success &&
               (!dom1->claims || dom1->claims[node0] == 2) &&
               dom2->outstanding_pages == 1 && !dom2->node_claims &&
               !dom2->claims && outstanding_claims == (success ? 5 : 1) &&
               total_avail_pages == 16 - allocated,
               "Validation %u: domain or global state differs", scenario);

        for ( unsigned int target = 0; target < 4; ++target )
            ENSURE(node_claimed_pages[target] ==
                   (success && target == node0 ? 2 : 0) &&
                   node_avail_pages[target] ==
                   (target == node0 ? 8 :
                    target == node1 ? 8 - allocated : 0) &&
                   (!dom1->claims || dom1->claims[target] ==
                    (target == node0 ? 2 : 0)),
                   "Validation %u: node %u state differs", scenario, target);

        XE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0), 0);
        EQ(outstanding_claims, 1);
        ASSERT(!dom1->claims && !node_claimed_pages[node0]);

        XE(xc_domain_set_memory_claims(xch, dom2->domain_id, NULL, 0), 0);
        ASSERT(!outstanding_claims && total_avail_pages == 16 - allocated);
    }

    printf("Claim validation: 9 native scenarios passed.\n");
}

/*
 * Test setting and getting memory claims under 30 different scenarios,
 * covering buffer capacities 0–5 across initial claims, partial redemption,
 * full redemption, and cancellation.
 *
 * The sweep checks -ENOBUFS and required counts, host-first/node-ascending
 * ordering, omission of consumed claims, untouched buffer space beyond
 * successful results, and unchanged claim accounting after reads.
 */
static inline void test_get_claims_scenario(int start_mfn,
                                            unsigned int state,
                                            unsigned int capacity,
                                            unsigned int scenario)
{
    xen_domctl_memory_claim_t set[3] = {
        { .target = node1, .pages = 2 },
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
    };
    xen_domctl_memory_claim_t expected[3], get[5], untouched[5];
    unsigned int count = 0;
    unsigned int total = state < 3 ? 6 - 2 * state : 0;
    unsigned int node_total = state < 2 ? 4 - 2 * state : 0;
    unsigned int free_pages = state == 4 ? 6 : 8 - 2 * state;
    unsigned int *installed;
    uint32_t nr = capacity;
    int ret;

    reset_page_alloc_state(0);
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                       set, ARRAY_SIZE(set)) == 0,
           "Get scenario %u: installing claims failed", scenario);

    for ( unsigned int step = 0; step < (state == 4 ? 1 : state); ++step )
        ENSURE(alloc_domheap_pages(dom1, order1,
                                   MEMF_node(step ? node0 : node1) |
                                   MEMF_exact_node),
               "Get scenario %u: redemption failed", scenario);

    if ( state == 4 )
        ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0) == 0,
               "Get scenario %u: cancellation failed", scenario);

    if ( state < 3 )
        expected[count++] = (xen_domctl_memory_claim_t){
            .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 2,
        };

    if ( state < 2 )
        expected[count++] = (xen_domctl_memory_claim_t){
            .target = node0, .pages = 2,
        };

    if ( !state )
        expected[count++] = (xen_domctl_memory_claim_t){
            .target = node1, .pages = 2,
        };

    memset(get, 0xa5, sizeof(get));
    memset(untouched, 0xa5, sizeof(untouched));
    installed = dom1->claims;
    XE(xc_domain_get_memory_claims(xch, dom1->domain_id, capacity ? get : NULL,
                                    &nr), capacity < count ? -ENOBUFS : 0);
    ASSERT(nr == count);
    if ( !errno )
        ENSURE(!memcmp(get, expected, count * sizeof(*get)) &&
               !memcmp(get + count, untouched + count,
                       (ARRAY_SIZE(get) - count) * sizeof(*get)),
               "Get scenario %u: records or buffer tail differ", scenario);

    ENSURE(dom1->claims == installed &&
           dom1->outstanding_pages == total &&
           dom1->node_claims == node_total &&
           outstanding_claims == total && total_avail_pages == free_pages &&
           node_claimed_pages[node0] == (state < 2 ? 2 : 0) &&
           node_claimed_pages[node1] == (!state ? 2 : 0) &&
           (!installed ||
            (installed[node0] == (state < 2 ? 2 : 0) &&
             installed[node1] == (!state ? 2 : 0))),
           "Get scenario %u: reading changed claims", scenario);

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0) == 0 &&
           !dom1->claims && !outstanding_claims &&
           !node_claimed_pages[node0] && !node_claimed_pages[node1],
           "Get scenario %u: final release failed", scenario);
}

static void test_get_claims_model(int start_mfn)
{
    unsigned int scenarios = 0;

    for ( unsigned int state = 0; state < 5; ++state )
        for ( unsigned int capacity = 0; capacity <= 5; ++capacity )
            test_get_claims_scenario(start_mfn, state, capacity, scenarios++);

    printf("Get claims model: %u native scenarios passed.\n", scenarios);
}

static void test_protection_and_lifetime(int start_mfn)
{
    xen_domctl_memory_claim_t entry = { .target = 0, .pages = 4 };
    unsigned int exact = MEMF_node(node0) | MEMF_exact_node;
    unsigned int *installed;

    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, &entry, 1), 0);
    installed = dom1->claims;
    EQ(alloc_domheap_pages(dom2, order0, exact), 0);
    EQ(alloc_domheap_pages(dom1, order0, exact | MEMF_no_refcount), 0);
    ASSERT(dom1->outstanding_pages == 4 && node_claimed_pages[0] == 4);
    EQ(!alloc_domheap_pages(dom1, order2,
                            MEMF_node(node1) | MEMF_exact_node), 0);
    ASSERT(!outstanding_claims && !dom1->node_claims);
    ASSERT(dom1->claims == installed && !installed[0]);
    ASSERT(!node_claimed_pages[0]);

    /* The array of redeemed claims is replaced when claims are set again */
    entry.pages = 2;
    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, &entry, 1), 0);
    ASSERT(dom1->claims && dom1->claims[0] == 2 && dom1->node_claims == 2);
    ASSERT(node_claimed_pages[0] == 2 && outstanding_claims == 2);
    EQ(domain_set_outstanding_pages(dom1, 0), 0);
    ASSERT(!dom1->claims && !outstanding_claims);
}

static void test_move_claim_at_capacity(int start_mfn)
{
    xen_domctl_memory_claim_t entry = { .target = 0, .pages = 4 };

    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, &entry, 1), 0);
    EQ(domain_set_outstanding_pages(dom2, 4), 0);
    ASSERT(outstanding_claims == total_avail_pages);

    /* The claim has to be released before it can be set on node 1 */
    entry.target = 1;
    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0), 0);
    ASSERT(!dom1->claims && !node_claimed_pages[0]);

    XE(xc_domain_set_memory_claims(xch, dom1->domain_id, &entry, 1), 0);
    ASSERT(dom1->claims && !dom1->claims[0] && dom1->claims[1] == 4);
    ASSERT(!node_claimed_pages[0] && node_claimed_pages[1] == 4);
    EQ(!alloc_domheap_pages(dom2, order2,
                            MEMF_node(node0) | MEMF_exact_node), 0);
    ASSERT(outstanding_claims == 4 && total_avail_pages == 4);
    EQ(domain_set_outstanding_pages(dom1, 0), 0);
    ASSERT(!dom1->claims && !outstanding_claims);
}

/*
 * Test competing per-node claims under 128 different scenarios.
 *
 * Give both domains claims on the same node, then alternate allocations and
 * cancellations. Check that each allocation redeems only its owner's claims,
 * global node totals equal the sum of both domains' remaining claims, and
 * cancellation makes capacity available again.
 *
 * The sweep covers all four nodes and every nonzero split of an 8-page node
 * between two domains. It verifies that alternating allocations redeem only
 * the allocator's claims, uncredited allocations cannot consume claimed
 * capacity, and cancelling either domain releases exactly its remaining
 * capacity. A request one page above that capacity must fail without
 * changing accounting; reclaiming the exact amount must succeed.
 */
static inline void test_competing_scenario(int start_mfn, unsigned int target,
                                           unsigned int first_claim,
                                           unsigned int rounds,
                                           unsigned int cancelled,
                                           unsigned int scenario)
{
    struct domain *domains[2] = { dom1, dom2 };
    unsigned int expected[2] = { first_claim, 8 - first_claim };
    unsigned int flags = MEMF_node(target) | MEMF_exact_node;
    unsigned int free_pages = 8;
    unsigned int released;
    xen_domctl_memory_claim_t entry = { .target = target };

    reset_page_alloc_state(0);
    test_page_list_add_node_buddy(target, start_mfn, order3);

    for ( unsigned int owner = 0; owner < 2; ++owner )
    {
        entry.pages = expected[owner];
        ENSURE(xc_domain_set_memory_claims(xch, domains[owner]->domain_id,
                                           &entry, 1) == 0,
               "Competition %u: installing claims failed", scenario);
    }

    ENSURE(!alloc_domheap_pages(NULL, order0, flags),
           "Competition %u: uncredited allocation succeeded", scenario);

    for ( unsigned int step = 0; step < 2 * rounds; ++step )
    {
        unsigned int owner = step % 2;
        struct page_info *page = alloc_domheap_pages(domains[owner], order0,
                                                     flags);

        ENSURE(page && page_to_nid(page) == target,
               "Competition %u step %u: allocation failed", scenario, step);
        --expected[owner];
        --free_pages;

        for ( unsigned int other = 0; other < 2; ++other )
            ENSURE(domains[other]->claims[target] == expected[other] &&
                   domains[other]->node_claims == expected[other] &&
                   domains[other]->outstanding_pages == expected[other],
                   "Competition %u step %u: domain %u differs",
                   scenario, step, other);

        ENSURE(node_claimed_pages[target] == expected[0] + expected[1] &&
               outstanding_claims == free_pages &&
               total_avail_pages == free_pages &&
               node_avail_pages[target] == free_pages,
               "Competition %u step %u: shared totals differ",
               scenario, step);
    }

    released = expected[cancelled];
    ENSURE(xc_domain_set_memory_claims(xch, domains[cancelled]->domain_id,
                                       NULL, 0) == 0 &&
           !domains[cancelled]->claims &&
           !domains[cancelled]->node_claims &&
           !domains[cancelled]->outstanding_pages &&
           node_claimed_pages[target] == expected[1 - cancelled] &&
           outstanding_claims == expected[1 - cancelled] &&
           total_avail_pages == free_pages,
           "Competition %u: cancellation differs", scenario);

    entry.pages = released + 1;
    XE(xc_domain_set_memory_claims(xch, domains[cancelled]->domain_id,
                                    &entry, 1), -ENOMEM);
    ENSURE(!domains[cancelled]->claims &&
           !domains[cancelled]->outstanding_pages &&
           !domains[cancelled]->node_claims &&
           node_claimed_pages[target] == expected[1 - cancelled] &&
           outstanding_claims == expected[1 - cancelled] &&
           total_avail_pages == free_pages,
           "Competition %u: excess claim changed state", scenario);

    entry.pages = released;
    ENSURE(xc_domain_set_memory_claims(xch, domains[cancelled]->domain_id,
                                       &entry, 1) == 0 &&
           node_claimed_pages[target] == free_pages &&
           outstanding_claims == free_pages,
           "Competition %u: released capacity could not be reclaimed",
           scenario);

    for ( unsigned int owner = 0; owner < 2; ++owner )
        ENSURE(domains[owner]->claims[target] == expected[owner] &&
               domains[owner]->node_claims == expected[owner] &&
               domains[owner]->outstanding_pages == expected[owner],
               "Competition %u: restored domain %u differs", scenario, owner);

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0) == 0 &&
           xc_domain_set_memory_claims(xch, dom2->domain_id, NULL, 0) == 0 &&
           !dom1->claims && !dom2->claims && !outstanding_claims &&
           !node_claimed_pages[target] && total_avail_pages == free_pages,
           "Competition %u: final release differs", scenario);
}

static void test_competing_claims(int start_mfn)
{
    unsigned int scenarios = 0;

    for ( unsigned int target = 0; target < 4; ++target )
        for ( unsigned int first_claim = 1; first_claim < 8; ++first_claim )
            for ( unsigned int rounds = 0;
                  rounds < min(first_claim, 8 - first_claim); ++rounds )
                for ( unsigned int cancelled = 0; cancelled < 2; ++cancelled )
                    test_competing_scenario(start_mfn, target, first_claim,
                                            rounds, cancelled, scenarios++);

    printf("Competing claims: %u native scenarios passed.\n", scenarios);
}

/*
 * Test claim protection at admission and redemption boundaries.
 *
 * The sweep varies:
 * - Node 0 free capacity: 1, 2, 4, or 8 pages.
 * - Node 0's claim: zero, full capacity, and values around a request boundary.
 * - Host-wide claim: boundary values, with eight free pages on node 1.
 * - Exact-node allocation size: 1, 2, 4, 8, or 16 pages.
 * - Allocation mode: claimant, another domain, anonymous,
 *                    or claimant with MEMF_no_refcount.
 *
 * Each case starts with a fresh heap.
 * - An arithmetic oracle predicts admission using both per-node protection
 *   and Xen's host-wide claim rule.
 * - The test checks allocation success or failure, free-page counters, global
 *   and domain claim totals, per-node claims, and the installed claims array.
 * - Rejected allocations must preserve that state; successful allocations
 *   without claim credit must leave claims untouched.
 */
static inline void test_protection_scenario(int start_mfn,
                                            unsigned int free_order,
                                            unsigned int claimed,
                                            unsigned int host,
                                            unsigned int mode,
                                            unsigned int order,
                                            unsigned int scenario)
{
    unsigned int free_pages = 1U << free_order;
    unsigned int pages = 1U << order;
    unsigned int total_claim = claimed + host;
    unsigned int unclaimed = free_pages + 8 - total_claim;
    bool credit = mode == 0;
    bool success = pages <= free_pages - claimed + (credit ? claimed : 0) &&
                   (pages <= unclaimed ||
                    (credit && pages <= total_claim));
    unsigned int redeemed = success && credit ? min(total_claim, pages) : 0;
    unsigned int node_redeemed = min(claimed, redeemed);
    unsigned int allocated = success ? pages : 0;
    unsigned int flags = MEMF_node(node0) | MEMF_exact_node;
    xen_domctl_memory_claim_t claims[2];
    unsigned int count = 0;
    struct domain *alloc_domain;
    unsigned int *installed;
    struct page_info *page;

    reset_page_alloc_state(0);
    test_page_list_add_node_buddy(node0, start_mfn, free_order);
    test_page_list_add_node_buddy(node1, start_mfn, order3);

    if ( claimed )
        claims[count++] = (xen_domctl_memory_claim_t){
            .target = node0, .pages = claimed,
        };

    if ( host )
        claims[count++] = (xen_domctl_memory_claim_t){
            .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = host,
        };

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                       claims, count) == 0,
           "Protection scenario %u: installing claims failed", scenario);

    installed = dom1->claims;
    alloc_domain = mode == 1 ? dom2 : mode == 2 ? NULL : dom1;

    if ( mode == 3 )
        flags |= MEMF_no_refcount;

    page = alloc_domheap_pages(alloc_domain, order, flags);
    ENSURE(!!page == success,
           "Protection scenario %u: admission differs (mode %u)",
           scenario, mode);

    ENSURE(total_avail_pages == free_pages + 8 - allocated &&
           node_avail_pages[node0] == free_pages - allocated &&
           node_avail_pages[node1] == 8 &&
           outstanding_claims == total_claim - redeemed &&
           node_claimed_pages[node0] == claimed - node_redeemed &&
           !node_claimed_pages[node1] &&
           dom1->outstanding_pages == total_claim - redeemed &&
           dom1->node_claims == claimed - node_redeemed &&
           dom1->claims == installed &&
           (!installed || installed[node0] == claimed - node_redeemed) &&
           !dom2->outstanding_pages,
           "Protection scenario %u: allocation changed claims incorrectly",
           scenario);

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                       NULL, 0) == 0 &&
           !outstanding_claims && !node_claimed_pages[node0] &&
           !dom1->claims &&
           total_avail_pages == free_pages + 8 - allocated,
           "Protection scenario %u: release differs", scenario);
}

static void test_protection_model(int start_mfn)
{
    unsigned int scenarios = 0;

    for ( unsigned int free_order = 0; free_order <= 3; ++free_order )
        for ( unsigned int claimed = 0;
              claimed <= (1U << free_order); ++claimed )
            for ( unsigned int host = 0; host <= 8; ++host )
                for ( unsigned int mode = 0; mode < 4; ++mode )
                    for ( unsigned int order = 0; order <= 4; ++order )
                        test_protection_scenario(start_mfn, free_order,
                                                 claimed, host, mode, order,
                                                 scenarios++);

    printf("Protection model: %u native scenarios passed.\n", scenarios);
}

static void check_mixed_claims(unsigned int target, unsigned int host,
                                unsigned int node_pages)
{
    xen_domctl_memory_claim_t claims[2];
    uint32_t count = ARRAY_SIZE(claims);

    EQ(dom1->outstanding_pages, host + node_pages);
    EQ(dom1->node_claims, node_pages);
    EQ(dom2->outstanding_pages, 1);
    EQ(dom2->node_claims, 0);
    EQ(outstanding_claims, host + node_pages + 1);
    EQ(total_avail_pages, 16 - domain_tot_pages(dom1));

    for ( unsigned int node = 0; node < 4; ++node )
    {
        EQ(node_claimed_pages[node], node == target ? node_pages : 0);
        EQ(dom1->claims ? dom1->claims[node] : 0,
           node == target ? node_pages : 0);
        EQ(node_avail_pages[node],
           node == target ? 8 - domain_tot_pages(dom1) :
           node == (target + 1) % 4 ? 8 : 0);
    }

    EQ(xc_domain_get_memory_claims(xch, dom1->domain_id, claims, &count), 0);
    EQ(count, !!host + !!node_pages);

    if ( host )
    {
        EQ(claims[0].target, XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST);
        EQ(claims[0].pages, host);
    }

    if ( node_pages )
    {
        EQ(claims[!!host].target, target);
        EQ(claims[!!host].pages, node_pages);
    }
}

static void test_mixed_claim_lifecycle(int start_mfn)
{
    for ( unsigned int target = 0; target < 4; ++target )
        for ( unsigned int legacy_first = 0; legacy_first < 2; ++legacy_first )
        {
            xen_domctl_memory_claim_t claim = { .target = target, .pages = 2 };
            xen_domctl_memory_claim_t other = {
                .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 1,
            };
            unsigned int flags = MEMF_node(target) | MEMF_exact_node;
            unsigned int *installed;
            struct page_info *page;

            reset_page_alloc_state(0);
            test_page_list_add_node_buddy(target, start_mfn, order3);
            test_page_list_add_node_buddy((target + 1) % 4, start_mfn, order3);
            EQ(xc_domain_set_memory_claims(xch, dom2->domain_id, &other, 1), 0);

            if ( legacy_first )
                EQ(xc_domain_claim_pages(xch, dom1->domain_id, 2), 0);
            else
                EQ(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                               &claim, 1), 0);

            installed = dom1->claims;
            check_mixed_claims(target, legacy_first ? 2 : 0,
                                legacy_first ? 0 : 2);
            XE(xc_domain_claim_pages(xch, dom1->domain_id, 3), -EINVAL);
            XE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                           &claim, 1), -EINVAL);
            EQ(dom1->claims, installed);
            check_mixed_claims(target, legacy_first ? 2 : 0,
                                legacy_first ? 0 : 2);

            page = alloc_domheap_pages(dom1, order0, flags);
            ENSURE(page, "Mixed claims: initial allocation failed");
            EQ(page_get_owner(page), dom1);
            check_mixed_claims(target, legacy_first ? 1 : 0,
                                legacy_first ? 0 : 1);
            free_domheap_pages(page, order0);
            EQ(domain_tot_pages(dom1), 0);
            check_mixed_claims(target, legacy_first ? 1 : 0,
                                legacy_first ? 0 : 1);

            ENSURE(alloc_domheap_pages(dom1, order0, flags),
                   "Mixed claims: final redemption failed");
            EQ(dom1->claims, installed);
            check_mixed_claims(target, 0, 0);

            if ( legacy_first )
                EQ(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                               &claim, 1), 0);
            else
                EQ(xc_domain_claim_pages(xch, dom1->domain_id,
                                         domain_tot_pages(dom1) + 2), 0);

            if ( !legacy_first )
                EQ(dom1->claims, installed);

            check_mixed_claims(target, legacy_first ? 0 : 2,
                                legacy_first ? 2 : 0);
            EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0), 0);
            EQ(dom1->claims, NULL);
            check_mixed_claims(target, 0, 0);

            EQ(xc_domain_set_memory_claims(xch, dom1->domain_id, &claim, 1), 0);
            check_mixed_claims(target, 0, 2);
            EQ(xc_domain_claim_pages(xch, dom1->domain_id, 0), 0);
            EQ(dom1->claims, NULL);
            check_mixed_claims(target, 0, 0);
            EQ(xc_domain_set_memory_claims(xch, dom2->domain_id, NULL, 0), 0);
            EQ(outstanding_claims, 0);
        }

    printf("Mixed claim lifecycle: 8 native scenarios passed.\n");
}

/*
 * Test shared-capacity claim admission below, at and above each limit.
 *
 * Two nodes each provide eight free pages. Existing claims belong to domain 2;
 * domain 1 requests new claims on the same target node and host-wide. Each
 * fixture isolates node capacity, global capacity, or both limits, with each
 * of the four nodes used as the target.
 *
 * An arithmetic oracle accepts requests only when combined claims fit both
 * the 16-page global capacity and the eight-page target-node capacity. The
 * sweep covers exact-boundary successes, failures of either limit alone,
 * and failures of both limits.
 *
 * Verify admission, stored claims, domain and global totals, and per-node
 * free-page counts. Rejected requests must leave both domains' state
 * unchanged.
 * Releasing domain 1's claims must preserve domain 2's claims; releasing both
 * must clear claim accounting without changing free-page counts.
 */
static void test_shared_capacity_model(int start_mfn)
{
    static const unsigned int boundaries[][4] = {
        { 0, 0, 0, 0 }, { 4, 0, 3, 0 }, { 4, 0, 4, 0 },
        { 4, 0, 5, 0 }, { 0, 8, 0, 7 }, { 0, 8, 0, 8 },
        { 1, 8, 0, 8 }, { 4, 4, 4, 4 }, { 4, 4, 4, 5 },
        { 4, 4, 5, 4 }, { 8, 8, 0, 0 }, { 0, 0, 8, 8 },
    };
    unsigned int scenarios = 0;

    for ( unsigned int scenario = 0;
          scenario < 4 * ARRAY_SIZE(boundaries); ++scenario )
    {
        unsigned int target = scenario % 4;
        unsigned int peer = (target + 1) % 4;
        const unsigned int *boundary = boundaries[scenario / 4];
        unsigned int existing_node = boundary[0];
        unsigned int existing_host = boundary[1];
        unsigned int requested_node = boundary[2];
        unsigned int requested_host = boundary[3];
        unsigned int existing_total = existing_node + existing_host;
        unsigned int requested_total = requested_node + requested_host;
        bool success = existing_total + requested_total <= 16 &&
                       existing_node + requested_node <= 8;
        xen_domctl_memory_claim_t existing[2], request[2];
        unsigned int existing_count = 0, request_count = 0;
        unsigned int *installed;
        int ret;

        scenarios++;
        reset_page_alloc_state(0);
        test_page_list_add_node_buddy(target, start_mfn, order3);
        test_page_list_add_node_buddy(peer, start_mfn, order3);

        if ( existing_node )
            existing[existing_count++] = (xen_domctl_memory_claim_t){
                .target = target, .pages = existing_node,
            };

        if ( existing_host )
            existing[existing_count++] = (xen_domctl_memory_claim_t){
                .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST,
                .pages = existing_host,
            };

        if ( requested_node )
            request[request_count++] = (xen_domctl_memory_claim_t){
                .target = target, .pages = requested_node,
            };

        if ( requested_host )
            request[request_count++] = (xen_domctl_memory_claim_t){
                .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST,
                .pages = requested_host,
            };

        XE(xc_domain_set_memory_claims(xch, dom2->domain_id,
                                        existing, existing_count), 0);
        installed = dom2->claims;
        XE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                        request, request_count),
            (success ? 0 : -ENOMEM));

        ENSURE(dom2->claims == installed &&
               dom2->outstanding_pages == existing_total &&
               dom2->node_claims == existing_node &&
               dom1->outstanding_pages == (success ? requested_total : 0) &&
               dom1->node_claims == (success ? requested_node : 0) &&
               !!dom1->claims == (success && requested_node) &&
               outstanding_claims == existing_total +
               (success ? requested_total : 0) &&
               total_avail_pages == 16,
               "Capacity %u: domain or global accounting differs", scenario);

        for ( unsigned int node = 0; node < 4; ++node )
        {
            unsigned int claimed = node == target ? existing_node +
                                   (success ? requested_node : 0) : 0;

            ENSURE(node_claimed_pages[node] == claimed &&
                   node_avail_pages[node] ==
                   (node == target || node == peer ? 8 : 0) &&
                   (installed ? installed[node] : 0) ==
                   (node == target ? existing_node : 0) &&
                   (dom1->claims ? dom1->claims[node] : 0) ==
                   (success && node == target ? requested_node : 0),
                   "Capacity %u: node %u accounting differs", scenario, node);
        }

        XE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0), 0);
        EQ(outstanding_claims, existing_total);
        EQ(node_claimed_pages[target], existing_node);
        XE(xc_domain_set_memory_claims(xch, dom2->domain_id, NULL, 0), 0);
        EQ(!dom1->claims && !dom2->claims && !outstanding_claims &&
           !node_claimed_pages[target] && total_avail_pages == 16, 1);
    }

    printf("Shared capacity model: %u native scenarios passed.\n", scenarios);
}

/*
 * Test redemption at the boundaries between node, host and other-node claims.
 *
 * The fixtures cover:
 * - Empty, host-only, single-node and mixed claims.
 * - Allocation from any of four nodes, including the unclaimed fourth node.
 * - Total allocation sizes from 0–20 pages.
 * - Each node gets a heap with a 16-page buddy and an 8-page buddy.
 */
static void test_redemption_model(int start_mfn)
{
    static const unsigned int claim_patterns[][4] = {
        { 0, 0, 0, 0 }, { 4, 0, 0, 0 }, { 0, 4, 0, 0 },
        { 0, 0, 4, 0 }, { 0, 0, 0, 4 }, { 2, 1, 3, 2 },
        { 0, 1, 3, 2 }, { 4, 4, 4, 4 },
    };
    unsigned int scenarios = 0;

    for ( unsigned int scenario = 0;
          scenario < ARRAY_SIZE(claim_patterns) * 4 * 21; ++scenario )
    {
        const unsigned int *pattern = claim_patterns[scenario / (4 * 21)];
        unsigned int host = pattern[0];
        unsigned int expected[4] = {
            pattern[1], pattern[2], pattern[3], 0,
        };
        unsigned int alloc_node = (scenario / 21) % 4;
        unsigned int alloc_flags = MEMF_node(alloc_node) | MEMF_exact_node;
        unsigned int pages = scenario % 21;
        unsigned int remaining = pages;
        unsigned int total = host + expected[0] + expected[1] + expected[2];
        unsigned int redeemed = min(total, pages);
        unsigned int alloc_node_redeemed = min(expected[alloc_node], redeemed);
        unsigned int host_used = min(host, redeemed - alloc_node_redeemed);
        unsigned int other_used = redeemed - alloc_node_redeemed - host_used;
        xen_domctl_memory_claim_t claims[4];
        xen_domctl_memory_claim_t other_claim = {
            .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 1,
        };
        unsigned int count = 0;
        unsigned int expected_node_total = 0;

        {
            unsigned int node_boundary = expected[alloc_node];
            unsigned int host_boundary = node_boundary + host;

                    if ( pages != 0 && pages != node_boundary &&
                     pages != node_boundary + 1 && pages != host_boundary &&
                     pages != host_boundary + 1 && pages != total &&
                     pages != total + 1 &&
                     (!node_boundary || pages != node_boundary - 1) &&
                     (!host_boundary || pages != host_boundary - 1) )
                    continue;
        }

        scenarios++;
        reset_page_alloc_state(0);

        for ( unsigned int target = 0; target < 4; ++target )
        {
            ENSURE(page_to_nid(test_get_node_page(target, start_mfn)) ==
                   target, "Scenario %u: incorrect node mapping", scenario);
            test_page_list_add_node_buddy(target, start_mfn, order4);
            test_page_list_add_node_buddy(target, start_mfn - 8, order3);
        }

        if ( host )
            claims[count++] = (xen_domctl_memory_claim_t){
                .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = host,
            };

        for ( unsigned int target = 0; target < 3; ++target )
            if ( expected[target] )
                claims[count++] = (xen_domctl_memory_claim_t){
                    .target = target, .pages = expected[target],
                };

        ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                           claims, count) == 0,
               "Scenario %u: installing domain 1 claims failed", scenario);
        ENSURE(xc_domain_set_memory_claims(xch, dom2->domain_id,
                                           &other_claim, 1) == 0,
               "Scenario %u: installing domain 2 claim failed", scenario);
        ENSURE(outstanding_claims == total + 1,
               "Scenario %u: initial outstanding total differs", scenario);

        while ( remaining )
        {
            unsigned int order = fls(remaining) - 1;

            ENSURE(alloc_domheap_pages(dom1, order, alloc_flags),
                   "Scenario %u: order %u allocation failed", scenario, order);
            remaining -= 1U << order;
        }

        expected[alloc_node] -= alloc_node_redeemed;

        for ( unsigned int target = 0; target < 4; ++target )
        {
            unsigned int used = target == alloc_node ? 0 :
                                min(expected[target], other_used);

            expected[target] -= used;
            other_used -= used;
            expected_node_total += expected[target];
            ENSURE(node_claimed_pages[target] == expected[target],
                   "Scenario %u: node %u claim differs", scenario, target);
        }

        ENSURE(!other_used, "Scenario %u: model remainder", scenario);
        EQ(dom1->outstanding_pages - dom1->node_claims, host - host_used);

        ENSURE(total_avail_pages == 96 - pages &&
               outstanding_claims == total - redeemed + 1 &&
               dom1->outstanding_pages == total - redeemed &&
               dom1->node_claims == expected_node_total &&
               dom2->outstanding_pages == 1,
               "Scenario %u: redemption differs (node %u, pages %u)",
               scenario, alloc_node, pages);

        ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                           NULL, 0) == 0 &&
               outstanding_claims == 1 &&
               !node_claimed_pages[0] && !node_claimed_pages[1] &&
               !node_claimed_pages[2] && !node_claimed_pages[3],
               "Scenario %u: releasing domain 1 claims failed", scenario);

        ENSURE(xc_domain_set_memory_claims(xch, dom2->domain_id,
                                           NULL, 0) == 0 &&
               !outstanding_claims && total_avail_pages == 96 - pages,
               "Scenario %u: releasing domain 2 claim failed", scenario);
    }

    printf("Redemption model: %u native scenarios passed.\n", scenarios);
}

/*
 * Exercise every three-node sequence with binary host/per-node claims and
 * order-0 allocations. Selected multi-page sequences additionally exercise
 * partial redemption and spillover between claim classes.
 *
 * After each allocation, it checks redemption priority, stored claims,
 * domain/global counters, and per-node free pages. It also verifies that
 * another domain's claim remains untouched and that final claim release
 * preserves free-page accounting.
 */
static inline void test_redemption_sequence(int start_mfn,
                                            unsigned int claim_case,
                                            unsigned int sequence,
                                            unsigned int claim_base,
                                            unsigned int orders,
                                            unsigned int scenario)
{
    unsigned int expected[4];
    unsigned int free_pages[4] = { 16, 16, 16, 16 };
    unsigned int host = claim_case % claim_base;
    unsigned int total_free = 64;
    xen_domctl_memory_claim_t claims[5];
    xen_domctl_memory_claim_t other_claim = {
        .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 1,
    };
    unsigned int count = 0;

    reset_page_alloc_state(0);
    claim_case /= claim_base;

    for ( unsigned int target = 0; target < 4; ++target )
    {
        expected[target] = claim_case % claim_base;
        claim_case /= claim_base;
        test_page_list_add_node_buddy(target, start_mfn, order4);
        if ( expected[target] )
            claims[count++] = (xen_domctl_memory_claim_t){
                .target = target, .pages = expected[target],
            };
    }

    if ( host )
        claims[count++] = (xen_domctl_memory_claim_t){
            .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = host,
        };

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                       claims, count) == 0 &&
           xc_domain_set_memory_claims(xch, dom2->domain_id,
                                       &other_claim, 1) == 0,
           "Sequence %u: installing claims failed", scenario);

    for ( unsigned int step = 0; step < 3; ++step )
    {
        unsigned int alloc_node = sequence % 4;
        unsigned int order = (sequence / 4) % orders;
        unsigned int pages = 1U << order;
        unsigned int remaining = pages;
        unsigned int used = min(expected[alloc_node], remaining);
        unsigned int node_total = 0;
        struct page_info *page;

        sequence /= 4 * orders;
        page = alloc_domheap_pages(dom1, order,
                                   MEMF_node(alloc_node) | MEMF_exact_node);

        ENSURE(page && page_to_nid(page) == alloc_node,
               "Sequence %u step %u: allocation differs", scenario, step);

        free_pages[alloc_node] -= pages;
        total_free -= pages;
        expected[alloc_node] -= used;
        remaining -= used;
        used = min(host, remaining);
        host -= used;
        remaining -= used;

        for ( unsigned int target = 0; target < 4; ++target )
        {
            used = target == alloc_node ? 0 :
                   min(expected[target], remaining);
            expected[target] -= used;
            remaining -= used;
            node_total += expected[target];
            EQ(dom1->claims ? dom1->claims[target] : 0, expected[target]);
            ENSURE(node_claimed_pages[target] == expected[target] &&
                   node_avail_pages[target] == free_pages[target],
                   "Sequence %u step %u: node %u differs",
                   scenario, step, target);
        }

        ENSURE(dom1->outstanding_pages == node_total + host &&
               dom1->node_claims == node_total &&
               outstanding_claims == node_total + host + 1 &&
               total_avail_pages == total_free &&
               dom2->outstanding_pages == 1 && !dom2->node_claims &&
               !dom2->claims,
               "Sequence %u step %u: accounting differs", scenario, step);
    }

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0) == 0 &&
           outstanding_claims == 1 && !dom1->claims &&
           !dom1->node_claims && !dom1->outstanding_pages &&
           !node_claimed_pages[0] && !node_claimed_pages[1] &&
           !node_claimed_pages[2] && !node_claimed_pages[3] &&
           xc_domain_set_memory_claims(xch, dom2->domain_id, NULL, 0) == 0 &&
           !outstanding_claims && total_avail_pages == total_free,
           "Sequence %u: releasing claims failed", scenario);
}

static void test_redemption_sequences(int start_mfn)
{
    static const unsigned int multi_claims[] = {
        2, 6, 8, 26, 80, 242,
    };
    static const unsigned int multi_sequences[] = {
        8 + 12 * 8 + 144 * 8,
        11 + 12 * 6 + 144 * 1,
        4 + 12 * 5 + 144 * 6,
        3 + 12 * 8 + 144 * 11,
    };
    unsigned int scenarios = 0;

    for ( unsigned int claim_case = 0; claim_case < 32; ++claim_case )
        for ( unsigned int sequence = 0; sequence < 64; ++sequence )
            test_redemption_sequence(start_mfn, claim_case, sequence, 2, 1,
                                     scenarios++);

    for ( unsigned int claims = 0;
          claims < ARRAY_SIZE(multi_claims); ++claims )
        for ( unsigned int sequence = 0;
              sequence < ARRAY_SIZE(multi_sequences); ++sequence )
            test_redemption_sequence(start_mfn, multi_claims[claims],
                                     multi_sequences[sequence], 3, 3,
                                     scenarios++);

    printf("Redemption sequences: %u native scenarios passed.\n", scenarios);
}

/*
 * Test allocations falling back to other NUMA nodes.
 * Exercises the selection path in get_free_page() for fallback allocations.
 *
 * Force the requested node to be unavailable or protected by another domain's
 * claim, then allocate without MEMF_exact_node. Verify the actual allocation
 * node and that redemption starts with that node's claim, not the requested
 * node's. Include fallback nodes outside the domain's preferred nodemask.
 *
 * Covers every distinct requested/fallback node pair, allocation orders 0–1,
 * empty or claim-protected requested nodes, and full or requested-node-only
 * affinity.
 *
 * Verifies exact-node rejection, successful fallback to the expected node,
 * and redemption from the actual allocation node's claim while preserving
 * host claims and another domain's claims. It also checks per-node accounting
 * and final claim release.
 */
static inline void test_fallback_scenario(int start_mfn,
                                          unsigned int requested,
                                          unsigned int fallback,
                                          unsigned int order,
                                          unsigned int mode,
                                          unsigned int scenario)
{
    unsigned int pages = 1U << order;
    bool protected = mode & 1;
    unsigned int requested_claim = protected ? pages - 1 : 0;
    unsigned int other_claim = protected ? 8 - requested_claim : 0;
    unsigned int free_pages = protected ? 16 : 8;
    xen_domctl_memory_claim_t claims[3] = {
        { .target = fallback, .pages = pages },
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 1 },
        { .target = requested, .pages = requested_claim },
    };
    xen_domctl_memory_claim_t other = {
        .target = requested, .pages = other_claim,
    };
    struct page_info *page;

    reset_page_alloc_state(0);
    test_page_list_add_node_buddy(fallback, start_mfn, order3);

    if ( protected )
        test_page_list_add_node_buddy(requested, start_mfn, order3);

    if ( mode & 2 )
    {
        nodes_clear(dom1->node_affinity);
        node_set(requested, dom1->node_affinity);
    }

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id, claims,
                                       requested_claim ? 3 : 2) == 0 &&
           (!protected ||
            xc_domain_set_memory_claims(xch, dom2->domain_id, &other, 1) == 0),
           "Fallback %u: installing claims failed", scenario);

    ENSURE(!alloc_domheap_pages(dom1, order,
                                MEMF_node(requested) | MEMF_exact_node) &&
           dom1->outstanding_pages == pages + requested_claim + 1 &&
           outstanding_claims == pages + requested_claim + other_claim + 1 &&
           total_avail_pages == free_pages,
           "Fallback %u: exact-node failure changed state", scenario);

    page = alloc_domheap_pages(dom1, order, MEMF_node(requested));
    ENSURE(page && page_to_nid(page) == fallback,
           "Fallback %u: incorrect allocation node", scenario);

    for ( unsigned int target = 0; target < 4; ++target )
    {
        unsigned int expected = target == requested ?
                                requested_claim + other_claim : 0;
        unsigned int available = target == fallback ? 8 - pages :
                                 target == requested && protected ? 8 : 0;

        EQ(dom1->claims[target], target == requested ? requested_claim : 0);
        EQ(node_claimed_pages[target], expected);
        EQ(node_avail_pages[target], available);
        ENSURE((!protected || dom2->claims[target] ==
                (target == requested ? other_claim : 0)),
               "Fallback %u: node %u accounting differs", scenario, target);
    }

    ENSURE(dom1->node_claims == requested_claim &&
           dom1->outstanding_pages == requested_claim + 1 &&
           dom2->node_claims == other_claim &&
           dom2->outstanding_pages == other_claim &&
           outstanding_claims == requested_claim + other_claim + 1 &&
           total_avail_pages == free_pages - pages,
           "Fallback %u: redemption used the wrong claims", scenario);

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0) == 0 &&
           xc_domain_set_memory_claims(xch, dom2->domain_id, NULL, 0) == 0 &&
           !outstanding_claims && !dom1->claims && !dom2->claims &&
           !node_claimed_pages[requested] &&
           total_avail_pages == free_pages - pages,
           "Fallback %u: releasing claims failed", scenario);
}

static void test_fallback_model(int start_mfn)
{
    unsigned int scenarios = 0;

    for ( unsigned int requested = 0; requested < 4; ++requested )
        for ( unsigned int fallback = 0; fallback < 4; ++fallback )
            if ( requested != fallback )
                for ( unsigned int order = 0; order <= 1; ++order )
                    for ( unsigned int mode = 0; mode < 4; ++mode )
                        test_fallback_scenario(start_mfn, requested, fallback,
                                               order, mode, scenarios++);

    printf("Fallback model: %u native scenarios passed.\n", scenarios);
}

/*
 * Test fragmentation despite sufficient capacity.
 * The requested node has two nonadjacent half-size buddies, while the fallback
 * node has one full-size buddy. Both nodes have enough free pages for the
 * order-1 request, but only the fallback node can satisfy it.
 * Exact-node failure must preserve claims and free-page accounting. Fallback
 * must redeem its own node claim, preserving the requested-node and host
 * claims. Exercise every distinct requested/fallback node pair.
 */
static inline void test_fragmentation_scenario(int start_mfn,
                                               unsigned int requested,
                                               unsigned int fallback,
                                               unsigned int order,
                                               unsigned int scenario)
{
    unsigned int pages = 1U << order;
    xen_domctl_memory_claim_t claims[3] = {
        { .target = requested, .pages = pages - 1 },
        { .target = fallback, .pages = pages },
        { .target = XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST, .pages = 1 },
    };
    unsigned int *installed;
    struct page_info *page;

    reset_page_alloc_state(0);
    test_page_list_add_node_buddy(requested, start_mfn, order - 1);
    test_page_list_add_node_buddy(requested, start_mfn + pages, order - 1);
    test_page_list_add_node_buddy(fallback, start_mfn, order);

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id,
                                       claims, ARRAY_SIZE(claims)) == 0,
           "Fragmentation %u: installing claims failed", scenario);

    installed = dom1->claims;
    ENSURE(!alloc_domheap_pages(dom1, order,
                                MEMF_node(requested) | MEMF_exact_node) &&
           dom1->claims == installed &&
           dom1->outstanding_pages == 2 * pages &&
           dom1->node_claims == 2 * pages - 1 &&
           outstanding_claims == 2 * pages && total_avail_pages == 2 * pages,
           "Fragmentation %u: exact-node failure changed totals", scenario);

    for ( unsigned int target = 0; target < 4; ++target )
    {
        unsigned int claimed = target == requested ? pages - 1 :
                               target == fallback ? pages : 0;

        ENSURE(installed[target] == claimed &&
               node_claimed_pages[target] == claimed &&
               node_avail_pages[target] ==
               (target == requested || target == fallback ? pages : 0),
               "Fragmentation %u: failed allocation changed node %u",
               scenario, target);
    }

    page = alloc_domheap_pages(dom1, order, MEMF_node(requested));
    ENSURE(page && page_to_nid(page) == fallback &&
           dom1->claims == installed && dom1->outstanding_pages == pages &&
           dom1->node_claims == pages - 1 && outstanding_claims == pages &&
           total_avail_pages == pages,
           "Fragmentation %u: fallback or redemption differs", scenario);

    for ( unsigned int target = 0; target < 4; ++target )
        ENSURE(installed[target] == (target == requested ? pages - 1 : 0) &&
               node_claimed_pages[target] == installed[target] &&
               node_avail_pages[target] == (target == requested ? pages : 0),
               "Fragmentation %u: fallback changed node %u incorrectly",
               scenario, target);

    ENSURE(xc_domain_set_memory_claims(xch, dom1->domain_id, NULL, 0) == 0 &&
           !dom1->claims && !dom1->node_claims && !dom1->outstanding_pages &&
           !outstanding_claims && !node_claimed_pages[requested] &&
           total_avail_pages == pages,
           "Fragmentation %u: releasing claims failed", scenario);
}

static void test_fragmentation_model(int start_mfn)
{
    unsigned int scenarios = 0;

    for ( unsigned int requested = 0; requested < 4; ++requested )
        for ( unsigned int fallback = 0; fallback < 4; ++fallback )
            if ( requested != fallback )
                test_fragmentation_scenario(start_mfn, requested, fallback,
                                            order1, scenarios++);

    printf("Fragmentation model: %u native scenarios passed.\n", scenarios);
}

int main(void)
{
    run_test(test_claim_xsm_denials, 4);
    run_test(test_guest_copy_faults, 4);
    run_test(test_domctl_copyback_faults, 4);
    run_test(test_redemption_basic, 4);
    run_test(test_xc_domain_set_memory_claims, 4);
    run_test(test_xc_domain_get_memory_claims, 4);
    run_test(test_protection_and_lifetime, 4);
    run_test(test_move_claim_at_capacity, 4);
    run_test(test_set_claims_error_handling, 4);
    run_test(test_claim_validation_boundaries, 16);
    run_test(test_get_claims_model, 4);
    run_test(test_competing_claims, 16);
    run_test(test_protection_model, 16);
    run_test(test_mixed_claim_lifecycle, 16);
    run_test(test_shared_capacity_model, 16);
    run_test(test_redemption_model, 16);
    run_test(test_redemption_sequences, 16);
    run_test(test_fallback_model, 16);
    run_test(test_fragmentation_model, 16);
    return test_complete();
}
