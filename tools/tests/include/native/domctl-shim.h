/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal shim to include xen/common/domctl.c in native program tests.
 *
 * This shim provides the minimal Xen definitions that domctl.c
 * needs to run in a native program test environment.  It replaces a
 * minimal subset of the Xen environment that xen/common/domctl.c
 * interacts with with stubs so it can run in the test environment,
 * allowing test scenarios to verify the behavior of domctl.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_DOMCTL_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_DOMCTL_SHIM_H

#ifdef TEST_WRAP_XEN_COMMON_DOMCTL_C
#ifdef __XEN_FRAME_NUM_H__ /* x86_64 header differnce */
#define mfn_to_gfn(d, mfn) ((void)(d), _gfn(mfn_x(mfn)))
#endif

/* vcpu iteration: no vCPUs in the test context */
/* for_each_vcpu is provided by xen/sched.h; override with test stub */
#undef for_each_vcpu
#define for_each_vcpu(d, v) for ( (v) = NULL; (v) != NULL; )

/* Domain predicates not derived from any included header */
#define cpupool_get_id(d) 0

/* spin_trylock: in the test context all locks are always available */
#define spin_trylock(l) (spin_lock(l), true)

/* XSM hooks: permit all operations in the test context */
#define xsm_security_domaininfo(d, info)  ((void)(d), (void)(info))
#define xsm_domctl(xsm, d, ...)           0
#define xsm_getdomaininfo(xsm, d)         0
#define xsm_iomem_permission(xsm, d, ...) 0
#define xsm_iomem_mapping(xsm, d, ...)    0
#define xsm_set_target(xsm, d, e)         0
#define xsm_claim_pages(xsm, d)           0
#define xsm_get_domain_state(xsm, d)      0

/* XEN_DOMCTL_soft_reset_cont is inside #ifdef __XEN__ in public/domctl.h */
#ifndef XEN_DOMCTL_soft_reset_cont
#define XEN_DOMCTL_soft_reset_cont 23
#endif

/* physical address bit-width of the hypervisor */
#define paddr_bits PADDR_BITS

/* vcpu_guest_context allocation helpers */
#define alloc_vcpu_guest_context()  calloc(1, sizeof(struct vcpu_guest_context))
#define free_vcpu_guest_context(p)  free(p)

/* Domain lifecycle stubs */
#define domain_pause(d)                       ((void)(d))
#define domain_unpause(d)                     ((void)(d))
#define domain_resume(d)                      ((void)(d))
#define domain_kill(d)                        0
#define domain_create(id, cfg, hvm)           ((struct domain *)NULL)
#define domain_update_node_affinity(d)        ((void)(d))
#define domain_set_node_affinity(d, mask)     0
#define domain_pause_by_systemcontroller(d)   0
#define domain_unpause_by_systemcontroller(d) 0
#define domain_set_time_offset(d, off)        ((void)(d))
#define get_domain_by_id(id)                  ((struct domain *)NULL)
#define hypercall_create_continuation(...)    0

/* vCPU stubs */
#define vcpu_pause(v)   ((void)(v))
#define vcpu_unpause(v) ((void)(v))

/* Scheduling / affinity stubs */
#define sched_adjust(d, op)               0
#define vcpu_affinity_domctl(d, cmd, aff) 0

/* IOMEM and MMIO stubs */
#define iomem_access_permitted(d, ...) false
#define iomem_permit_access(d, ...)    0
#define iomem_deny_access(d, ...)      0
#define paging_mode_translate(d)       false
#define is_hardware_domain(d)          false

/* Misc domctl sub-operation stubs */
#define vm_event_domctl(d, op)           (-EOPNOTSUPP)
#define set_global_virq_handler(d, virq) 0
#define get_domain_state(st, d, id)      (-EOPNOTSUPP)
#define arch_do_domctl(op, d, u)         (-EOPNOTSUPP)

/* Provide no-op stubs for these */
domid_t domid_alloc(domid_t domid) { return domid; }

/* For do_domctl() to work with multiple domains for testing claims */
struct domain *rcu_lock_domain_by_id(domid_t domain_id)
{
    struct domain *d;

    for_each_domain ( d )
    {
        if ( d->domain_id == domain_id )
            return d;
    }
    return NULL;
}

struct vcpu *vcpu_create(struct domain *d, unsigned int vcpu_id)
{
    return NULL;
}

int vcpu_reset(struct vcpu *v) { return 0; }
uint64_t vcpu_runstate_get_running(const struct vcpu *v) { return 0; }
int arch_set_info_guest(struct vcpu *v, vcpu_guest_context_u c) { return 0; }
void arch_get_info_guest(struct vcpu *v, vcpu_guest_context_u c) { }
void arch_get_domain_info(const struct domain *d,
                          struct xen_domctl_getdomaininfo *info) { }
#endif
#endif
