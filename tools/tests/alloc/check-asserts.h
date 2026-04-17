/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Helpers for assertions used by unit tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_ALLOC_CHECK_ASSERTS_H
#define TOOLS_TESTS_ALLOC_CHECK_ASSERTS_H

#include <assert.h>
#include <execinfo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#ifndef CONFIG_NUMA
#define CONFIG_NUMA 0
#endif
#define __used __attribute__((__used__))

/** ## Global state for the test framework and assertions */

/** True while assertions are expected to fail. */
static bool testcase_assert_expected_to_fail = false;
/** True when verbose assertion logging is enabled. */
static bool testcase_assert_verbose_assertions = true;

/**
 * Current function for verbose assertion output.
 *
 * Used to avoid repeating the function name in logs for multiple assertions
 * in the same function.
 */
static const char *testcase_assert_current_func = NULL;
/** Current indentation level for verbose assertion output. */
static int testcase_assert_verbose_indent_level = 0;
/** Failed checks since the last call to EXPECTED_TO_FAIL_BEGIN(). */
static int testcase_assert_expected_failures = 0;
/** Failed checks within EXPECTED_TO_FAIL_BEGIN()/END() for this test case. */
static int testcase_assert_expected_failures_total = 0;
/** Successful assertions in this test case. */
static int testcase_assert_successful_assert_total = 0;
#define assert_failed_str "Assertion failed: "

/** ## Assertion macros and helpers */

/** Check a condition and log the result with context. */
#define CHECK(condition, fmt, ...)                                    \
        testcase_assert(condition, __FILE__, __LINE__, __func__, fmt, \
                        ##__VA_ARGS__)

/** If condition is false, report a test assertion failure. */
#define ASSERT(x) \
        testcase_assert(x, __FILE__, __LINE__, __func__, assert_failed_str #x)

/** If condition is true, report a Xen-style bug. */
#define BUG_ON(x) \
        testcase_assert(!(x), __FILE__, __LINE__, __func__, "BUG_ON: " #x)

/** Assert that execution cannot reach this point. */
#define ASSERT_UNREACHABLE() assert(false)

/** ## Helpers for expected assertion failures */

/** Mark the start of a block where assertions are expected to fail. */
#define EXPECTED_TO_FAIL_BEGIN() (testcase_assert_expected_to_fail = true)
/** Mark the end of a block where assertions are expected to fail. */
#define EXPECTED_TO_FAIL_END(c) testcase_assert_check_expected_failures(c)

/** Check the number of expected failures against the actual count. */
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

/** ## Test case management and reporting */

/** Function pointer used to initialize a test case. */
static void (*testcase_init_func)(const char *, int);

/** Set the function pointer used to initialize a test case. */
static void __used setup_testcase_init_func(void (*init_fn)(const char *, int))
{
    testcase_init_func = init_fn;
}

/**
 * Assert a condition within a test case.
 *
 * This is the core assertion helper used by the test framework. It checks a
 * condition and handles both expected and unexpected failures.
 *
 * If failure is expected, log it and increment the expected-failure count.
 * If failure is not expected, log it and abort the test. If the assertion
 * passes, increment the success count and optionally log the successful
 * assertion when verbose mode is enabled.
 *
 * Args:
 *  condition (bool):    Condition to check. If false, the assertion fails.
 *  file (const char *): File containing the assertion, used for logging.
 *  line (int):          Source line of the assertion, used for logging.
 *  func (const char *): Function containing the assertion.
 *  fmt (const char *):  printf-style format string with assertion context.
 *  ...:                 Additional arguments for the format string.
 */
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
        /* The assertion passed, so drop the failure prefix. */
        if ( strncmp(fmt, assert_failed_str, strlen(assert_failed_str)) == 0 )
            fmt += strlen(assert_failed_str);

        if ( strcmp(fmt, "ret == 0") == 0 )
            goto out;

        for ( int i = 0; i < testcase_assert_verbose_indent_level; i++ )
            printf("  ");

        printf("%s:%d: ", relpath, line);

        /*
         * Skip the function name if it was already logged for the current
         * function, or if the source path or function name starts with test.
         */
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

/** Representation of a test case and its reporting state. */
struct testcase {
    /** Human-readable test case name. */
    const char *name;
    /** Test ID. */
    const char *tid;
    /** Integer argument passed to the test case. */
    int         intarg;
    /** Test case function. */
    void        (*func)(int);
    /** Number of assertions that passed. */
    int         passed_asserts;
    /** Number of expected failures observed. */
    int         expected_failures;
} testcases[40];
/** Pointer to the current test case, used while collecting results. */
struct testcase *current_testcase = testcases;

static void print_testcase_report(struct testcase *tc)
{
    printf("- %-5s %-34s %2d: %3d assertions passed", tc->tid, tc->name,
           tc->intarg, tc->passed_asserts);
    if ( tc->expected_failures )
        printf(" (%2d XFAIL)", tc->expected_failures);
    printf("\n");
}

/**
 * Execute a test function and record results for the test report.
 *
 * The test function is expected to use CHECK, ASSERT, and BUG_ON for
 * assertions, and may use EXPECTED_TO_FAIL_BEGIN and EXPECTED_TO_FAIL_END
 * for negative test scenarios.
 *
 * The integer argument can be used to select scenarios or parameters.
 *
 * The report includes the test case name, integer argument, passed
 * assertions, and expected failures. A per-test report is printed after the
 * function returns, followed by a summary after the full suite completes.
 *
 * Args:
 *   case_func (void (*)(int)):
 *                  Test function to execute. It takes one integer argument.
 *   int_arg (int): Argument passed to the test function.
 *   tid (const char *):
 *                  String identifier for the test case.
 *   case_name (const char *):
 *                  Human-readable test case name used for reporting.
 */
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

    /*
     * Call the test case initialization hook if one is registered. Tests can
     * use it to reset global state or prepare scenario-specific setup.
     *
     * For example, the page allocator tests use it to reset the synthetic page
     * structures and heap before each test case.
     */
    if ( testcase_init_func && int_arg >= 0 )
        testcase_init_func(case_name, int_arg);

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

/** Print a summary of all executed test cases and their results. */
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

/** Parse command-line arguments for the test suite and log the setup. */
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

#endif /* TOOLS_TESTS_ALLOC_CHECK_ASSERTS_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
