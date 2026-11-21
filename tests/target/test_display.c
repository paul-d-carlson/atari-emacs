/*
 * test_display.c
 *
 * First of two src/display.c test files (see also test_display2.c).
 * Covers update()/updateline()'s ordinary (non-extended-line,
 * non-fast-path) rendering, modeline(), mlwrite()/mlputs()/mlputi(),
 * and movecursor(). No source file under src/ is modified or mocked:
 * every assertion below reads back real bytes from the emulated
 * Atari's screen memory (via cc65's documented OS.savmsc shadow
 * pointer, the same technique test_atari.c uses), after driving the
 * real display.c code through a real WINDOW/BUFFER built with real
 * line.c primitives (linsert/ldelete).
 *
 * Screen memory holds Atari *internal* character codes, not ASCII (see
 * test_atari.c's file comment for why) -- so, like test_atari.c, these
 * tests check code-independent facts instead of hardcoded expected byte
 * values: a written cell differs from an untouched/erased "blank"
 * reference cell, and the count of non-blank cells in a row matches the
 * expected string length.
 *
 * SPLIT HISTORY: this file originally held all 21 display.c tests
 * (6201 bytes, the single largest overlay at the time). It was split
 * into this file (13 non-interactive tests) plus test_display2.c (the
 * remaining 8, including the interactive mlyesno()/mlreplyt() ones)
 * when src/search.c's suite added ~1.3KB of new *resident* CODE and
 * needed more OVERLAY_SIZE headroom than the largest overlay
 * (test_random2.c, 6677 bytes) allowed to be reclaimed -- see
 * test_display2.c's header and abi_table.h's OVERLAY_SIZE comment for
 * the full story. Splitting lowers the shared overlay window's floor
 * without touching any real application logic.
 */

#include <atari.h>
#include <string.h>
#include <conio.h>

#include "resident_api.h"
#include "harness.h"
#include "test_display.h"

/* Row helper: OS.savmsc points at the start of the 40x24 GR.0 screen
 * (see test_atari.c); each row is term.t_ncol bytes wide. */
static unsigned char *screen_row(unsigned char row)
{
    return OS.savmsc + ((unsigned int)row * (unsigned int)term.t_ncol);
}

/* Counts the contiguous run of non-blank cells starting at column 0 of
 * the given row (comparing against `blank`, an already-erased cell's
 * byte value). Matches the fact that every string tested here is
 * written starting at column 0 with no embedded blanks. Used where the
 * exact rendered content doesn't matter, only its length. */
static unsigned char count_prefix(unsigned char *row, unsigned char blank)
{
    unsigned char n = 0;
    while (n < term.t_ncol && row[n] != blank)
        n++;
    return n;
}

/* Compares the first strlen(s) columns of `row` against what cc65's own
 * cputs() renders for the same ASCII text on a scratch reference row
 * (row 10 -- never touched by any other test in this suite). This
 * validates exact rendered content without needing to reason about
 * Atari-internal screen character codes, matching test_atari.c's
 * documented rationale for not hardcoding an ASCII<->internal charmap.
 * Pass `rev` TRUE when comparing against modeline() output, which
 * display.c always renders in reverse video (see updateline()'s VFREQ
 * handling in src/display.c). */
static unsigned char row_matches_string(unsigned char *row, const char *s, unsigned char rev)
{
    unsigned char *ref = screen_row(10);
    unsigned char len = (unsigned char)strlen(s);
    unsigned char i;

    (*term.t_move)(10, 0);
    (*term.t_rev)(rev);
    cputs(s);
    (*term.t_rev)(FALSE);

    for (i = 0; i < len; i++)
        if (row[i] != ref[i])
            return FALSE;
    return TRUE;
}

static void insert_text(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n')
            lnewline();
        else
            linsert(1, (unsigned char)*s);
    }
}

/* ------------------------------------------------------------------- */

/* update() renders the current window's buffer lines into vbuf, via
 * vtmove()/vtputc()/vteeol(), then updateline() paints the diff to the
 * real physical screen. This test builds a fresh two-line buffer, forces
 * a full redraw (WFHARD), and confirms both lines land on the right
 * physical rows with the right length. */
