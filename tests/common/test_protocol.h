/*
 * test_protocol.h
 *
 * Shared result-reporting protocol between the cc65-compiled 6502
 * resident test binary (tests/target/test_main_resident.c, linked against
 * the REAL src/ application C code) and its native host driver
 * (tests/driver/test_runner_resident.c, linked against
 * libatari800 via the shared tests/driver/harness_runner.c engine).
 *
 * The test binary writes its progress/results into a fixed, low-memory
 * address (Atari "Page Six", $0600-$06FF -- conventional free RAM not used
 * by the OS, DOS, or cc65's runtime when the program is linked above
 * $1E00). The native driver peeks this same address directly out of the
 * emulated RAM via libatari800_get_main_memory_ptr().
 *
 * This header is included by BOTH cc65 (target) and gcc/clang (host)
 * builds, so keep it strictly C89 and free of platform-specific types.
 */
#ifndef TEST_PROTOCOL_H
#define TEST_PROTOCOL_H

#define RESULTS_ADDR   0x0600
#define RESULTS_MAGIC  0xA5

#define MAX_TESTS      192

/* results[i]: 0 = not run yet, 1 = pass, 2 = fail */
#define TEST_NOT_RUN   0
#define TEST_PASS      1
#define TEST_FAIL      2

typedef struct {
    unsigned char magic;             /* RESULTS_MAGIC once test_main starts */
    unsigned char done;              /* 1 once every test has run           */
    unsigned char total;             /* number of tests attempted so far    */
    unsigned char failed;            /* number of failures so far           */
    unsigned char cur_test;          /* index of test currently running     */
    unsigned char results[MAX_TESTS];
} test_results_t;

#endif /* TEST_PROTOCOL_H */
