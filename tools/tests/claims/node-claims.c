/* SPDX-License-Identifier: GPL-2.0-only */
/* Integration tests for NUMA-aware memory claims with CONFIG_NUMA enabled */

#define CONFIG_NUMA 1
#define TEST_USES_LIBXENCTRL_DOMAIN_API
#include <native/init.h>

static void test_domain_set_claim_entries(int start_mfn)
{
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    xen_domctl_memory_claim_t set[3] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };
    claim_set_t rq = { .nr_entries = ARRAY_SIZE(set), .claim = set };
    unsigned long expected_avail_pages = 8, expected_outstanding_claims = 6;

    EQ(domain_set_claim_entries(dom1, &rq), 0);
    EQ(total_avail_pages, expected_avail_pages);
    EQ(outstanding_claims, expected_outstanding_claims);
    CLAIMS(dom1, set);

    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node0)));
    EQ(total_avail_pages, --expected_avail_pages);
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(set[1].target, node0);
    set[1].pages--; /* Expect the allocation redeemed from node 0 */
    CLAIMS(dom1, set);

    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    EQ(total_avail_pages, --expected_avail_pages);
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(set[2].target, node1);
    set[2].pages--; /* Expect the allocation redeemed from node 1 */
    CLAIMS(dom1, set);

    ASSERT(alloc_domheap_pages(dom1, order1, MEMF_node(node1)));
    EQ(total_avail_pages, (expected_avail_pages -= 1 << order1));
    EQ(outstanding_claims, (expected_outstanding_claims -= 1 << order1));
    xen_domctl_memory_claim_t claim_set2[2] = {
        set[0],
        set[1],
        /* The claim from node 1 is consumed */
    };
    claim_set2[0].pages--; /* The 2nd page is redeemed from host-wide claim */
    CLAIMS(dom1, claim_set2);

    /* An allocation on node 1 falls back to the host-wide claim */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(total_avail_pages, --expected_avail_pages);
    claim_set2[0].pages--; /* The host-wide claim consumed the allocation */
    CLAIMS(dom1, claim_set2);

    /* An allocation on node 1 falls back to node 0 */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(total_avail_pages, --expected_avail_pages);
    claim_set2[1].pages--; /* The node 0 claim consumed the allocation */
    CLAIMS(dom1, ((xen_domctl_memory_claim_t[]){
                      { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 0 }
                  }));
}

static void test_domain_get_claim_entries(int start_mfn)
{
    test_page_list_add_node_buddy(node0, start_mfn, order2);
    test_page_list_add_node_buddy(node1, start_mfn, order2);

    xen_domctl_memory_claim_t set[3] = {
        { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
        { .target = node0, .pages = 2 },
        { .target = node1, .pages = 2 },
    };
    claim_set_t rq = { .nr_entries = ARRAY_SIZE(set), .claim = set };
    claim_set_t get = { 0 };
    uint32_t nr_records = 0, expected_records = ARRAY_SIZE(set);
    int install = domain_set_claim_entries(dom1, &rq);

    /* Test the domctl helper domain_get_claim_entries() */
    EQ(install, 0);
    EQ(domain_get_claim_entries(dom1, &get), -ERANGE);
    EQ(get.nr_entries, expected_records);

    /* Test the libxc interface xc_domain_get_memory_claims() */
    EQ(xc_domain_get_memory_claims(&test_xc_handle, dom1->domain_id,
                                   &nr_records, NULL), -ERANGE);
    EQ(nr_records, expected_records);
    CLAIMS(dom1, ((xen_domctl_memory_claim_t[]){
                      { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 2 },
                      { .target = node0, .pages = 2 },
                      { .target = node1, .pages = 2 }
                  }));
}

int main(void)
{
    run_test(test_domain_set_claim_entries, 4);
    run_test(test_domain_get_claim_entries, 4);
    return test_complete();
}
