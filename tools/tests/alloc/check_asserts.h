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
#include <sys/utsname.h>

#define __used __attribute__((__used__))

static bool        testcase_assert_expected_to_fail        = false;
static bool        testcase_assert_verbose_assertions      = true;
static const char *testcase_assert_verbose_called          = NULL;
static int         testcase_assert_verbose_indent_level    = 0;
static int         testcase_assert_expected_failures       = 0;
static int         testcase_assert_expected_failures_total = 0;
static int         testcase_assert_successful_assert_total = 0;
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

        if ( strcmp(fmt, "ret == 0") == 0 )
            return;

        for ( int i = 0; i < testcase_assert_verbose_indent_level; i++ )
            fprintf(stdout, "  ");

        fprintf(stdout, "%s:%d: ", relpath, line);
        /* Skip the func if relpath starts with test- */
        if ( testcase_assert_verbose_called == NULL ||
             strcmp(testcase_assert_verbose_called, func) != 0 )
        {
            // testcase_assert_verbose_called = func;
            if ( strncmp(relpath, "test-", strlen("test-")) &&
                 strncmp(func, "test_", strlen("test_")) != 0 )
                fprintf(stdout, "%s(): ", func);
        }

        if ( strncmp(fmt, "BUG_ON:", strlen("BUG_ON:")) )
            fprintf(stdout, "ASSERT(");

        vfprintf(stdout, fmt, ap);
        va_end(ap);

        if ( strncmp(fmt, "BUG_ON:", strlen("BUG_ON:")) )
            fprintf(stdout, ")");

        fprintf(stdout, "\n");
    }
}

#define CHECK(condition, fmt, ...) \
    testcase_assert(condition, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define ASSERT(x) \
    testcase_assert(x, __FILE__, __LINE__, __func__, assert_failed_str #x)

#define BUG_ON(x) \
    testcase_assert(!(x), __FILE__, __LINE__, __func__, "BUG_ON: " #x)

#define ASSERT_UNREACHABLE() assert(false)

static void (*testcase_init_func)(const char *, int);
struct testcase {
    const char *name;
    int         intarg;
    void (*func)(int);
    int passed_asserts;
    int expected_failures;
} testcases[40];
struct testcase *current_testcase = testcases;

static void print_testcase_report(struct testcase *tc)
{
    printf("- %-34s %2d: %3d assertions passed", tc->name, tc->intarg,
           tc->passed_asserts);
    if ( tc->expected_failures )
        printf(" (%2d XFAIL)", tc->expected_failures);
    printf("\n");
}

/* Execute the given test function and record the number of assertions */
static void run_testcase(void (*case_func)(int), int mfn, const char *case_name)
{
    printf("\nTest Case: %s...\n", case_name);
    current_testcase->name              = case_name;
    current_testcase->func              = case_func;
    current_testcase->intarg            = mfn;
    current_testcase->passed_asserts    = 0;
    current_testcase->expected_failures = 0;
    if ( testcase_init_func && mfn >= 0 )
        testcase_init_func(case_name, mfn);
    case_func(mfn);
    current_testcase->passed_asserts = testcase_assert_successful_assert_total;
    current_testcase->expected_failures =
        testcase_assert_expected_failures_total;
    testcase_assert_successful_assert_total = 0;
    testcase_assert_expected_failures_total = 0;
    printf("\nResults:\n");
    print_testcase_report(current_testcase);
    current_testcase++;
}
#define RUN_TESTCASE(func, mfn) run_testcase(func, mfn, #func)

/* Generate a report from the test results */
static int testcase_print_summary(const char *argv0)
{
    struct utsname uts;
    int            total_asserts = 0, expected_failures = 0;

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
    current_testcase->name              = "Total";
    current_testcase->passed_asserts    = total_asserts;
    current_testcase->expected_failures = expected_failures;
    current_testcase->intarg            = current_testcase - testcases;
    print_testcase_report(current_testcase);

    uname(&uts);
    printf("\nTest suite %s for %s completed.\n", argv0, uts.machine);
    return 0;
}

static const char *parse_args(int argc, char *argv[], const char *topic)
{
    const char    *program_name = argv[0];
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
    printf("Suite   : %s\n", program_name);
    printf("Topic   : %s\n", topic);
    printf("Config  : CONFIG_NUMA %s\n",
           config_enabled(CONFIG_NUMA) ? "enabled" : "disabled");
#ifdef CONFIG_NUMA
    printf("enabled\n");
#else
    printf("disabled\n");
#endif

#ifndef __clang__
    printf("Target: gcc %s/%s\n", __VERSION__, uts.machine);
#else
    printf("Target: %s/%s\n", __VERSION__, uts.machine);
#endif
    return program_name;
}

static void __used setup_testcase_init_func(void (*init_f)(const char *, int))
{
    testcase_init_func = init_f;
}

#endif /* _CHECK_ASSERTS_H_ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
