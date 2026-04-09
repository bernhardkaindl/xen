/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Header-only library for check assertions in unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef _CHECK_ASSERTS_H_
#define _CHECK_ASSERTS_H_

#include <assert.h>
#include <execinfo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define __used __attribute__((__used__))

static bool testcase_assert_expected_to_fail        = false;
static bool testcase_assert_verbose_assertions      = true;
static int  testcase_assert_expected_failures       = 0;
static int  testcase_assert_expected_failures_total = 0;
static int  testcase_assert_successful_assert_total = 0;
#define assert_failed_str        "Assertion failed: "
#define EXPECTED_TO_FAIL_BEGIN() (testcase_assert_expected_to_fail = true)
#define EXPECTED_TO_FAIL_END(c)  testcase_assert_check_expected_failures(c)

static void __used testcase_assert_check_expected_failures(int expected)
{
    if ( testcase_assert_expected_failures != expected )
    {
        fprintf(stderr, "Test assertion expected %d failures, but got %d\n",
                expected, testcase_assert_expected_failures);
        abort();
    }
    testcase_assert_expected_to_fail  = false;
    testcase_assert_expected_failures = 0;
    testcase_assert_expected_failures_total += expected;
}

static void testcase_assert(bool condition, const char *file, int line,
                            const char *func, const char *fmt, ...)
{
    va_list     ap;
    const char *relpath = file;

    while ( (file = strstr(relpath, "../")) )
        relpath += 3;

    va_start(ap, fmt);
    if ( testcase_assert_expected_to_fail )
    {
        if ( condition )
        {
            fprintf(stderr,
                    "\n- Test assertion unexpectedly passed at %s:%d:\n  ",
                    relpath, line);
            vfprintf(stderr, fmt, ap);
            fprintf(stderr, "\n");
            abort();
        }
        else
        {
            fprintf(stderr,
                    "\n- Test assertion expectedly failed at %s:%d:\n  ",
                    relpath, line);
            vfprintf(stderr, fmt, ap);
            va_end(ap);
            fprintf(stderr, "\n");
            testcase_assert_expected_failures++;
        }
        return;
    }
    if ( !condition )
    {
        fprintf(stderr, "Test assertion failed at %s:%d: ", relpath, line);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        abort();
    }
    testcase_assert_successful_assert_total++;
    if ( testcase_assert_verbose_assertions )
    {
        /* As the assertion didn't actually fail, remove the prefix */
        if ( strncmp(fmt, assert_failed_str, strlen(assert_failed_str)) == 0 )
            fmt += strlen(assert_failed_str);

        fprintf(stdout, "%s:%d: ", relpath, line);
        /* Skip the func if relpath starts with test- */
        if ( strncmp(relpath, "test-", strlen("test-")) &&
             strncmp(func, "test_", strlen("test_")) != 0 )
            fprintf(stdout, "%s(): ", func);

        if ( strncmp(fmt, "BUG_ON:", strlen("BUG_ON:")) )
            fprintf(stdout, "ASSERT(");

        vfprintf(stdout, fmt, ap);
        va_end(ap);

        if ( strncmp(fmt, "BUG_ON:", strlen("BUG_ON:")) )
            fprintf(stdout, ")");

        fprintf(stdout, "\n");
    }
}

/* Generate a report from the test results */
static void testcase_print_summary(const char *argv0)
{
    fprintf(stderr, "%s: Test summary: ", argv0);
    if ( testcase_assert_expected_failures_total )
    {
        fprintf(stderr, "%d expected assertion failures\n",
                testcase_assert_expected_failures_total);
        fprintf(stderr, "%s: Test summary: ", argv0);
        fprintf(stderr, "%d of %d assertions passed\n",
                testcase_assert_successful_assert_total,
                testcase_assert_successful_assert_total +
                    testcase_assert_expected_failures_total);
    }
    else
        fprintf(stderr, "All %d assertions passed\n",
                testcase_assert_successful_assert_total);
}

#define CHECK(condition, fmt, ...) \
    testcase_assert(condition, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define ASSERT(x) \
    testcase_assert(x, __FILE__, __LINE__, __func__, assert_failed_str #x)

#define ASSERT_UNREACHABLE() \
    testcase_assert(0, __FILE__, __LINE__, __func__, "Unreachable code reached")

#define BUG_ON(x) \
    testcase_assert(!(x), __FILE__, __LINE__, __func__, "BUG_ON: " #x)

#endif /* _CHECK_ASSERTS_H_ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
