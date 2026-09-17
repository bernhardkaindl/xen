/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host bit operation setup for native Xen tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_BITOPS_H
#define TOOLS_TESTS_NATIVE_HARNESS_BITOPS_H

/* Atomic bit operations for the host, based on GCC built-in atomics. */
#define arch_test_bit(nr, addr) \
    ((__atomic_load_n((const volatile bitop_uint_t *)(addr) + \
                      BITOP_WORD(nr), __ATOMIC_SEQ_CST) & BITOP_MASK(nr)) != 0)
#define test_and_set_bit arch__test_and_set_bit
#define arch__test_and_set_bit(nr, addr) \
    ((__atomic_fetch_or((volatile bitop_uint_t *)(addr) + \
                        BITOP_WORD(nr), BITOP_MASK(nr), \
                        __ATOMIC_SEQ_CST) & BITOP_MASK(nr)) != 0)
#define test_and_clear_bit arch__test_and_clear_bit
#define arch__test_and_clear_bit(nr, addr) \
    ((__atomic_fetch_and((volatile bitop_uint_t *)(addr) + \
                         BITOP_WORD(nr), ~BITOP_MASK(nr), \
                         __ATOMIC_SEQ_CST) & BITOP_MASK(nr)) != 0)
#define __set_bit set_bit
#define set_bit(nr, addr) \
    __atomic_fetch_or((volatile bitop_uint_t *)(addr) + \
                      BITOP_WORD(nr), BITOP_MASK(nr), __ATOMIC_SEQ_CST)
#define __clear_bit clear_bit
#define clear_bit(nr, addr) \
    __atomic_fetch_and((volatile bitop_uint_t *)(addr) + \
                       BITOP_WORD(nr), ~BITOP_MASK(nr), __ATOMIC_SEQ_CST)

#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#endif
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#define __XEN_BUG_H__
#define __XEN_ERRNO_H__
#define _X86_BITOPS_H
#define _ARM_BITOPS_H
#define ASM__RISCV__BITOPS_H
#include <xen/cpumask.h>
#pragma GCC diagnostic pop

#endif
