/*
 * stress_harness.h
 *
 * A CONTINUOUS-SESSION variant of int_harness.h's boot/frame-step engine,
 * for the long-running (operator-terminated) heap/stack/disk soak test
 * (see stress/stress_test.c). Deliberately a SEPARATE file, not a
 * refactor of int_harness.c: the two existing scenarios' timing is
 * carefully bisected and proven (see scenarios/scenario_file_operations.c's
 * header comment and /memories/repo/microemacs-debug.md), and this file
 * must never risk perturbing that.
 *
 * The key difference from int_harness_run(): that function boots,
 * runs ONE fixed keystroke script to completion, and shuts down.
 * stress_harness instead exposes boot() and feed() separately, so the
 * SAME libatari800 process/RAM state (heap, C stack, buffer list) can be
 * fed MANY successive keystroke batches over an arbitrarily long,
 * operator-terminated run -- required so heap fragmentation and stack
 * pressure can actually accumulate across cycles, rather than resetting
 * to a pristine state every time (which a fresh boot-per-cycle design,
 * like re-running int_harness_run() in a loop, would do).
 */
#ifndef STRESS_HARNESS_H
#define STRESS_HARNESS_H

#include "int_harness.h"   /* reuses int_key_segment_t */

typedef struct {
    unsigned char *mem;    /* emulated 64K RAM; valid until stress_shutdown() */
    int total_frames;      /* cumulative frames since stress_boot() */
    int crashed;
    int crash_frame;       /* cumulative frame count at crash, if crashed */
} stress_result_t;

/* Boots atr_path and runs boot_settle_frames frames of no input. Returns
 * 0 and fills out->mem/total_frames on success, -1 if libatari800 itself
 * failed to initialize. */
int stress_boot(const char *atr_path, int boot_settle_frames, stress_result_t *out);

/* Feeds one bounded batch of segments (same hold/idle timing model as
 * int_harness_run) into the ALREADY-BOOTED session, stopping either
 * when the batch is exhausted (+ post_script_settle_frames) or after
 * max_extra_frames, whichever comes first. Updates io->total_frames
 * (cumulative across all feed() calls since boot) and io->crashed/
 * crash_frame if a crash occurs during this call. Returns 0 normally
 * (check io->crashed), -1 only if called without a successful boot.
 */
int stress_feed(const int_key_segment_t *segments, int key_hold_frames,
                 int key_idle_frames, int post_script_settle_frames,
                 int max_extra_frames, stress_result_t *io);

void stress_shutdown(void);

#endif /* STRESS_HARNESS_H */
