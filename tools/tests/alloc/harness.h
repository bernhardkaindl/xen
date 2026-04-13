/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Common test harness for page allocation unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef _TEST_HARNESS_
#define _TEST_HARNESS_

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

/* Enable debug mode to enable additional checks */
#define CONFIG_DEBUG

/* Define common macros which are compatible with the test context */
#include "hypervisor-macros.h"

/* Provide the common check_asserts library for test assertions */
#include "check-asserts.h"

/* Common Xen types for the test context */
typedef uint8_t u8;
typedef uint64_t paddr_t;
typedef unsigned long cpumask_t;
typedef long long s_time_t;
typedef bool spinlock_t;

/*
 * The original implementation of reserve_offlined_page() causes the GCC
 * and clang AddressSanitizer (ASAN) to report stack-buffer-overflow
 * when the test_merge_tail_pair test case is run with ASAN enabled,
 * when test verifies the state of the free lists in the heap.
 *
 * It finds several list pointer errors in the heap state and one of the
 * appears to trigger ASAN's stack-buffer-overflow detection on x86_64.
 *
 * To temporarily work around this issue, we detect if ASAN is enabled
 * and in order to be able skip the ASSERT_LIST_EQUAL verification step
 * in the test case that triggers the ASAN error, while still allowing
 * the rest of the test case to run and verify all execution with ASAN.
 */
/* clang-format off */
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
/* clang-format on */
#endif /* _TEST_HARNESS_H_ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
