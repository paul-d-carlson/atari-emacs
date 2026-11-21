/*
 * harness.h
 *
 * Shared test-harness primitives used by every target-side test suite
 * (test_buffer.c, test_atari.c, ...). Kept separate from any one suite so
 * new suites can be added without duplicating this scaffolding.
 */
#ifndef HARNESS_H
#define HARNESS_H

#include "../common/test_protocol.h"

typedef unsigned char (*test_fn)(void);

/* Fixed-address result block, shared by every suite and by main(). */
extern test_results_t * const results;

void test_begin(unsigned char idx);
void test_record(unsigned char idx, unsigned char passed);

#define ASSERT(cond) do { if (!(cond)) return FALSE; } while (0)

#endif /* HARNESS_H */
