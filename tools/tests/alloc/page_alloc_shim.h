/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Shim definitions for Xen types and functions used by page_alloc.c.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef _TEST_ALLOC_PAGE_ALLOC_SHIM_
#define _TEST_ALLOC_PAGE_ALLOC_SHIM_

#include "harness.h"

/*
 * Let page_alloc.c resolve Xen's real header paths, but suppress
 * the bodies of headers we replace locally in this shim on in the
 * complier's limits.h and stddef.h to avoid pulling in unwanted
 * definitions that conflict with the test harness and prevent clean
 * compilation of the included page_alloc.c.
 */
#define __TYPES_H__
#define __XEN_EVENT_H__
#define __VM_EVENT_H__
#define XEN_SOFTIRQ_H
#define __LINUX_NODEMASK_H
#define __XEN_DOMAIN_PAGE_H__
#define XEN__XVMALLOC_H
#define __XEN_MM_H__
#define _LINUX_INIT_H
#define __SCHED_H__
#define __XEN_KEYHANDLER_H__
#define __XEN_SECTIONS_H__
#define _XEN_PARAM_H
#define _XEN_NUMA_H
#define __XEN_SCRUB_H__
#define __XEN_LLC_COLORING_H__
#define __XEN_IRQ_H__
#define __SPINLOCK_H__
#define __LIB_H__
#define __XEN_PFN_H__
#define __XEN_PERFC_H__
#define __XEN_PUBLIC_SCHED_H__
#define __XEN_PUBLIC_SYSCTL_H__
#define __X86_PAGE_H__
#define __FLUSHTLB_H__

typedef uint64_t paddr_t;
typedef uint8_t nodeid_t;
typedef unsigned long mfn_t;
typedef unsigned long cpumask_t;
typedef unsigned long nodemask_t;
typedef uint8_t u8;
typedef long long s_time_t;

struct domain;
struct page_info;

struct page_list_head {
    struct page_info *head;
    struct page_info *tail;
    unsigned int count;
};

struct page_info {
    unsigned long count_info;
    union {
        /* Page is in use: ((count_info & PGC_count_mask) != 0). */
        struct {
            unsigned long type_info;
        } inuse;
        /* Page is on a free list: ((count_info & PGC_count_mask) == 0). */
        union {
            struct {
                unsigned int first_dirty;
                uint8_t scrub_state;
                bool need_tlbflush;
            };
            unsigned long val;
        } free;
    } u;
    union {
        struct {
            unsigned int order;
        } free;
        unsigned long type_info;
    } v;
    uint32_t tlbflush_timestamp;
    mfn_t mfn;
    nodeid_t nid;
    struct domain *owner;
    struct page_info *list_next;
    struct page_info *list_prev;
};

struct domain {
    spinlock_t page_alloc_lock;
    nodemask_t node_affinity;
    nodeid_t last_alloc_node;
    domid_t domain_id;
    unsigned int tot_pages;
    unsigned int max_pages;
    unsigned int extra_pages;
    unsigned int outstanding_pages;
    unsigned int xenheap_pages;
    bool is_dying;
    struct domain *next_in_list;
};

struct vcpu {
    struct domain *domain;
};

#define DEFINE_SPINLOCK(l)        spinlock_t l
#define spin_lock(l)              (void)(l)
#define spin_unlock(l)            (void)(l)
#define spin_lock_cb(l, cb, data) spin_lock(l)
#define spin_lock_kick()          ((void)0)
#define spin_is_locked(l)         true
#define rspin_is_locked(l)        true
#define nrspin_lock(l)            spin_lock(l)
#define nrspin_unlock(l)          spin_unlock(l)
#define rspin_lock(l)             spin_lock(l)
#define rspin_unlock(l)           spin_unlock(l)

#define DOMID_XEN 0
#define DOMID_IO  1

#define __init
#define __initdata
#define __ro_after_init
#define __read_mostly
#define cf_check

#define XENLOG_INFO
#define XENLOG_DEBUG
#define XENLOG_WARNING
#define XENLOG_ERR
#define KERN_INFO

