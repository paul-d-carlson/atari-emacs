/*
 * test_window.c
 *
 * src/window.c unit tests: reposition() and refresh(). In this Atari
 * port window.c has been trimmed to just these two single-window
 * command functions (no split-window/multi-window management exists
 * here) -- both are plain flag-setters on curwp/sgarbf with a single
 * `f == FALSE` branch each, no loops or pointers, so a handful of
 * direct-call tests give full line coverage.
 *
 * SELF-CONTAINED: every test sets its own known sentinel values into
 * curwp->w_force/w_flag and sgarbf immediately before calling, so
 * assertions never depend on leftover state from an earlier test or
 * suite. curwp itself is the one permanent window onto the "main"
 * buffer (see test_main_resident.c's reset_buffers()) -- window.c
 * never allocates/frees a WINDOW, so there's nothing to build here the
 * way other suites bfind() their own scratch BUFFERs.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_window.h"

/* ------------------------------------------------------------------- */
/* reposition(): sets curwp->w_force from n (or 0 if f==FALSE) and     */
/* always sets WFFORCE.                                                */
/* ------------------------------------------------------------------- */

static unsigned char test_reposition_with_f_false_forces_center(void)
{
    curwp->w_force = 42;
    curwp->w_flag = 0;

    ASSERT(reposition(FALSE, 7) == TRUE);
    ASSERT(curwp->w_force == 0);
    ASSERT((curwp->w_flag & WFFORCE) != 0);

    return TRUE;
}

static unsigned char test_reposition_with_positive_n_sets_force(void)
{
    curwp->w_force = 0;
    curwp->w_flag = 0;

    ASSERT(reposition(TRUE, 5) == TRUE);
    ASSERT(curwp->w_force == 5);
    ASSERT((curwp->w_flag & WFFORCE) != 0);

    return TRUE;
}

static unsigned char test_reposition_with_negative_n_sets_force(void)
{
    curwp->w_force = 0;
    curwp->w_flag = 0;

    ASSERT(reposition(TRUE, -3) == TRUE);
    /* w_force is a plain `char`, unsigned by cc65's default -- cast back
     * to signed to compare against the negative literal actually passed
     * in, rather than its raw truncated bit pattern. */
    ASSERT((signed char)curwp->w_force == -3);
    ASSERT((curwp->w_flag & WFFORCE) != 0);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* refresh(): f==FALSE sets sgarbf (full repaint); f==TRUE centers dot */
/* (same as reposition(_, 0)) without touching sgarbf.                 */
/* ------------------------------------------------------------------- */

static unsigned char test_refresh_with_f_false_sets_sgarbf(void)
{
    sgarbf = FALSE;
    curwp->w_force = 99;
    curwp->w_flag = 0;

    ASSERT(refresh(FALSE, 0) == TRUE);
    ASSERT(sgarbf == TRUE);
    /* f==FALSE branch never touches w_force/w_flag */
    ASSERT(curwp->w_force == 99);
    ASSERT(curwp->w_flag == 0);

    return TRUE;
}

static unsigned char test_refresh_with_f_true_centers_dot(void)
{
    sgarbf = FALSE;
    curwp->w_force = 99;
    curwp->w_flag = 0;

    ASSERT(refresh(TRUE, 123) == TRUE);
    ASSERT(curwp->w_force == 0);
    ASSERT((curwp->w_flag & WFFORCE) != 0);
    /* f==TRUE branch never touches sgarbf */
    ASSERT(sgarbf == FALSE);

    return TRUE;
}

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn window_tests[NUM_WINDOW_TESTS] = {
    test_reposition_with_f_false_forces_center,
    test_reposition_with_positive_n_sets_force,
    test_reposition_with_negative_n_sets_force,
    test_refresh_with_f_false_sets_sgarbf,
    test_refresh_with_f_true_centers_dot,
};
#pragma rodata-name (pop)