static unsigned char test_update_renders_buffer_line(void)
{
    BUFFER *bp = bfind("displaytest", TRUE, 0);
    unsigned char *row0, *row1;
    unsigned char blank;

    swbuffer(bp);
    insert_text("HELLO\nWORLD");
    gotobob(FALSE, 0);

    curwp->w_flag |= WFHARD;
    update();

    row0 = screen_row(0);
    row1 = screen_row(1);
    blank = row0[39];   /* untouched column -> erased "blank" byte */

    ASSERT(count_prefix(row0, blank) == 5);   /* "HELLO" */
    ASSERT(count_prefix(row1, blank) == 5);   /* "WORLD" */

    return TRUE;
}

/* Shrinking a line's real content (via the real ldelete()) and forcing
 * a redraw must erase the now-stale tail back to blank on the physical
 * screen -- exercising updateline()'s pbuf/vbuf diff + erase path, not
 * just vtputc()'s virtual-buffer write path. Also the first exercise of
 * update()'s single-line WFEDIT fast-path branch. */
static unsigned char test_update_erases_shrunk_line_tail(void)
{
    unsigned char *row0 = screen_row(0);
    unsigned char blank = row0[39];

    /* Buffer still holds "HELLO\nWORLD" from the previous test; dot to
     * the start of line 1 ("HELLO"), keep "HE", delete "LLO". */
    gotobob(FALSE, 0);
    forwchar(FALSE, 2);
    ldelete(3, FALSE);

    curwp->w_flag |= WFEDIT;
    update();

    ASSERT(count_prefix(row0, blank) == 2);   /* "HE" survives      */
    ASSERT(row0[2] == blank);                 /* "LLO" erased       */
    ASSERT(row0[4] == blank);

    return TRUE;
}

/* modeline() renders "ME: <bufname>[*] <filename>" -- with no filename
 * and a clean buffer, the non-blank prefix should be exactly
 * "ME: " + bufname, no trailing '*'. */
static unsigned char test_modeline_shows_bufname_no_star_when_clean(void)
{
    BUFFER *bp = bfind("MODE1", TRUE, 0);
    unsigned char *row;

    swbuffer(bp);
    bp->b_flag &= ~BFCHG;
    curwp->w_flag |= WFMODE | WFHARD;
    update();

    row = screen_row(term.t_nrow - 1);

    ASSERT(row_matches_string(row, "ME: MODE1 ", TRUE));   /* no '*' */

    return TRUE;
}

/* Same buffer, but marked BFCHG (changed-since-write): modeline() must
 * append one extra '*' character before the trailing padding. */
static unsigned char test_modeline_shows_star_when_buffer_changed(void)
{
    BUFFER *bp = bfind("MODE1", TRUE, 0);
    unsigned char *row;

    swbuffer(bp);
    bp->b_flag |= BFCHG;
    curwp->w_flag |= WFMODE | WFHARD;
    update();

    row = screen_row(term.t_nrow - 1);

    ASSERT(row_matches_string(row, "ME: MODE1* ", TRUE));

    bp->b_flag &= ~BFCHG;   /* leave tidy for any later test on this buffer */

    return TRUE;
}

/* mlwrite() writes straight to the message line (row term.t_nrow, one
 * row below anything update() manages) and always finishes with a real
 * t_eeol() call, so everything after the message is guaranteed erased.
 * The "%d" format goes through mlputi(). */
static unsigned char test_mlwrite_renders_message_with_mlputi(void)
{
    unsigned char *row;

    mlwrite("Count: %d", 42);

    row = screen_row(term.t_nrow);

    ASSERT(row_matches_string(row, "Count: 42", FALSE));
    ASSERT(mpresf == TRUE);

    return TRUE;
}

/* mlputs() appends raw text at the current hardware cursor position
 * without erasing anything -- unlike mlwrite(), it does not call
 * t_eeol() first, so it can extend an existing message. */
static unsigned char test_mlputs_appends_without_erasing(void)
{
    unsigned char *row;

    /* Use movecursor() (not mlwrite()) to get to a known starting
     * position: cc65's cclearxy(), which mlwrite() calls via t_eeol()
     * to blank the rest of the line, does not preserve the cursor
     * position on this target (it ends up wrapped to (0,0) after
     * clearing all the way to the last column). That's a cc65/atari
     * conio quirk orthogonal to what's being tested here -- mlputs()'s
     * own "append at the current position, don't erase" behavior. */
    movecursor(term.t_nrow, 0);
    mlputs("AB");
    mlputs("CDE");

    row = screen_row(term.t_nrow);

    ASSERT(row_matches_string(row, "ABCDE", FALSE));

    return TRUE;
}