#define PAGE_SHIFT   12
#define PAGE_SIZE    (1UL << PAGE_SHIFT)
#define MAX_ORDER    18
#define PADDR_BITS   52
#define MAX_NUMNODES 1
#define NUMA_NO_NODE 0U

#define PFN_DOWN(x) ((x) >> PAGE_SHIFT)
#define PFN_UP(x)   (((x) + PAGE_SIZE - 1) >> PAGE_SHIFT)

#define pfn_to_paddr(pfn) ((paddr_t)(pfn) << PAGE_SHIFT)
#define paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))

#define CONFIG_DEBUG

static inline void test_page_list_init(struct page_list_head *list)
{
    list->head  = NULL;
    list->tail  = NULL;
    list->count = 0;
}

static inline void test_page_list_add_common(struct page_info *pg,
                                             struct page_list_head *list,
                                             bool at_tail)
{
    pg->list_next = NULL;
    pg->list_prev = NULL;

    if ( list->head == NULL )
    {
        list->head = pg;
        list->tail = pg;
    }
    else if ( at_tail )
    {
        pg->list_prev         = list->tail;
        list->tail->list_next = pg;
        list->tail            = pg;
    }
    else
    {
        pg->list_next         = list->head;
        list->head->list_prev = pg;
        list->head            = pg;
    }

    list->count++;
}

#define test_page_list_count(list)   ((list)->count)
#define test_page_list_empty(list)   ((list)->head == NULL)
#define test_page_list_add(pg, list) test_page_list_add_common(pg, list, false)
#define test_page_list_add_tail(pg, list) \
    test_page_list_add_common(pg, list, true)

static inline void test_page_list_del(struct page_info *pg,
                                      struct page_list_head *list)
{
    if ( pg->list_prev )
        pg->list_prev->list_next = pg->list_next;
    else
        list->head = pg->list_next;

    if ( pg->list_next )
        pg->list_next->list_prev = pg->list_prev;
    else
        list->tail = pg->list_prev;

    pg->list_next = NULL;
    pg->list_prev = NULL;

    ASSERT(list->count > 0);
    list->count--;
}

static inline struct page_info *
test_page_list_remove_head(struct page_list_head *list)
{
    struct page_info *pg = list->head;

    if ( pg )
        test_page_list_del(pg, list);

    return pg;
}

#define PAGE_LIST_HEAD(name)   struct page_list_head name = {NULL, NULL, 0}
#define INIT_PAGE_LIST_HEAD(l) test_page_list_init(l)

/* Architecture-specific page state defines */
#define PG_shift(idx)         (BITS_PER_LONG - (idx))
#define PG_mask(x, idx)       (x##UL << PG_shift(idx))
#define PGT_count_width       PG_shift(2)
#define PGT_count_mask        ((1UL << PGT_count_width) - 1)
#define PGC_allocated         PG_mask(1, 1)
#define PGC_xen_heap          PG_mask(1, 2)
#define _PGC_need_scrub       PG_shift(4)
#define PGC_need_scrub        PG_mask(1, 4)
#define _PGC_broken           PG_shift(7)
#define PGC_broken            PG_mask(1, 7)
#define PGC_state             PG_mask(3, 9)
#define PGC_state_inuse       PG_mask(0, 9)
#define PGC_state_offlining   PG_mask(1, 9)
#define PGC_state_offlined    PG_mask(2, 9)
#define PGC_state_free        PG_mask(3, 9)
#define page_state_is(pg, st) (((pg)->count_info & PGC_state) == PGC_state_##st)
#define PGC_count_width       PG_shift(9)
#define PGC_count_mask        ((1UL << PGC_count_width) - 1)
#define _PGC_extra            PG_shift(10)
#define PGC_extra             PG_mask(1, 10)

