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

#ifndef CONFIG_NUMA
#define CONFIG_NUMA 0
#endif
#define __used __attribute__((__used__))

/** ## Global state for the test framework and assertions */

/** Set when assertions are expected to fail */
static bool testcase_assert_expected_to_fail = false;
/** Set when verbose assertions are enabled */
static bool testcase_assert_verbose_assertions = true;
/**
 * The current function for verbose assertions, used to avoid repeating the
 * function name in the logs for multiple assertions within the same function.
 */
static const char *testcase_assert_current_func = NULL;
/** The current indentation level for verbose assertions */
static int testcase_assert_verbose_indent_level = 0;
/** Failed checks since the last call call to EXPECTED_TO_FAIL_BEGIN() */
static int testcase_assert_expected_failures = 0;
/** Failed checks within EXPECTED_TO_FAIL_BEGIN()/END() in this test case */
static int testcase_assert_expected_failures_total = 0;
/** Successful assertions in this test case */
static int testcase_assert_successful_assert_total = 0;
#define assert_failed_str "Assertion failed: "

/** ## Assertion macros and helpers */

/** Check a condition and log the result with context. */
#define CHECK(condition, fmt, ...)                                    \
        testcase_assert(condition, __FILE__, __LINE__, __func__, fmt, \
                        ##__VA_ARGS__)

/** If the condition is false, treat it as a test assertion failure */
#define ASSERT(x) \
        testcase_assert(x, __FILE__, __LINE__, __func__, assert_failed_str #x)

/** If the condition is true, treat it as a bug, used in Xen hypervisor code */
#define BUG_ON(x) \
        testcase_assert(!(x), __FILE__, __LINE__, __func__, "BUG_ON: " #x)

/** Assert that the code is unreachable */
#define ASSERT_UNREACHABLE() assert(false)

/** ## Helpers for expected assertion failures */

/** Marks the beginning of a block where assertions are expected to fail */
#define EXPECTED_TO_FAIL_BEGIN() (testcase_assert_expected_to_fail = true)
/** Marks the end of a block where assertions are expected to fail */
#define EXPECTED_TO_FAIL_END(c) testcase_assert_check_expected_failures(c)

/** Checks the number of expected failures against the actual count */
static void __used testcase_assert_check_expected_failures(int expected)
{
    if ( testcase_assert_expected_failures != expected )
    {
        fprintf(stderr, "Test assertion expected %d failures, but got %d\n",
                expected, testcase_assert_expected_failures);
        abort();
    }
    testcase_assert_expected_to_fail = false;
    testcase_assert_expected_failures = 0;
    testcase_assert_expected_failures_total += expected;
}

/** ## Test case management and reporting */

/** Function pointer used for initializing a test case */
static void (*testcase_init_func)(const char *, int);

/** Set up the function pointer for initializing a test case */
static void __used setup_testcase_init_func(void (*init_fn)(const char *, int))
{
    testcase_init_func = init_fn;
}

/**
 * Assert a condition within a test case
 *
 * This function is the core of the assertion mechanism for test cases.
 * It checks a given condition and handles both expected and unexpected
 * assertion failures.
 *
 * If the assertion is expected to fail, it logs the failure and increments
 * the expected failure count.  If the assertion is not expected to fail but
 * does, it logs the failure and aborts the test.  If the assertion passes,
 * it increments the successful assertion count and optionally logs the
 * successful assertion if verbose assertions are enabled.
 *
 * Args:
 *  condition (bool):    The condition to check.  If false, the assertion fails.
 *  file (const char *): The file where the assertion is located, for logging.
 *  line (int):          The source line of the assertion, for logging.
 *  func (const char *): The function name where the assertion is located.
 *  fmt (const char *):  A printf format string with context for the assertion.
 *  ...:                 Additional arguments for the format string.
 */
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
                condition ? "unexpectedly passed" : "expectedly failed",
                relpath, line);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fprintf(stderr, "\n");

        if ( condition )
            abort(); /* Unexpected pass, treat as test failure */
        else
            testcase_assert_expected_failures++; /* update for the report */
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
            printf("  ");

        printf("%s:%d: ", relpath, line);

        /*
         * Skip logging the passed function if it was already logged for the
         * current function, or the source or the function starts with test.
         */
        if ( (testcase_assert_current_func == NULL ||
              strcmp(testcase_assert_current_func, func)) &&
             (strncmp(relpath, "test-", strlen("test-")) &&
              strncmp(func, "test_", strlen("test_"))) )
            printf("%s(): ", func);

        if ( strncmp(fmt, "BUG_ON:", strlen("BUG_ON:")) )
            printf("ASSERT(");

        vprintf(fmt, ap);
        va_end(ap);

        if ( strncmp(fmt, "BUG_ON:", strlen("BUG_ON:")) )
            printf(")");

        printf("\n");
    }
}

/** Structure to represent a test case and its results for reporting */
struct testcase {
    /** Human-readable name of the test case */
    const char *name;
    /** Test ID */
    const char *tid;
    /** Integer argument for the test case */
    int         intarg;
    /** Function pointer to the test case function */
    void        (*func)(int);
    /** Number of assertions passed */
    int         passed_asserts;
    /** Number of expected failures occurred */
    int         expected_failures;
} testcases[40];
/** Pointer to the current test case being executed, for tracking results */
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
 * Execute the given test function and record the number of assertions
 * passed and expected failures for the test report.  The test function
 * is expected to use the CHECK, ASSERT, and BUG_ON macros for assertions,
 * and can use EXPECTED_TO_FAIL_BEGIN and EXPECTED_TO_FAIL_END to mark
 * assertions that are expected to fail for testing negative scenarios.
 *
 * The test function is also passed an integer argument that can be used
 * to specify different scenarios or parameters for the test.
 *
 * The test report will include the name of the test case, the integer
 * argument, the number of assertions passed, and the number of expected
 * failures.  The test report is printed after the test function completes,
 * and a summary report is printed after all test cases have been executed.
 *
 * The test function can also use the verbose assertion mode to print
 * additional context for each assertion, which can be helpful for debugging
 * test failures and understanding the test flow.
 *
 * Args:
 *   case_func (void (*)(int)):
 *                  The test function to execute, which takes an int argument.
 *   int_arg (int): An argument to pass to the test function, which can be used
 *                  to specify different scenarios or parameters for the test.
 *   tid (const char *):
 *                  A test id; string identifier for the test case.
 *   case_name (const char *):
 *                  A human-readable name for the test case, used for reporting.
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
     * Call the testcase initialization function if it is set, which can be
     * used to reset global state or set up specific scenarios for the test.
     *
     * For example, the page allocator tests use this to reset the state of the
     * synthetic page structures and the heap before each test case.
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

/**
 * Provide a report of all test cases executed and their results,
 * including the total number of assertions passed and expected failures.
 */
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

#endif /* _CHECK_ASSERTS_H_ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
