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

/* Include the common check_asserts library for test assertions */
#include "check_asserts.h"

/* Common Xen types for the test context */
typedef uint8_t       u8;
typedef uint16_t      domid_t;
typedef uint64_t      paddr_t;
typedef unsigned long cpumask_t;
typedef long long     s_time_t;
typedef bool          spinlock_t;

#endif /* _TEST_HARNESS_H_ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