#define BUDDY_NOT_SCRUBBING     0
#define BUDDY_SCRUBBING         1
#define BUDDY_SCRUB_ABORT       2
#define INVALID_DIRTY_IDX       (~0U)
#define INVALID_MFN_INITIALIZER 0UL
#define INVALID_M2P_ENTRY       (~0UL)
#define PRI_mfn                 "lu"
#define PRI_stime               "lld"

/* memflags: */
#define _MEMF_no_refcount        0
#define MEMF_no_refcount         (1U << _MEMF_no_refcount)
#define _MEMF_populate_on_demand 1
#define MEMF_populate_on_demand  (1U << _MEMF_populate_on_demand)
#define _MEMF_keep_scrub         2
#define MEMF_keep_scrub          (1U << _MEMF_keep_scrub)
#define _MEMF_no_dma             3
#define MEMF_no_dma              (1U << _MEMF_no_dma)
#define _MEMF_exact_node         4
#define MEMF_exact_node          (1U << _MEMF_exact_node)
#define _MEMF_no_owner           5
#define MEMF_no_owner            (1U << _MEMF_no_owner)
#define _MEMF_no_tlbflush        6
#define MEMF_no_tlbflush         (1U << _MEMF_no_tlbflush)
#define _MEMF_no_icache_flush    7
#define MEMF_no_icache_flush     (1U << _MEMF_no_icache_flush)
#define _MEMF_no_scrub           8
#define MEMF_no_scrub            (1U << _MEMF_no_scrub)
#define _MEMF_node               16
#define MEMF_node_mask           ((1U << (8 * sizeof(nodeid_t))) - 1)
#define MEMF_node(n)             ((((n) + 1) & MEMF_node_mask) << _MEMF_node)
#define MEMF_get_node(f)         ((((f) >> _MEMF_node) - 1) & MEMF_node_mask)
#define _MEMF_bits               24
#define MEMF_bits(n)             ((n) << _MEMF_bits)

#define VIRQ_ENOMEM      0
#define NUMA_NO_DISTANCE INT_MAX

#define SYS_STATE_active 1

#define string_param(name, var)
#define custom_param(name, fn)
#define size_param(name, var)
#define boolean_param(name, func)
#define integer_param(name, var)
#define STATIC_IF(x)   static
#define ACCESS_ONCE(x) (x)
#define likely(x)      (!!(x))
#define unlikely(x)    (!!(x))
#undef MB
#define MB(x) ((x) << 20)

#define cmpxchg(ptr, oldv, newv) \
    ({                           \
        *(ptr) = (newv);         \
        (oldv);                  \
    })
#define for_each_online_node(i) for ( (i) = 0; (i) < MAX_NUMNODES; ++(i) )
#define for_each_cpu(i, mask)   for ( (i) = 0; (i) < 1; ++(i) )
#define page_list_for_each_safe(pos, tmp, list)    \
    for ( (pos) = page_list_first(list),           \
          (tmp) = (pos) ? (pos)->list_next : NULL; \
          (pos) != NULL;                           \
          (pos) = (tmp), (tmp) = (pos) ? (pos)->list_next : NULL )