/* movecursor() is display.c's own de-duplicating wrapper around
 * term.t_move -- test_atari.c's move test calls (*term.t_move)()
 * directly, bypassing this wrapper entirely, so it is not otherwise
 * covered. */
static unsigned char test_movecursor_positions_hardware_cursor(void)
{
    movecursor(7, 12);
    ASSERT(wherey() == 7);
    ASSERT(wherex() == 12);

    movecursor(3, 1);
    ASSERT(wherey() == 3);
    ASSERT(wherex() == 1);

    return TRUE;
}

/* mlerase() clears the message line and resets mpresf. */
static unsigned char test_mlerase_clears_message_line(void)
{
    unsigned char *row;
    unsigned char blank;

    mlwrite("temporary message");
    row = screen_row(term.t_nrow);
    blank = row[39];
    ASSERT(count_prefix(row, blank) > 0);

    mlerase();

    ASSERT(count_prefix(row, blank) == 0);
    ASSERT(mpresf == FALSE);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* The tests below were added to push src/display.c's real line coverage
 * from 67.7% towards 85%+ -- each targets a specific branch that
 * dbgcov's per-line report (tests/driver/dbgcov.c's verbose mode) showed
 * was never reached by the tests above. (More such tests continue in
 * test_display2.c.) */

/*
 * vtputc() has three branches never hit by a short, plain-ASCII line:
 * tab expansion, control-character caret-echo, and the end-of-line
 * overflow marker ('$') for a line rendered longer than the screen is
 * wide. Build such a line, but park dot on a different (short) line so
 * this one renders through update()'s ordinary (non-extended) path,
 * where vtputc() alone -- not vtpute() -- does the work.
 */
static unsigned char test_vtputc_handles_tab_ctrlchar_and_overflow(void)
{
    BUFFER *bp = bfind("vtputctest", TRUE, 0);
    unsigned char *row0;
    unsigned char i;

    swbuffer(bp);
    linsert(1, '\t');
    linsert(1, 0x01);
    for (i = 0; i < 50; i++)
        linsert(1, 'X');
    lnewline();
    insert_text("Y");              /* short second line; dot ends up here */

    gotobob(FALSE, 0);
    curwp->w_dotp = lforw(curwp->w_dotp);   /* onto the short "Y" line */
    curwp->w_doto = 0;

    curwp->w_flag |= WFHARD;
    update();

    row0 = screen_row(0);
    ASSERT(row_matches_string(row0 + 39, "$", FALSE));

    return TRUE;
}

/*
 * A line becomes "extended" once the cursor's column reaches
 * term.t_ncol-1; updext() then takes over. This pins the cursor exactly
 * at that boundary (curcol == t_ncol-1), the special-cased branch
 * documented by the comment above updext()'s rcursor computation
 * (curcol - t_ncol would underflow in unsigned arithmetic at the exact
 * boundary). updext() always stamps column 0 with '$' once scrolled.
 */
static unsigned char test_update_extends_line_at_boundary(void)
{
    BUFFER *bp = bfind("extend1", TRUE, 0);
    unsigned char i;

    swbuffer(bp);
    for (i = 0; i < 60; i++)
        linsert(1, 'A');

    gotobob(FALSE, 0);
    curwp->w_doto = term.t_ncol - 1;   /* curcol will equal 39 exactly */

    curwp->w_flag |= WFHARD;
    update();

    ASSERT(row_matches_string(screen_row(0), "$", FALSE));

    return TRUE;
}

/*
 * Same as above but with the cursor far enough right to land in
 * updext()'s *other* branch (curcol >= t_ncol, real scroll-offset
 * modulo math instead of the boundary special case), and with a tab
 * and control character earlier in the line so vtpute() -- updext()'s
 * own character renderer, a virtual twin of vtputc() -- exercises its
 * tab-expansion and caret-echo branches too.
 */
static unsigned char test_update_extends_line_with_far_scroll_and_ctrlchars(void)
{
    BUFFER *bp = bfind("extend2", TRUE, 0);
    unsigned char i;

    swbuffer(bp);
    linsert(1, '\t');
    linsert(1, 0x01);
    for (i = 0; i < 70; i++)
        linsert(1, 'B');

    gotobob(FALSE, 0);
    curwp->w_doto = 50;   /* past the tab+ctrl prefix -> curcol >= t_ncol */

    curwp->w_flag |= WFHARD;
    update();

    ASSERT(row_matches_string(screen_row(0), "$", FALSE));

    return TRUE;
}

