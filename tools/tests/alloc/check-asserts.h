/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Helpers for assertions used by unit tests.
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_ALLOC_CHECK_ASSERTS_H
#define TOOLS_TESTS_ALLOC_CHECK_ASSERTS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

static bool testcase_assert_expected_to_fail = false;
static bool testcase_assert_verbose_assertions = true;
static const char *testcase_assert_current_func = NULL;
static int testcase_assert_verbose_indent_level = 0;
static int testcase_assert_expected_failures = 0;
static int testcase_assert_expected_failures_total = 0;
static int testcase_assert_successful_assert_total = 0;
static void (*testcase_init_func)(int);
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
#define EXPECTED_TO_FAIL_BEGIN() (testcase_assert_expected_to_fail = true)
#define EXPECTED_TO_FAIL_END(c) testcase_assert_check_expected_failures(c)

static void __used testcase_assert_check_expected_failures(int expected)
{
    if ( testcase_assert_expected_failures != expected )
    {
        fprintf(stderr, "Expected %d assertion failures, got %d\n",
                expected, testcase_assert_expected_failures);
        abort();
    }
    testcase_assert_expected_to_fail = false;
    testcase_assert_expected_failures = 0;
    testcase_assert_expected_failures_total += expected;
}

static void __used setup_testcase_init_func(void (*init_fn)(int))
{
    testcase_init_func = init_fn;
}

__attribute__((format(printf, 5, 6)))
static void testcase_assert(bool condition, const char *file, int line,
                            const char *func, const char *fmt, ...)
{
    va_list ap;
    const char *relpath = file;

    while ( (file = strstr(relpath, "../")) )
        relpath += 3;

    va_start(ap, fmt);
    if ( testcase_assert_expected_to_fail )
    {
        fprintf(stderr, "\n- Test assertion %s at %s:%d:\n  ",
                condition ? "unexpectedly passed" : "failed as expected",
                relpath, line);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");

        if ( condition )
            abort(); /* Unexpected pass, treat as test failure */
        else
            testcase_assert_expected_failures++; /* Count for the report. */
        goto out;
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

struct testcase {
    const char *name;              /* test case name */
    const char *tid;               /* Test ID */
    int         intarg;            /* passed to the test case */
    void        (*func)(int);      /* Test case function */
    int         passed_asserts;    /* Number of ASSERTS that passed. */
    int         expected_failures; /* Number of XFAILs */
} testcases[40];
struct testcase *current_testcase = testcases;

static void print_testcase_report(struct testcase *tc)
{
    printf("- %-5s %-34s %2d: %3d assertions passed", tc->tid, tc->name,
           tc->intarg, tc->passed_asserts);
    if ( tc->expected_failures )
        printf(" (%2d XFAIL)", tc->expected_failures);
    printf("\n");
}

static void run_testcase(void (*case_func)(int), int int_arg, const char *tid,
                         const char *case_name)
{
    printf("\nTest Case: %s...\n", case_name);
    current_testcase->name = case_name;
    current_testcase->func = case_func;
    current_testcase->intarg = int_arg;
    current_testcase->tid = tid;
    current_testcase->passed_asserts = 0;
    current_testcase->expected_failures = 0;

    if ( testcase_init_func && int_arg >= 0 )
        testcase_init_func(int_arg);

    case_func(int_arg);

    current_testcase->passed_asserts = testcase_assert_successful_assert_total;
    current_testcase->expected_failures =
        testcase_assert_expected_failures_total;

    testcase_assert_successful_assert_total = 0;
    testcase_assert_expected_failures_total = 0;

    printf("\nResults:\n");
    print_testcase_report(current_testcase);
    current_testcase++;
}
#define RUN_TESTCASE(tid, func, arg) run_testcase(func, arg, #tid, #func)

static int testcase_print_summary(const char *argv0)
{
    struct utsname uts;
    int total_asserts = 0, expected_failures = 0;

    fprintf(stderr, "\nTest Report:\n");

    current_testcase = testcases;
    for ( size_t i = 0; i < ARRAY_SIZE(testcases) && current_testcase->func;
          i++ )
    {
        print_testcase_report(current_testcase);
        total_asserts += current_testcase->passed_asserts;
        expected_failures += current_testcase->expected_failures;
        current_testcase++;
    }
    current_testcase->tid = "Total";
    current_testcase->name = "";
    current_testcase->passed_asserts = total_asserts;
    current_testcase->expected_failures = expected_failures;
    current_testcase->intarg = current_testcase - testcases;
    print_testcase_report(current_testcase);

    uname(&uts);
    printf("\nTest suite %s for %s completed.\n", argv0, uts.machine);
    return 0;
}

static const char *parse_args(int argc, char *argv[], const char *topic)
{
    const char *program_name = argv[0];
    struct utsname uts;

    if ( argc != 1 )
    {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        return NULL;
    }
    program_name = strrchr(program_name, '/');
    if ( program_name )
        program_name++;
    else
        program_name = argv[0];

    uname(&uts);
    printf("Suite : %s\n", program_name);
    printf("Topic : %s\n", topic);
    printf("Config: CONFIG_NUMA %s\n",
           config_enabled(CONFIG_NUMA) ? "enabled" : "disabled");
#ifndef __clang__
    printf("Target: gcc %s/%s\n", __VERSION__, uts.machine);
#else
    printf("Target: %s/%s\n", __VERSION__, uts.machine);
#endif
    return program_name;
}
#endif
