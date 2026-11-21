/*
 * harness.c
 *
 * Defines the fixed-address result block and the two small helper
 * functions every test suite uses to report progress/results into it.
 * See tests/common/test_protocol.h for the wire format read back by the
 * native driver (tests/driver/test_runner_resident.c).
 */
#include "harness.h"

test_results_t * const results = (test_results_t *) RESULTS_ADDR;

void test_begin(unsigned char idx)
{
    results->cur_test = idx;
}

void test_record(unsigned char idx, unsigned char passed)
{
    results->results[idx] = passed ? TEST_PASS : TEST_FAIL;
    results->total++;
    if (!passed)
        results->failed++;
}
