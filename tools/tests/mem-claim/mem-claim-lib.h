/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MEM_CLAIM_LIB_H
#define MEM_CLAIM_LIB_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <xenctrl.h>

#define MAX_SELECTED_TESTS 64
#define INVALID_NODE UINT_MAX

struct test_env
{
    xc_interface *xch;
    struct xen_domctl_createdomain create_template;
    unsigned int num_nodes;
    unsigned int primary_node;
    unsigned int secondary_node;
    bool have_secondary_node;
    xc_meminfo_t *meminfo;
};

struct runtime_config
{
    const char *selected_ids[MAX_SELECTED_TESTS];
    size_t nr_selected_ids;
    bool list_only;
    bool verbose;
};

enum test_status
{
    TEST_PASSED,
    TEST_FAILED,
    TEST_SKIPPED,
};

struct test_case;

struct test_result
{
    const struct test_case *test;
    enum test_status status;
    char params[256];
    char details[4096];
    double duration_ms;
};

struct test_ctx
{
    struct test_env *env;
    const struct runtime_config *cfg;
    struct test_result *result;
    uint32_t domid;
    uint32_t helper_domid;
    unsigned int node;
    unsigned int other_node;
    unsigned long alloc_pages;
    char step[160];
};

struct lib_populate_exact_args
{
    uint32_t domid;
    xen_pfn_t gpfn;
    unsigned long nr_extents;
    unsigned int order;
    unsigned int node;
    const char *reason;
};

/*
 * test_fn_t: the test body.  Called after the fixture has created
 * ctx->domid and captured a baseline outstanding-pages count.  Tests needing
 * extra domains should create and destroy them explicitly.
 * Returns 0 on pass, -1 on fail, 1 on skip.
 */
typedef int (*test_fn_t)(struct test_ctx *ctx);

struct test_case
{
    const char *id;
    const char *name;
    test_fn_t run;
};

/* --- diagnostics helpers --- */
void lib_appendf(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void lib_debugf(struct test_ctx *ctx, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void lib_set_step(struct test_ctx *ctx, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int lib_fail_with_errno(struct test_ctx *ctx, int errnum, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
int lib_fail(struct test_ctx *ctx, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int lib_skip_test(struct test_ctx *ctx, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* --- memory-state queries --- */
int lib_get_node_free_pages(struct test_env *env, unsigned int node,
                            unsigned long *free_pages,
                            unsigned long *total_pages);
int lib_get_global_free_pages(struct test_env *env, unsigned long *free_pages);
int lib_get_baseline_outstanding(struct test_ctx *ctx,
                                 uint64_t *baseline_outstanding);
int lib_check_claim(struct test_ctx *ctx, uint64_t baseline_outstanding,
                    uint64_t expected_delta, const char *reason);

/* --- domain lifecycle --- */
int lib_create_domain(struct test_ctx *ctx, uint32_t *domid, const char *label);
int lib_destroy_domain(struct test_ctx *ctx, uint32_t *domid,
                       const char *label);

/* --- claim operations --- */
int lib_claim_memory(struct test_ctx *ctx, uint32_t domid, uint32_t nr_claims,
                     memory_claim_t *claims, const char *reason);
int lib_expect_claim_memory_failure(struct test_ctx *ctx, uint32_t domid,
                                    uint32_t nr_claims, memory_claim_t *claims,
                                    int expected_errno, const char *reason);
int lib_release_all_claims(struct test_ctx *ctx, uint32_t domid);
int lib_claim_pages_legacy(struct test_ctx *ctx, uint32_t domid,
                           unsigned long nr_pages, const char *reason);
int lib_claim_pages_legacy_failure(struct test_ctx *ctx, uint32_t domid,
                                   unsigned long nr_pages, int expected_errno,
                                   const char *reason);

/* --- physmap population --- */
int lib_populate_any(struct test_ctx *ctx, uint32_t domid, xen_pfn_t gpfn,
                     const char *reason);
int lib_populate_exact_node(struct test_ctx *ctx,
                            struct lib_populate_exact_args args);
int lib_expect_populate_exact_failure(struct test_ctx *ctx,
                                      struct lib_populate_exact_args args);
int lib_get_domain_mfn(struct test_ctx *ctx, uint32_t domid, xen_pfn_t gpfn,
                       xen_pfn_t *mfn, const char *reason);
int lib_offline_memory(struct test_ctx *ctx, uint32_t domid,
                       unsigned long nr_pages, const char *reason);
int lib_offline_memory_from_mfn(struct test_ctx *ctx, uint32_t domid,
                                xen_pfn_t start_mfn, unsigned long nr_pages,
                                const char *reason);

/* --- test runner --- */
int lib_run_one_test(struct test_env *env, const struct runtime_config *cfg,
                     const struct test_case *test, struct test_result *result);
int lib_initialise_test_env(struct test_env *env);
void lib_release_test_env(struct test_env *env);
unsigned long lib_default_alloc_pages(unsigned long free_pages);

static inline const char *status_name(enum test_status status)
{
    switch ( status )
    {
    case TEST_PASSED:
        return "PASSED";
    case TEST_FAILED:
        return "FAILED";
    case TEST_SKIPPED:
        return "SKIPPED";
    }
    return "UNKNOWN";
}

static inline bool test_is_selected(const struct runtime_config *cfg,
                                    const struct test_case *test)
{
    if ( !cfg->nr_selected_ids )
        return true;

    for ( size_t i = 0; i < cfg->nr_selected_ids; i++ )
        if ( !strcmp(cfg->selected_ids[i], test->id) )
            return true;
    return false;
}

#endif /* MEM_CLAIM_LIB_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
