/*
 * test_atari.c
 *
 * Unit tests for the REAL src/atari.c terminal driver -- the Atari
 * 8-bit-specific glue between MicroEMACS's abstract TERM interface and
 * cc65's conio.h. atari.c's five worker functions (atari_move,
 * atari_eeol, atari_eeop, atari_beep, atari_rev) are `static`, so they
 * are only reachable the same way the rest of MicroEMACS reaches them:
 * through the real, linked `term` struct's function pointers. There is
 * no mocking of atari.c, conio.h, or the emulated hardware -- every
 * assertion below is checked against actual bytes read back out of the
 * emulated Atari's screen memory (via the documented `OS.savmsc` OS
 * shadow pointer from cc65's <atari.h>), or from cc65's own conio
 * cursor-position queries (wherex()/wherey()), which are backed by the
 * real Atari OS screen editor running under libatari800.
 *
 * atari_open (term.t_open) is not separately tested here: it is already
 * exercised once, for real, by vtinit() during harness startup (see
 * test_main.c), and calling it again mid-suite would reset terminal
 * state relied upon by other tests.
 *
 * Screen memory holds Atari *internal* (screen) character codes, not
 * ASCII -- cc65's conio driver does not expose a documented way to peek
 * a cell and get back an ASCII character on this target (no cpeekc.o /
 * cpeekrevers.o in atari.lib). Rather than hand-implement the ASCII<->
 * internal-code table (risk of getting an edge case wrong), the eeol/
 * eeop tests instead check for the well-documented, code-independent
 * facts that (a) writing text changes a cell's byte away from whatever
 * "blank" was there before, and (b) erasing puts it back to that same
 * blank value -- and the reverse-video test relies on the other
 * well-documented, code-independent Atari convention that bit 7 of a
 * screen byte is the reverse-video flag, common to every internal code.
 */

#include <atari.h>
#include <conio.h>

#include "resident_api.h"
#include "harness.h"
#include "test_atari.h"

static unsigned char test_term_dimensions(void)
{
    /* GR.0 text mode, 40x24, matching src/atari.c's NROW/NCOL/MARGIN/SCRSIZ. */
    ASSERT(term.t_nrow == 23);   /* last row index, 0-based -> 24 rows   */
    ASSERT(term.t_ncol == 40);
    ASSERT(term.t_margin == 4);
    ASSERT(term.t_scrsiz == 32);

    return TRUE;
}

static unsigned char test_move_positions_cursor(void)
{
    (*term.t_move)(5, 10);
    ASSERT(wherey() == 5);
    ASSERT(wherex() == 10);

    (*term.t_move)(2, 0);
    ASSERT(wherey() == 2);
    ASSERT(wherex() == 0);

    return TRUE;
}

static unsigned char test_eeol_clears_to_end_of_line(void)
{
    unsigned char *row = OS.savmsc + (2 * (unsigned int)term.t_ncol);
    unsigned char blank, x;

    blank = row[term.t_ncol - 1];    /* untouched cell -> the "blank" code */

    (*term.t_move)(2, 0);
    cputs("ABCDEFGHIJ");              /* writes cols 0-9 on row 2           */

    for (x = 0; x < 10; x++)
        ASSERT(row[x] != blank);      /* written cells differ from blank    */

    (*term.t_move)(2, 5);
    (*term.t_eeol)();                 /* clear cols 5..end of row 2         */

    for (x = 0; x < 5; x++)
        ASSERT(row[x] != blank);      /* untouched by the clear             */
    for (x = 5; x < term.t_ncol; x++)
        ASSERT(row[x] == blank);      /* erased back to blank               */

    return TRUE;
}

static unsigned char test_eeop_clears_rest_of_screen(void)
{
    unsigned char *row3 = OS.savmsc + (3 * (unsigned int)term.t_ncol);
    unsigned char *row4 = OS.savmsc + (4 * (unsigned int)term.t_ncol);
    unsigned char blank, x;

    blank = row3[term.t_ncol - 1];

    (*term.t_move)(3, 0);
    cputs("1111111111");
    (*term.t_move)(4, 0);
    cputs("2222222222");

    (*term.t_move)(3, 5);
    (*term.t_eeop)();                 /* clear from (row 3, col 5) onward   */

    for (x = 0; x < 5; x++)
        ASSERT(row3[x] != blank);     /* row 3 head survives                */
    for (x = 5; x < term.t_ncol; x++)
        ASSERT(row3[x] == blank);     /* row 3 tail erased                  */
    for (x = 0; x < term.t_ncol; x++)
        ASSERT(row4[x] == blank);     /* row 4 entirely erased              */

    return TRUE;
}

static unsigned char test_rev_toggles_reverse_video(void)
{
    unsigned char *row6 = OS.savmsc + (6 * (unsigned int)term.t_ncol);
    unsigned char normal, reversed;

    (*term.t_rev)(0);
    (*term.t_move)(6, 0);
    cputc('X');
    normal = row6[0];

    (*term.t_rev)(1);
    (*term.t_move)(6, 1);
    cputc('X');
    reversed = row6[1];

    (*term.t_rev)(0);                 /* leave reverse video off afterward  */

    ASSERT((normal & 0x80) == 0);
    ASSERT((reversed & 0x80) != 0);
    ASSERT((normal & 0x7F) == (reversed & 0x7F)); /* same base character    */

    return TRUE;
}

/* atari_beep() writes ATASCII 0x07 through the console device and
 * flushes; there is no portable way to observe "a beep occurred" from
 * C, so this is a smoke test only: it confirms the call completes
 * without hanging or crashing the emulated CPU. */
static unsigned char test_beep_does_not_crash(void)
{
    (*term.t_beep)();

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn atari_tests[NUM_ATARI_TESTS] = {
    test_term_dimensions,
    test_move_positions_cursor,
    test_eeol_clears_to_end_of_line,
    test_eeop_clears_rest_of_screen,
    test_rev_toggles_reverse_video,
    test_beep_does_not_crash,
};
#pragma rodata-name (pop)