#define is_xen_heap_page(pg)              false
#define PFN_ORDER(pg)                     ((pg)->v.free.order)
#define page_to_mfn(pg)                   ((pg)->mfn)
#define page_to_nid(pg)                   ((pg)->nid)
#define page_to_virt(pg)                  ((void *)(pg))
#define virt_to_page(v)                   ((struct page_info *)(v))
#define mfn_to_page(mfn)                  (&test_dummy_page)
#define page_list_add(pg, list)           test_page_list_add((pg), (list))
#define page_list_add_tail(pg, list)      test_page_list_add_tail((pg), (list))
#define page_list_del(pg, list)           test_page_list_del((pg), (list))
#define page_list_empty(list)             test_page_list_empty((list))
#define page_list_first(list)             ((list)->head)
#define page_list_last(list)              ((list)->tail)
#define page_list_remove_head(list)       test_page_list_remove_head((list))
#define _mfn(x)                           ((mfn_t)(x))
#define mfn_x(x)                          ((unsigned long)(x))
#define mfn_add(mfn, nr)                  ((mfn) + (nr))
#define mfn_min(a, b)                     ((a) < (b) ? (a) : (b))
#define cpu_to_node(cpu)                  0U
#define smp_processor_id()                0U
#define smp_wmb()                         ((void)0)
#define node_online(node)                 ((node) < MAX_NUMNODES)
#define node_spanned_pages(node)          0UL
#define node_start_pfn(node)              0UL
#define __node_distance(a, b)             0
#define nodes_intersects(a, b)            true
#define nodes_and(dst, a, b)              ((dst) = (a) & (b))
#define nodes_andnot(dst, a, b)           ((dst) = (a) & ~(b))
#define nodes_clear(dst)                  ((dst) = 0)
#define nodemask_test(node, mask)         true
#define node_set(node, mask)              ((void)(mask))
#define node_clear(node, mask)            ((void)(mask))
#define node_test_and_set(node, mask)     false
#define first_node(mask)                  0U
#define next_node(node, mask)             MAX_NUMNODES
#define cycle_node(node, mask)            0U
#define cpumask_empty(mask)               true
#define cpumask_clear(mask)               ((void)(mask))
#define cpumask_and(dst, a, b)            ((void)(dst), (void)(a), (void)(b))
#define cpumask_or(dst, a, b)             ((void)(dst), (void)(a), (void)(b))
#define cpumask_copy(dst, src)            ((void)(dst), (void)(src))
#define cpumask_first(mask)               0U
#define cpumask_intersects(a, b)          false
#define cpumask_weight(mask)              1
#define __cpumask_set_cpu(cpu, mask)      ((void)(cpu), (void)(mask))
#define page_get_owner(pg)                ((pg)->owner)
#define page_set_owner(pg, d)             ((pg)->owner = (d))
#define page_get_owner_and_reference(pg)  ((pg)->owner)
#define page_set_tlbflush_timestamp(pg)   ((pg)->tlbflush_timestamp = 0)
#define set_gpfn_from_mfn(mfn, gpfn)      ((void)0)
#define page_is_offlinable(mfn)           true
#define is_xen_fixed_mfn(mfn)             false
#define filtered_flush_tlb_mask(ts)       ((void)(ts))
#define accumulate_tlbflush(need, pg, ts) ((void)(need), (void)(pg), (void)(ts))
#define flush_page_to_ram(mfn, icache)    ((void)(mfn), (void)(icache))
#define scrub_page_hot(ptr)               clear_page_hot(ptr)
#define scrub_page_cold(ptr)              clear_page_cold(ptr)
#define send_global_virq(virq)            ((void)(virq))
#define softirq_pending(cpu)              false
#define cpu_online(cpu)                   true
#define process_pending_softirqs()        ((void)0)
#define on_selected_cpus(msk, f, data, w) ((void)0)
#define cpu_relax()                       ((void)0)
#define xmalloc(type)                     calloc(1, sizeof(type))
#define xmalloc_array(type, nr)           calloc((nr), sizeof(type))
#define xvzalloc_array(type, nr)          calloc((nr), sizeof(type))
#define xvmalloc_array(type, nr)          calloc((nr), sizeof(type))
#define get_order_from_pages(nr)          0U
#define get_order_from_bytes(bytes)       0U
#define arch_mfns_in_directmap(mfn, nr)   true
#define mfn_to_virt(mfn)                  ((void *)&test_dummy_storage)
#define mfn_to_nid(mfn)                   0U
#define mfn_valid(mfn)                    true
#define maddr_to_mfn(pa)                  ((mfn_t)paddr_to_pfn(pa))
#define maddr_to_page(pa)                 (&test_dummy_page)

