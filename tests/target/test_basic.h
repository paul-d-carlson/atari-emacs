#ifndef TEST_BASIC_H
#define TEST_BASIC_H

#include "harness.h"

#define NUM_BASIC_TESTS 20

/* Builds the "basicmove" test buffer (see test_basic.c's file comment
 * for its exact line layout) and switches curbp/curwp to it. Must be
 * called once, before dispatching any of basic_tests[]. */
void build_basic_test_buffer(void);

extern const test_fn basic_tests[NUM_BASIC_TESTS];

#endif /* TEST_BASIC_H */
