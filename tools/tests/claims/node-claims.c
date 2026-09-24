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
    xen_domctl_memory_claim_t claim_set2[2] = {
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
    claim_set2[0].pages--; /* The host-wide claim consumed the allocation */
    EQ_CLAIMS(dom1, claim_set2);

    /* An allocation on node 1 falls back to node 0 */
    ASSERT(alloc_domheap_pages(dom1, order0, MEMF_node(node1)));
    EQ(outstanding_claims, --expected_outstanding_claims);
    EQ(total_avail_pages, --expected_avail_pages);
    claim_set2[1].pages--; /* The node 0 claim consumed the allocation */
    EQ_CLAIMS(dom1, ((xen_domctl_memory_claim_t[]){
                         { .target = XEN_DOMCTL_CLAIM_MEMORY_HOST, .pages = 0 }
                     }));
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

int main(void)
{
    run_test(test_xc_domain_set_memory_claims, 4);
    run_test(test_xc_domain_get_memory_claims, 4);
    return test_complete();
}