#define PG_OFFLINE_XENPAGE                    0x1U
#define PG_OFFLINE_FAILED                     0x2U
#define PG_OFFLINE_OWNER_SHIFT                8
#define PG_OFFLINE_NOT_CONV_RAM               0x4U
#define PG_OFFLINE_AGAIN                      0x8U
#define PG_OFFLINE_OFFLINED                   0x10U
#define PG_OFFLINE_BROKEN                     0x20U
#define PG_OFFLINE_OWNED                      0x40U
#define PG_OFFLINE_PENDING                    0x80U
#define PG_OFFLINE_ANONYMOUS                  0x100U
#define PG_ONLINE_FAILED                      0x200U
#define PG_ONLINE_BROKEN                      0x400U
#define PG_ONLINE_ONLINED                     0x800U
#define PG_OFFLINE_STATUS_OFFLINE_PENDING     0x1000U
#define PG_OFFLINE_STATUS_BROKEN              0x2000U
#define PG_OFFLINE_STATUS_OFFLINED            0x4000U
#define llc_coloring_enabled                  false
#define get_max_nr_llc_colors()               1U
#define page_to_llc_color(pg)                 0U
#define ASSERT_ALLOC_CONTEXT()                ((void)0)
#define arch_want_default_dmazone()           false
#define arch_get_dma_bitsize()                0U
#define arch_free_heap_page(d, pg)            ((void)(d), (void)(pg))
#define get_knownalive_domain(d)              ((void)(d))
#define page_to_list(d, pg)                   (&test_page_list)
#define domain_clamp_alloc_bitsize(d, bits)   (bits)
#define mem_paging_enabled(d)                 false
#define put_domain(d)                         ((void)(d))
#define dump_llc_coloring_info()              ((void)0)
#define register_keyhandler(k, fn, desc, irq) ((void)0)
#define __initcall(f)                         static int (*f##_ptr)(void) = (f)
#define rcu_lock_domain(id)                   (&test_dummy_domain)
#define rcu_lock_domain_by_any_id(id)         (&test_dummy_domain)
#define NOW()                                 0LL
#define system_state                          0
#define node_to_cpumask(node)                 (test_cpumask)
#define num_online_nodes()                    1

#define round_pgup(x)             (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define round_pgdown(x)           ((x) & ~(PAGE_SIZE - 1))
#define IS_ALIGNED(x, a)          (!((x) & ((a) - 1)))
#define clear_page_hot(ptr)       memset((ptr), 0, PAGE_SIZE)
#define clear_page_cold(ptr)      memset((ptr), 0, PAGE_SIZE)
#define __map_domain_page(pg)     (&test_dummy_storage)
#define unmap_domain_page(ptr)    ((void)(ptr))
#define put_page(pg)              ((void)(pg))
#define domain_crash(d)           ((void)(d))
#define BUG()                     BUG_ON(true)
#define dprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define gdprintk(level, fmt, ...) printk(fmt, ##__VA_ARGS__)
#define gprintk(level, fmt, ...)  printk(fmt, ##__VA_ARGS__)
#define panic(fmt, ...)                      \
    do                                       \
    {                                        \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        abort();                             \
    } while ( 0 )

static cpumask_t test_cpumask;
static nodemask_t node_online_map = 1;
static cpumask_t cpu_online_map   = 1;
static struct page_info test_dummy_page;
static struct page_list_head test_page_list;
static unsigned char test_dummy_storage[PAGE_SIZE];
static struct domain test_dummy_domain;
static struct domain *dom_cow;
static domid_t dom_io;
static domid_t dom_xen;
static struct vcpu test_current_vcpu;
static struct vcpu *current          = &test_current_vcpu;
static struct page_info *frame_table = &test_dummy_page;

void *alloc_xenheap_pages(unsigned int order, unsigned int memflags);
struct page_info *alloc_domheap_pages(struct domain *d, unsigned int order,
                                      unsigned int memflags);
void init_domheap_pages(paddr_t ps, paddr_t pe);

/* Return number of pages currently posessed by the domain */
static inline unsigned int domain_tot_pages(const struct domain *d)
{
    ASSERT(d->extra_pages <= d->tot_pages);
    return d->tot_pages - d->extra_pages;
}

static inline unsigned long simple_strtoul(const char *nptr,
                                           const char **endptr, int base)
{
    return strtoul(nptr, (char **)endptr, base);
}

#endif
