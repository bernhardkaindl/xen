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

/* Macros for assertions and test case management. */
#ifndef _SUBPATH_
#define _SUBPATH_ "tools/tests/native"
#endif
#define __used __attribute__((__used__))
#define assert_failed_str "Assertion failed: "
#define CHECK(condition, fmt, ...)                                    \
        testcase_assert(condition, __FILE__, __LINE__, __func__, fmt, \
                        ##__VA_ARGS__)
#define ASSERT(x) \
        testcase_assert(x, __FILE__, __LINE__, __func__, assert_failed_str #x)
#define BUG_ON(x) \
        testcase_assert(!(x), __FILE__, __LINE__, __func__, "BUG_ON: " #x)
#define ASSERT_UNREACHABLE() assert(false)
#define EXPECT_FAIL_BEGIN() (testcase_assert_expect_fail = true)
#define EXPECT_FAIL_END(c) testcase_assert_check_expected_failures(c)

/* Structure to hold information about each test case. */
struct testcase {
    const char *name;              /* test case name */
    const char *tid;               /* Test ID */
    int         intarg;            /* passed to the test case */
    void        (*func)(int);      /* Test case function */
    int         passed_asserts;    /* Number of ASSERTS that passed. */
    int         expected_failures; /* Number of XFAILs */
    bool        disabled;          /* Whether the test case is disabled */
    bool        failed;            /* Whether the test case failed */
} testcases[40];

/* Global variables to track the current test case and assertion results. */
struct testcase *current_testcase = testcases;
static bool testcase_assert_expect_fail = false;
static bool testcase_assert_verbose_assertions;
static int testcase_assert_verbose_indent_level;
static int testcase_assert_expected_failures;
static int testcase_assert_expected_failures_total;
static int testcase_assert_successful_assert_total;
static void (*testcase_init_func)(int);
static char **testcase_assert_enabled_tests;
static const char *testcase_program_name;
static const char *testcase_assert_current_func;

/* Print a report for a single test, including its ID, name, and results. */
static void testcase_print_tid_report(struct testcase *tc)
{
    printf("- %-5s %-34s %2d: ", tc->tid, tc->name, tc->intarg);
    if ( tc->disabled )
        printf("    disabled"); /* This test case was disabled */
    else
    {
        printf("%3d assertions passed", tc->passed_asserts);
        if ( tc->failed )
            printf(" (FAILED)");
        if ( tc->expected_failures )
            printf(" (%2d XFAIL)", tc->expected_failures);
    }
    printf("\n");
}

/* Print a summary of all test cases, including totals of the enabled tests. */
static void testcase_print_summary(void)
{
    int total_asserts = 0, expected_failures = 0;

    printf("\nTest Report:\n");

    current_testcase = testcases;
    for ( size_t i = 0; i < ARRAY_SIZE(testcases) && current_testcase->func;
          i++ )
    {
        testcase_print_tid_report(current_testcase);
        total_asserts += current_testcase->passed_asserts;
        expected_failures += current_testcase->expected_failures;
        current_testcase++;
    }
    current_testcase->tid = "Total";
    current_testcase->name = "";
    current_testcase->passed_asserts = total_asserts;
    current_testcase->expected_failures = expected_failures;
    current_testcase->intarg = current_testcase - testcases;
    testcase_print_tid_report(current_testcase);

    /* Print if testcase_assert_enabled_tests has tests which were enabled */
    if ( *testcase_assert_enabled_tests )
    {
        printf("\nEnabled tests:");
        for ( char **p = testcase_assert_enabled_tests; *p; p++ )
        {
            printf(" %s", *p);
            /* Check if the enabled test was actually run, if not print a warning. */
            bool found = false;
            current_testcase = testcases;
            for ( size_t i = 0;
                  i < ARRAY_SIZE(testcases) && current_testcase->func; i++ )
            {
                if ( strcmp(current_testcase->tid, *p) == 0 )
                {
                    found = true;
                    break;
                }
                current_testcase++;
            }
            if ( !found )
                printf(" (WARNING: test not found)");
        }
        printf("\n");
    }
}

/* Print a test report and complete the test program successfully. */
static int test_complete(void)
{
    testcase_print_summary();
    printf("\nTest program %s completed.\n", testcase_program_name);
    return 0;
}

/* Print a test report and abort the test program. */
static void test_abort(void)
{
    testcase_print_summary();
    printf("\nTest program %s aborted.\n", testcase_program_name);
    abort();
}

/* Check if the expected number of assertion failures occurred. */
static void __used testcase_assert_check_expected_failures(int expected)
{
    if ( testcase_assert_expected_failures != expected )
    {
        fprintf(stderr, "Expected %d assertion failures, got %d\n",
                expected, testcase_assert_expected_failures);
        current_testcase->failed = true;
        test_abort();
    }
    testcase_assert_expect_fail = false;
    testcase_assert_expected_failures = 0;
    testcase_assert_expected_failures_total += expected;
}

/* Set up the initialization function for a test case. */
static void __used setup_testcase_init_func(void (*init_fn)(int))
{
    testcase_init_func = init_fn;
}

/* Assert a condition, record the result and print a message if it fails. */
__attribute__((format(printf, 5, 6)))
static void testcase_assert(bool condition, const char *file, int line,
                            const char *func, const char *fmt, ...)
{
    va_list ap;
    const char *relpath = file;

    fflush(stdout);
    while ( (file = strstr(relpath, "../")) )
        relpath += 3;

    va_start(ap, fmt);
    if ( testcase_assert_expect_fail )
    {
        fprintf(stderr, "\n- Test assertion %s at %s:%d:\n  ",
                condition ? "unexpectedly passed" : "failed as expected",
                relpath, line);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n\n");

        if ( condition )
        {
            current_testcase->failed = true;
            test_abort(); /* Unexpected pass, treat as test failure */
        }
        else
            testcase_assert_expected_failures++; /* Count for the report. */
        goto out;
    }
    if ( !condition )
    {
        fprintf(stderr, "Test assertion failed at %s:%d: ", relpath, line);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n\n%s/%s:%d, %s():\n", _SUBPATH_, relpath, line, func);
        fprintf(stderr, "Testcase %s(%s, %d) failed!\n", current_testcase->name,
                current_testcase->tid, current_testcase->intarg);
        current_testcase->failed = true;
        test_abort();
    }
    testcase_assert_successful_assert_total++;
    if ( testcase_assert_verbose_assertions )
    {
        if ( strncmp(fmt, assert_failed_str, strlen(assert_failed_str)) == 0 )
            fmt += strlen(assert_failed_str);

        if ( strcmp(fmt, "ret == 0") == 0 )
            goto out;

        for ( int i = 0; i < testcase_assert_verbose_indent_level; i++ )
            printf("  ");

        printf("%s:%d: ", relpath, line);
        if ( (testcase_assert_current_func == NULL ||
              strcmp(testcase_assert_current_func, func)) &&
             (strncmp(relpath, "test-", strlen("test-")) &&
              strncmp(func, "test_", strlen("test_"))) )
            printf("%s(): ", func);

        if ( strncmp(fmt, "BUG_ON:", 7) )
            printf("ASSERT(");

        vprintf(fmt, ap);

        if ( strncmp(fmt, "BUG_ON:", 7) )
            printf(")");

        printf("\n");
    }
 out:
    va_end(ap);
}

/* Run a test case function with the given argument and report results. */
static void run_testcase(void (*case_func)(int), int int_arg, const char *tid,
                         const char *case_name)
{
    current_testcase->name = case_name;
    current_testcase->func = case_func;
    current_testcase->intarg = int_arg;
    current_testcase->tid = tid;
    current_testcase->passed_asserts = 0;
    current_testcase->expected_failures = 0;

    if ( *testcase_assert_enabled_tests )
    {
        bool enabled = false;

        for ( char **p = testcase_assert_enabled_tests; *p; p++ )
        {
            if ( strcmp(*p, tid) == 0 )
            {
                enabled = true;
                break;
            }
        }
        if ( !enabled )
        {
            current_testcase->disabled = true;
            goto skip;
        }
    }
    printf("\nTest Case: %s...\n", case_name);

    if ( testcase_init_func && int_arg >= 0 )
        testcase_init_func(int_arg);

    case_func(int_arg);

    current_testcase->passed_asserts = testcase_assert_successful_assert_total;
    current_testcase->expected_failures =
        testcase_assert_expected_failures_total;

    testcase_assert_successful_assert_total = 0;
    testcase_assert_expected_failures_total = 0;

    printf("\nResults:\n");
    testcase_print_tid_report(current_testcase);
 skip:
    current_testcase++;
    if ( current_testcase - testcases >= (long)ARRAY_SIZE(testcases) )
    {
        fprintf(stderr, "Too many tests, increase the size of testcases[]\n");
        test_abort();
    }
}

/* Macro to run a test case with the given ID, function, and argument. */
#define RUN_TESTCASE(tid, func, arg) run_testcase(func, arg, tid, #func)

/* Parse command-line arguments and return the program name. */
static const char *parse_args(int argc, char *argv[], const char *topic)
{
    const char *program_name = argv[0];
    (void)argc;

    program_name = strrchr(program_name, '/');
    if ( program_name )
        program_name++;
    else
        program_name = argv[0];

    testcase_assert_enabled_tests = &argv[1];
    printf("Program: %s\n", program_name);
    printf("Topic  : %s\n", topic);
    memset(testcases, 0, sizeof(testcases));
    testcase_program_name = program_name;
    return program_name;
}

#endif
