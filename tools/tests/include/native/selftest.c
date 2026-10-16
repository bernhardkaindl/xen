/* SPDX-License-Identifier: GPL-2.0-only */
/* Selftests for the native test environment. */

#define TEST_USES_LIBXENCTRL_DOMAIN_API
#include <native/init.h>

static void test_equal_expressions(int start_mfn)
{
    unsigned long values[3];
    unsigned long *last = &values[2];

    EQ(last, values + 2);
    EQ(values + 2, last);
}

static void test_hypercall_bounce(int start_mfn)
{
    for ( unsigned int direction = XC_HYPERCALL_BUFFER_BOUNCE_IN;
          direction <= XC_HYPERCALL_BUFFER_BOUNCE_BOTH; ++direction )
    {
        unsigned char data[8], expected[8];
        unsigned char *buffer = data;
        DECLARE_HYPERCALL_BOUNCE(buffer, sizeof(data), direction);
        xc_hypercall_buffer_t *bounce = HYPERCALL_BUFFER(buffer);

        memset(data, 0xa5, sizeof(data));
        memcpy(expected, data, sizeof(data));
        ENSURE(!xc_hypercall_bounce_pre(xch, buffer) &&
               bounce->hbuf && bounce->hbuf != data,
               "Bounce direction %u: no separate buffer", direction);

        if ( direction & XC_HYPERCALL_BUFFER_BOUNCE_IN )
            ENSURE(!memcmp(bounce->hbuf, data, sizeof(data)),
                   "Bounce direction %u: input not copied", direction);
        else
        {
            unsigned char zero[sizeof(data)] = {};

            ENSURE(!memcmp(bounce->hbuf, zero, sizeof(data)),
                   "Bounce direction %u: output buffer not zeroed", direction);
        }

        memset(bounce->hbuf, 0x5a, sizeof(data));
        HYPERCALL_BOUNCE_SET_SIZE(buffer, 3);

        if ( direction & XC_HYPERCALL_BUFFER_BOUNCE_OUT )
            memset(expected, 0x5a, 3);

        xc_hypercall_bounce_post(xch, buffer);
        ENSURE(!memcmp(data, expected, sizeof(data)),
               "Bounce direction %u: output or untouched tail differs",
               direction);
    }
}


int main(void)
{
    run_test(test_equal_expressions, 0);
    run_test(test_hypercall_bounce, 0);
    return test_complete();
}