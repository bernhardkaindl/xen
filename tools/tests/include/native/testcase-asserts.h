/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Functions for running test cases and reporting results.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_TESTCASE_ASSERTS_H
#define TOOLS_TESTS_NATIVE_HARNESS_TESTCASE_ASSERTS_H

/* Common macros compatible with the test environment. */
#include "xen-macros.h"

#define ENSURE(cond, fmt, ...) \
    do { \
        ASSERT(cond); \
        printf(fmt "\n", ##__VA_ARGS__); \
    } while ( 0 )

static unsigned int test_functions_run;

static int test_complete(void)
{
    printf("Ran %u test functions.\n", test_functions_run);
    return 0;
}

#endif
