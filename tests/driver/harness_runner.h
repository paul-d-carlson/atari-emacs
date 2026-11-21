/*
 * harness_runner.h
 *
 * Shared driver engine for the native (host-arch) test runner. The
 * resident test binary (see tests/target/test_main_resident.c) has
 * a tiny tests/driver/test_runner_resident.c that just declares its own
 * test_info[] table, keystroke script, and dbgcov targets, then hands
 * them to run_harness() below -- which owns the actual libatari800
 * frame-stepping loop, keystroke injection, CPU-crash/timeout
 * detection, results report, and line-coverage summary.
 */
#ifndef HARNESS_RUNNER_H
#define HARNESS_RUNNER_H

#include <stddef.h>

#include "../common/test_protocol.h"
#include "dbgcov.h"

/* One row of the human-readable name table each runner supplies, in the
 * exact same order as its target binary's dispatch loop. */
typedef struct {
    const char *name;     /* test function name (unique)                */
    const char *src_file; /* source file under test, e.g. "src/atari.c" */
    const char *src_func; /* function(s) under test, e.g. "atari_eeol"  */
} test_info_t;

/*
 * One segment of scripted keystrokes, fed to the emulated keyboard in
 * order. `gate_test` gates when this segment is allowed to start
 * streaming: -1 means "as soon as the previous segment is exhausted",
 * a non-negative value means "wait until results->cur_test reaches (or
 * passes) this index first".
 *
 * Gating matters whenever a stretch of non-interactive tests runs
 * between two interactive ones: the frame-based drip-feed below doesn't
 * know which test is currently running, so an ungated segment would get
 * pushed into the Atari OS's keyboard buffer during that non-interactive
 * stretch -- and display.c's update() (and similar real MicroEMACS
 * code) checks typahead() and silently no-ops if *anything* is pending,
 * even a keystroke meant for a much later test. Confirmed empirically
 * this exact symptom (buffer content correct per direct memory checks,
 * but the rendered screen untouched) before this gating was added.
 *
 * The segment array must be terminated by a {NULL, 0} sentinel.
 */
typedef struct {
    const char *chars;
    int gate_test;
} script_segment_t;

typedef struct {
    const char *default_atr_path;
    const char *default_dbg_path;
    const test_info_t *test_info;
    int num_tests;
    const script_segment_t *segments;   /* array, {NULL,0}-terminated   */
    dbgcov_target_t *dbgcov_targets;    /* mutable: dbgcov_analyze()
                                          * fills in the coverage counts */
    int num_dbgcov_targets;
    int max_frames;
} harness_config_t;

/*
 * Runs the emulated test binary described by `cfg` to completion (or
 * until cfg->max_frames elapses / a CPU crash is detected), then prints
 * the PASS/FAIL report and line-coverage summary.
 *
 * argv[1]/argv[2] override cfg->default_atr_path/default_dbg_path if
 * present (same CLI convention the original single-binary test_runner
 * used).
 *
 * Returns 0 if every test passed, 1 otherwise (matching a shell-style
 * exit code), 2 if libatari800 itself failed to initialize.
 */
int run_harness(int argc, char **argv, const harness_config_t *cfg);

#endif /* HARNESS_RUNNER_H */
