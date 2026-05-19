/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Proxy to include definitions from xen/mm.h for native tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_MM_PROXY_H
#define TOOLS_TESTS_NATIVE_HARNESS_MM_PROXY_H

#ifndef TEST_WRAP_XEN_INCLUDE_XEN_MM_H
#warning "Include this header only in integration tests that need xen/mm.h"
#else
#include "common.h"

#define __XEN_KCONFIG_H
#include <xen/config.h>
#undef cf_check
#define cf_check __used

#define register_t foo; /* Workaround for arm64 sys/types conflict */
#include <asm/types.h>
#undef  register_t

/*
 * In the real hypervisor, the frame table is located at a fixed virtual
 * address, but in the test harness, it is allocated in BSS or heap, so
 * replace the fixed virtual address definitions with dynamic ones.
 * xen/mm.h defines the frame_table define put point to the virt start.
 */
#undef FRAMETABLE_VIRT_START
#undef FRAMETABLE_VIRT_END
unsigned long FRAMETABLE_VIRT_START;
unsigned long FRAMETABLE_VIRT_END;

/*
 * The test environment needs the virtual address of the direct map which
 * the test environment emulates with the bss symbol directmap_virt_start
 * defined below.  In the real hypervisor, this is a fixed virtual address,
 * but in the test environment, it is allocated in BSS, so replace the fixed
 * virtual address definition with a dynamic one.
 *
 * For example, page_set_owner() and page_get_owner() are used to get and set
 * the virtual address of the owner domain of a page. They use pdx_to_virt()
 * and virt_to_pdx() which use DIRECTMAP_VIRT_START to translate between the
 * page_info struct's pdx-encoded owner field and the virtual address of
 * the owner domain, so setting DIRECTMAP_VIRT_START to the address of a
 * an early page-aligned symbol in the test binary (e.g. test_bss_start)
 * allows the test code to set up and get page ownership.
 */
#undef DIRECTMAP_VIRT_START
unsigned long DIRECTMAP_VIRT_START = (unsigned long)&test_bss_start;

/* Shim definitions for including xen/mm.h in the test context. */
#define maddr_to_directmapoff(ma)  ((unsigned long)(ma))
#define directmapoff_to_maddr(off) ((paddr_t)(off))
#define perfc_incr(x)              ((void)0)
#define cpumask_copy(dst, src)     ((void)(dst), (void)(src))
#define cpumask_empty(mask)        true

unsigned long phys_offset;
unsigned long xen_phys_start;
char __init_begin[1], __init_end[1];
struct domain; /* Forward declaration for struct domain* used in arguments */

/* Include xen/mm.h into the test context */
#include <xen/macros.h>
#include <asm/types.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#endif
#include <xen/mm.h>
#pragma GCC diagnostic pop

/* Provide definitions for a minimum synthetic heap */
#undef page_to_mfn
#undef mfn_to_page
#undef mfn_valid
#define page_to_mfn(pg)    _mfn((pg) - &frame_table[0])
#define mfn_to_page(mfn)   (&frame_table[mfn_x(mfn)])
#define mfn_valid(mfn) \
        (mfn_x(mfn) >= mfn_x(first_valid_mfn) &&  mfn_x(mfn) < max_page)

/* Provide virtual addresses for synthetic memory provided by the harness */
#undef mfn_to_virt
#undef __mfn_to_virt
#undef page_to_virt
#undef virt_to_page
static unsigned char       test_dummy_storage[PAGE_SIZE];
#define mfn_to_virt(mfn)   ((void *)&test_dummy_storage)
#define __mfn_to_virt(mfn) mfn_to_virt(mfn)
#define page_to_virt(pg)   ((void *)(pg))
#define virt_to_page(v)    ((struct page_info *)(v))

/* Not supported by the test harness yet, only used by heap init functions. */
#undef maddr_to_page
#define maddr_to_page(pa) (ASSERT_UNREACHABLE(), NULL)
#endif

#if defined(__arm__) || defined(__aarch64__)
void dump_hyp_walk(vaddr_t addr)
{
    (void)addr;
}

void __attribute__((__noreturn__)) panic_PAR(uint64_t par)
{
    (void)par;
    ASSERT_UNREACHABLE();
}
#endif

#if defined(__riscv)
vaddr_t directmap_virt_start;
#endif

#endif
