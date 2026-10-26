/* SPDX-License-Identifier: GPL-2.0-only */
/* Xen notemap and bitmap operations with native bitops */

#ifndef TOOLS_TESTS_INCLUDE_NATIVE_BITOPS_SHIM_H
#define TOOLS_TESTS_INCLUDE_NATIVE_BITOPS_SHIM_H

#include "xen-lib.h"

#define arch_test_bit(nr, addr)                                   \
        ((__atomic_load_n((const volatile bitop_uint_t *)(addr) + \
                          BITOP_WORD(nr),                         \
                          __ATOMIC_SEQ_CST) &BITOP_MASK(nr)) != 0)
#define test_and_set_bit arch__test_and_set_bit
#define arch__test_and_set_bit(nr, addr)                      \
        ((__atomic_fetch_or((volatile bitop_uint_t *)(addr) + \
                            BITOP_WORD(nr), BITOP_MASK(nr),   \
                            __ATOMIC_SEQ_CST) &BITOP_MASK(nr)) != 0)
#define test_and_clear_bit arch__test_and_clear_bit
#define arch__test_and_clear_bit(nr, addr)                     \
        ((__atomic_fetch_and((volatile bitop_uint_t *)(addr) + \
                             BITOP_WORD(nr), ~BITOP_MASK(nr),  \
                             __ATOMIC_SEQ_CST) &BITOP_MASK(nr)) != 0)
#define __set_bit set_bit
#define set_bit(nr, addr)                                   \
        __atomic_fetch_or((volatile bitop_uint_t *)(addr) + \
                          BITOP_WORD(nr), BITOP_MASK(nr), __ATOMIC_SEQ_CST)
#define __clear_bit clear_bit
#define clear_bit(nr, addr)                                  \
        __atomic_fetch_and((volatile bitop_uint_t *)(addr) + \
                           BITOP_WORD(nr), ~BITOP_MASK(nr), __ATOMIC_SEQ_CST)

#define _X86_BITOPS_H
#define _ARM_BITOPS_H
#define ASM__RISCV__BITOPS_H
#include <xen/cpumask.h>
#if NR_CPUS > 4 * BITS_PER_LONG
unsigned int nr_cpumask_bits = BITS_TO_LONGS(NR_CPUS) * BITS_PER_LONG;
#endif

#define __X86_UACCESS_H__
#define __ASM_X86_GUEST_ACCESS_H__
#define __ASM_ARM_GUEST_ACCESS_H__
#define ASM__RISCV__GUEST_ACCESS_H
#define raw_copy_to_guest(t, f, l) (__builtin_memcpy(t, f, l), 0UL)
#define raw_copy_from_guest(t, f, l) (__builtin_memcpy(t, f, l), 0UL)
#define __raw_copy_to_guest raw_copy_to_guest
#define __raw_copy_from_guest raw_copy_from_guest

#define hweightl(v) __builtin_popcountl(v)
#include <common/bitmap.c>
#undef __XEN_TOOLS__
#include <lib/find-next-bit.c>
#define __XEN_TOOLS__
#include <lib/generic-ffsl.c>
#include <lib/generic-flsl.c>

#endif /* TOOLS_TESTS_INCLUDE_NATIVE_BITOPS_SHIM_H */
