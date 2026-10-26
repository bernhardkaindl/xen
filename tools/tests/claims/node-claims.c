/* SPDX-License-Identifier: GPL-2.0-only */
/* Integration tests for NUMA-aware memory claims with CONFIG_NUMA enabled */

#define CONFIG_NUMA 1
#define TEST_USES_LIBXENCTRL_DOMAIN_API
#include <native/init.h>

static void test_xc_domain_set_memory_claims(int start_mfn)
{
    unsigned long expected_avail_pages = 8, expected_outstanding_claims = 6;
    xen_domctl_memory_claim_t set[3] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };

    /* Prepare the free lists with the needed pages for the defined claims */
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    EQ(xc_domain_set_memory_claims(&test_xc_handle, dom1->domain_id,
                                   ARRAY_SIZE(set), set), 0);
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
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };
    uint32_t expected_records = ARRAY_SIZE(set), nr_records = 0;

    /* Prepare the free lists with the needed pages for the defined claims */
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    /* Install the defined claim in dom1 */
    EQ(xc_domain_set_memory_claims(&test_xc_handle, dom1->domain_id,
                                   ARRAY_SIZE(set), set), 0);
    EQ_CLAIMS(dom1, set);

    /* Test getting the count of claim entries */
    EQ(xc_domain_get_memory_claims(&test_xc_handle, dom1->domain_id,
                                   &nr_records, NULL), -ERANGE);
    EQ(nr_records, expected_records);

    /* Test getting the claim entries */
    EQ(xc_domain_get_memory_claims(&test_xc_handle, dom1->domain_id,
                                   &nr_records, get), 0);
    EQ(nr_records, expected_records);
    EQ(memcmp(get, set, sizeof(set)), 0);
    EQ_CLAIMS(dom1, get);
}

static void test_set_errors(int start_mfn)
{
    xen_domctl_memory_claim_t entries[2] = {
        { .target = 0, .pages = 2 },
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
    };

    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    /* Rejected requests leave no claims and no claims array behind */
    entries[0].pages = 5;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 2,
                                       entries) == -ENOMEM);
    entries[0].pages = 2;
    entries[1].target = 0;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 2,
                                       entries) == -EINVAL);
    entries[1].target = XEN_DOMCTL_CLAIM_MEMORY_HOST;
    entries[1].pad = 1;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 2,
                                       entries) == -EINVAL);
    entries[1].pad = 0;
    entries[0].pages = UINT64_MAX;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 2,
                                       entries) == -EINVAL);
    entries[0].pages = 2;
    entries[0].target = MAX_NUMNODES;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 2,
                                       entries) == -ENOENT);
    ASSERT(!dom1->claims && !dom1->outstanding_pages);
    ASSERT(!node_claimed_pages[0] && !outstanding_claims);

    /* Claims can be set once, a valid request is rejected until released */
    entries[0].target = 0;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 2, entries) == 0);
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 2,
                                       entries) == -EINVAL);
    ASSERT(dom1->claims && dom1->claims[0] == 2);
    ASSERT(dom1->node_claims == 2 && dom1->outstanding_pages == 4);
    ASSERT(node_claimed_pages[0] == 2 && outstanding_claims == 4);
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 0, NULL) == 0);
    ASSERT(!dom1->claims && !dom1->outstanding_pages);
    ASSERT(!node_claimed_pages[0] && !outstanding_claims);
}

static void test_move_claim_at_capacity(int start_mfn)
{
    xen_domctl_memory_claim_t entry = { .target = 0, .pages = 4 };

    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 1, &entry) == 0);
    ASSERT(domain_set_outstanding_pages(dom2, 4) == 0);
    ASSERT(outstanding_claims == total_avail_pages);

    /* The claim has to be released before it can be set on node 1 */
    entry.target = 1;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 1,
                                       &entry) == -EINVAL);
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 0, NULL) == 0);
    ASSERT(!dom1->claims && !node_claimed_pages[0]);
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 1, &entry) == 0);
    ASSERT(dom1->claims && !dom1->claims[0] && dom1->claims[1] == 4);
    ASSERT(!node_claimed_pages[0] && node_claimed_pages[1] == 4);
    ASSERT(alloc_domheap_pages(dom2, order2,
                               MEMF_node(node0) | MEMF_exact_node));
    ASSERT(outstanding_claims == 4 && total_avail_pages == 4);
    ASSERT(domain_set_outstanding_pages(dom1, 0) == 0);
    ASSERT(!dom1->claims && !outstanding_claims);
}

static void test_protection_and_lifetime(int start_mfn)
{
    xen_domctl_memory_claim_t entry = { .target = 0, .pages = 4 };
    unsigned int exact = MEMF_node(node0) | MEMF_exact_node;
    unsigned int *installed;

    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 1, &entry) == 0);
    installed = dom1->claims;
    ASSERT(!alloc_domheap_pages(dom2, order0, exact));
    ASSERT(!alloc_domheap_pages(dom1, order0, exact | MEMF_no_refcount));
    ASSERT(dom1->outstanding_pages == 4 && node_claimed_pages[0] == 4);
    ASSERT(alloc_domheap_pages(dom1, order2,
                               MEMF_node(node1) | MEMF_exact_node));
    ASSERT(!outstanding_claims && !dom1->node_claims);
    ASSERT(dom1->claims == installed && !installed[0]);
    ASSERT(!node_claimed_pages[0]);

    /* The array of redeemed claims is replaced when claims are set again */
    entry.pages = 2;
    ASSERT(xc_domain_set_memory_claims(xch, dom1->domain_id, 1, &entry) == 0);
    ASSERT(dom1->claims && dom1->claims[0] == 2 && dom1->node_claims == 2);
    ASSERT(node_claimed_pages[0] == 2 && outstanding_claims == 2);
    ASSERT(domain_set_outstanding_pages(dom1, 0) == 0);
    ASSERT(!dom1->claims && !outstanding_claims);
}

int main(void)
{
    run_test(test_xc_domain_set_memory_claims, 4);
    run_test(test_xc_domain_get_memory_claims, 4);
    run_test(test_protection_and_lifetime, 4);
    run_test(test_move_claim_at_capacity, 4);
    run_test(test_set_errors, 4);
    return test_complete();
}