/*
 * update()'s "is dot still inside the current window?" quick scan walks
 * forward from w_linep looking for w_dotp. Every earlier test in this
 * file puts dot on the window's very first line, so that scan always
 * matches on its first iteration -- this test puts dot a few lines down
 * (but still comfortably inside the window) so the scan must actually
 * walk forward before finding it.
 */
static unsigned char test_update_scans_forward_to_find_dot_in_view(void)
{
    unsigned char *row3;
    unsigned char blank;
    BUFFER *bp = bfind("scanfind", TRUE, 0);

    swbuffer(bp);
    insert_text("L0\nL1\nL2\nL3\nL4");

    gotobob(FALSE, 0);
    curwp->w_linep = curwp->w_dotp;     /* window top = first line       */
    forwline(FALSE, 3);                 /* dot -> "L3", still in view    */

    curwp->w_flag = WFHARD;             /* WFFORCE unset: quick scan runs */
    update();

    row3 = screen_row(3);
    blank = row3[39];
    ASSERT(count_prefix(row3, blank) == 2);   /* "L3" */

    return TRUE;
}

/*
 * When dot scrolls out of the window entirely, the quick scan above
 * exhausts without finding it, and update() recomputes a new top-of-
 * window line from w_force: zero means "center dot in the window",
 * positive/negative mean "put dot N lines from the top/bottom", each
 * clamped to the window height. This drives all three cases (plus both
 * clamps) directly against a buffer taller than the screen, confirming
 * dot's line is genuinely visible afterward each time.
 */
static unsigned char test_update_reframes_with_various_force_values(void)
{
    BUFFER *bp = bfind("reframe", TRUE, 0);
    unsigned char i;
    LINE *lp;

    swbuffer(bp);
    for (i = 0; i < 30; i++) {
        linsert(1, (int)('A' + (i % 26)));
        if (i != 29)
            lnewline();
    }

    /* Case 1: w_force == 0 -- the default "center it" branch. */
    gotobob(FALSE, 0);
    curwp->w_linep = curwp->w_dotp;
    gotoline(FALSE, 26);                /* dot far below the window */
    curwp->w_force = 0;
    curwp->w_flag  = WFHARD;
    update();
    lp = curwp->w_linep;
    for (i = 0; i < term.t_nrow - 1 && lp != curwp->w_dotp; i++)
        lp = lforw(lp);
    ASSERT(lp == curwp->w_dotp);

    /* Case 2: w_force > 0, large enough to hit the upper clamp. */
    gotobob(FALSE, 0);                  /* dot out of view again    */
    curwp->w_force = 30;
    curwp->w_flag  = WFHARD;
    update();
    lp = curwp->w_linep;
    for (i = 0; i < term.t_nrow - 1 && lp != curwp->w_dotp; i++)
        lp = lforw(lp);
    ASSERT(lp == curwp->w_dotp);

    /* Case 3: w_force < 0, large enough to hit the lower clamp. */
    gotoline(FALSE, 26);                /* dot out of view again    */
    curwp->w_force = -30;
    curwp->w_flag  = WFHARD;
    update();
    lp = curwp->w_linep;
    for (i = 0; i < term.t_nrow - 1 && lp != curwp->w_dotp; i++)
        lp = lforw(lp);
    ASSERT(lp == curwp->w_dotp);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn display_tests[NUM_DISPLAY_TESTS] = {
    test_update_renders_buffer_line,
    test_update_erases_shrunk_line_tail,
    test_modeline_shows_bufname_no_star_when_clean,
    test_modeline_shows_star_when_buffer_changed,
    test_mlwrite_renders_message_with_mlputi,
    test_mlputs_appends_without_erasing,
    test_movecursor_positions_hardware_cursor,
    test_mlerase_clears_message_line,
    test_vtputc_handles_tab_ctrlchar_and_overflow,
    test_update_extends_line_at_boundary,
    test_update_extends_line_with_far_scroll_and_ctrlchars,
    test_update_scans_forward_to_find_dot_in_view,
    test_update_reframes_with_various_force_values,
};
#pragma rodata-name (pop)
