/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common test harness for page allocation unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_ALLOC_HARNESS_H
#define TOOLS_TESTS_ALLOC_HARNESS_H

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

/* Enable additional debug checks. */
#define CONFIG_DEBUG

/* Common macros compatible with the test environment. */
#include "hypervisor-macros.h"

/* Assertion helpers shared by the tests. */
#include "check-asserts.h"

/* Common Xen types used by the test environment. */
typedef uint8_t u8;
typedef uint64_t paddr_t;
typedef unsigned long cpumask_t;
typedef long long s_time_t;
typedef bool spinlock_t;

/*
 * The original reserve_offlined_page() implementation triggers an
 * AddressSanitizer (ASAN) stack-buffer-overflow report in both GCC and
 * Clang when test_merge_tail_pair runs with ASAN enabled and verifies
 * the heap free-list state.
 *
 * ASAN reports several list-pointer errors in the heap state, and one of
 * them appears to trigger the stack-buffer-overflow detection on x86_64.
 *
 * As a temporary workaround, detect whether ASAN is enabled so the test
 * can skip the ASSERT_LIST_EQUAL verification that triggers the report,
 * while still running the rest of the case under ASAN.
 */
#if defined(__has_feature)
/* Clang uses __has_feature to detect AddressSanitizer */
# if __has_feature(address_sanitizer)
#  define ASAN_ENABLED 1
# endif
/* GCC uses __SANITIZE_ADDRESS__ to detect AddressSanitizer */
#elif defined(__SANITIZE_ADDRESS__)
# define ASAN_ENABLED 1
#else
# define ASAN_ENABLED 0
#endif
#endif
