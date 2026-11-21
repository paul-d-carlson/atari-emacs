/*
 * test_termio.c
 *
 * src/termio.c unit tests: ttopen()/ttclose()/ttflush() (no-ops),
 * ttputc(), ttgetc(), ttgetc_noshow(), and typahead().
 *
 * ttputc() is verified the same way test_display.c/test_display2.c
 * verify rendered text: write a character through it at a known
 * on-screen position, then compare the raw screen byte against a
 * reference byte rendered on a scratch row (row 10, never touched by
 * any other test in this suite) via the resident's own cputc() -- this
 * sidesteps having to know cc65's ASCII -> Atari-screen-code mapping
 * directly (see test_display.c's row_matches_string() for the fuller
 * rationale of the same trick).
 *
 * ttgetc()/ttgetc_noshow()/typahead() actually read the Atari OS
 * keyboard queue, so (like test_search.c's readpattern()/forwsearch()
 * tests) they need real scripted keystrokes from the driver (see
 * driver/test_runner_resident.c's TERMIO_GATE_TEST). Both keystroke
 * tests share ONE keystroke segment (no disk I/O happens between them).
 *
 * Ordering requirement: test_typahead_reports_false_when_no_key_pending
 * MUST run before the gate opens (i.e. before any other test in this
 * suite that could have a keystroke pending) -- it asserts kbhit() is
 * genuinely empty, which is only guaranteed while the driver's
 * keystroke segment for this suite hasn't started streaming yet. It is
 * therefore placed FIRST in termio_tests[] below, and the driver's gate
 * is set to the global index of test_ttgetc_returns_injected_keycode
 * (this suite's first keystroke-consuming test), not this one.
 */

#include <atari.h>
#include <conio.h>

#include "resident_api.h"
#include "harness.h"
#include "test_termio.h"

/* Row helper: OS.savmsc points at the start of the 40x24 GR.0 screen
 * (see test_atari.c); each row is term.t_ncol bytes wide. */
static unsigned char *screen_row(unsigned char row)
{
    return OS.savmsc + ((unsigned int)row * (unsigned int)term.t_ncol);
}

/* ------------------------------------------------------------------- */
/* typahead(): reports whether a keystroke is queued, without          */
/* consuming it.                                                       */
/* ------------------------------------------------------------------- */

static unsigned char test_typahead_reports_false_when_no_key_pending(void)
{
    /* Must run before the driver's keystroke segment for this suite
     * starts streaming (see file header) -- no key genuinely queued. */
    ASSERT(typahead() == FALSE);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ttopen()/ttclose()/ttflush(): no-ops on this port.                   */
/* ------------------------------------------------------------------- */

static unsigned char test_ttopen_ttclose_ttflush_are_noops(void)
{
    ttopen();
    ttflush();
    ttclose();

    /* Nothing to assert against -- just exercising the lines for
     * coverage. Re-open afterward since main() only calls ttopen()
     * once at boot and later screen writes in this suite still need a
     * "logically open" terminal (a no-op here, but keeps the sequence
     * sane if that ever changes). */
    ttopen();

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ttputc(): writes one character via cc65 conio at the current cursor */
/* position.                                                            */
/* ------------------------------------------------------------------- */

static unsigned char test_ttputc_writes_character_at_cursor_position(void)
{
    unsigned char actual, expected;

    movecursor(6, 4);
    ttputc('Q');
    actual = screen_row(6)[4];

    /* Render the same character on the untouched scratch row for
     * comparison, since cc65 conio's screen codes aren't plain ASCII. */
    movecursor(10, 0);
    cputc('Q');
    expected = screen_row(10)[0];

    ASSERT(actual == expected);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ttgetc() / ttgetc_noshow(): block until a real keystroke arrives.   */
/* Both share ONE keystroke segment (see file header + driver's        */
/* TERMIO_GATE_TEST).                                                   */
/* ------------------------------------------------------------------- */

static unsigned char test_ttgetc_returns_injected_keycode(void)
{
    int c;

    /* Wait for the driver's scripted keystroke to actually land in the
     * OS queue before reading it -- proves typahead() can report TRUE,
     * not just FALSE (see the other typahead test above). */
    while (!typahead())
        ;
    ASSERT(typahead() == TRUE);

    c = ttgetc();
    ASSERT(c == (int)(unsigned char)'K');
    ASSERT(typahead() == FALSE);

    return TRUE;
}

static unsigned char test_ttgetc_noshow_returns_injected_keycode(void)
{
    int c;

    while (!typahead())
        ;

    c = ttgetc_noshow();
    ASSERT(c == (int)(unsigned char)'j');
    ASSERT(typahead() == FALSE);

    return TRUE;
}

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn termio_tests[NUM_TERMIO_TESTS] = {
    test_typahead_reports_false_when_no_key_pending,
    test_ttopen_ttclose_ttflush_are_noops,
    test_ttputc_writes_character_at_cursor_position,
    test_ttgetc_returns_injected_keycode,
    test_ttgetc_noshow_returns_injected_keycode,
};
#pragma rodata-name (pop)
