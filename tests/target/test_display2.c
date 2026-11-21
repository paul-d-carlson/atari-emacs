/*
 * test_display2.c
 *
 * Second of two src/display.c test files (see also test_display.c).
 * Covers update()'s single-line WFEDIT fast path, modeline()'s
 * filename field, mlputi()'s negative-number branch, vttidy(), and the
 * interactive mlyesno()/mlreplyt() branches.
 *
 * SPLIT HISTORY: test_display.c originally held all 21 display.c tests
 * (6201 bytes, the single largest overlay at the time). It was split
 * into that file (13 non-interactive tests) plus this one (the
 * remaining 8) when src/search.c's suite added ~1.3KB of new
 * *resident* CODE and needed more OVERLAY_SIZE headroom than the
 * largest overlay (test_random2.c, 6677 bytes) allowed to be
 * reclaimed -- see abi_table.h's OVERLAY_SIZE comment for the full
 * story. Splitting lowers the shared overlay window's floor without
 * touching any real application logic. This split also happens to put
 * test_update_single_line_edit_fast_path (the test a heap-corruption
 * regression previously hung/crashed inside, see abi_table.h) as the
 * very FIRST test after this suite's own reset_buffers() call, giving
 * it the same fresh, low heap high-water mark the file1/file2 split
 * relies on.
 *
 * mlyesno()'s and mlreplyt()'s "happy path" (recognized answer /
 * plain-character-echo-then-Enter) is already exercised for real by
 * test_buffer.c's bclear/killbuffer/usebuffer tests, which block on the
 * real term.t_getchar() the same way. The interactive tests here
 * (mlyesno's invalid-key reprompt, and mlreplyt's
 * erase/kill/abort/caret-echo/custom-terminator branches, called
 * directly since nothing currently linked calls mlreplyt() with a
 * non-'\n' eolchar except src/search.c, which isn't linked here) are
 * driven by a dedicated keystroke segment in
 * driver/test_runner_resident.c, gated on the first of these tests
 * (mlyesno_reprompts_on_invalid_key) being reached, in the exact order
 * display2_tests[] below calls them.
 */

#include <atari.h>
#include <string.h>
#include <conio.h>

#include "resident_api.h"
#include "harness.h"
#include "test_display2.h"

/* Row helper: OS.savmsc points at the start of the 40x24 GR.0 screen
 * (see test_atari.c); each row is term.t_ncol bytes wide. */
static unsigned char *screen_row(unsigned char row)
{
    return OS.savmsc + ((unsigned int)row * (unsigned int)term.t_ncol);
}

/* Counts the contiguous run of non-blank cells starting at column 0 of
 * the given row (comparing against `blank`, an already-erased cell's
 * byte value). */
static unsigned char count_prefix(unsigned char *row, unsigned char blank)
{
    unsigned char n = 0;
    while (n < term.t_ncol && row[n] != blank)
        n++;
    return n;
}

/* Compares the first strlen(s) columns of `row` against what cc65's own
 * cputs() renders for the same ASCII text on a scratch reference row
 * (row 10 -- never touched by any other test in this suite). See
 * test_display.c's copy of this helper for the full rationale. */
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

/*
 * update()'s single-line "WFEDIT only" fast path (redraw just dot's own
 * line, skip the rest of the window) is distinct from the more general
 * WFEDIT|WFHARD branch test_display.c exercises -- real editing
 * primitives (ldelete() etc.) always also set WFHARD, so this branch
 * needs w_flag forced to WFEDIT alone, with dot NOT on the window's
 * first line (so the "find dot's row number" loop actually walks).
 */
static unsigned char test_update_single_line_edit_fast_path(void)
{
    unsigned char *row1;
    unsigned char blank;
    BUFFER *bp = bfind("editfast", TRUE, 0);

    swbuffer(bp);
    insert_text("AAA\nBBB\nCCC");

    gotobob(FALSE, 0);
    curwp->w_flag = WFHARD;
    update();                           /* clean baseline render first */

    curwp->w_linep = curwp->w_dotp;     /* window top = first line     */
    forwline(FALSE, 1);                 /* dot -> "BBB"                */
    curwp->w_flag = WFEDIT;             /* WFEDIT alone: the fast path */
    update();

    row1 = screen_row(1);
    blank = row1[39];
    ASSERT(count_prefix(row1, blank) == 3);   /* "BBB" */

    return TRUE;
}

/* modeline() only ever renders an empty filename field in every
 * test_display.c test; set a real b_fname and confirm it's appended
 * after the buffer name, exercising the "filename present" copy loop. */
static unsigned char test_modeline_shows_filename_when_set(void)
{
    BUFFER *bp = bfind("MODE2", TRUE, 0);
    unsigned char *row;

    swbuffer(bp);
    strcpy(bp->b_fname, "T.TXT");
    bp->b_flag &= ~BFCHG;
    curwp->w_flag |= WFMODE | WFHARD;
    update();

    row = screen_row(term.t_nrow - 1);
    ASSERT(row_matches_string(row, "ME: MODE2 T.TXT ", TRUE));

    bp->b_fname[0] = 0;   /* leave tidy for any later test on this buffer */

    return TRUE;
}

/* mlputi()'s negative-number branch (negate, emit '-', then recurse on
 * the magnitude) is never reached by any positive-only %d caller. */
static unsigned char test_mlputi_handles_negative_numbers(void)
{
    unsigned char *row;

    mlwrite("Val: %d", -42);

    row = screen_row(term.t_nrow);
    ASSERT(row_matches_string(row, "Val: -42", FALSE));

    return TRUE;
}

/* vttidy() (mlerase + movecursor to the message line + t_close, which is
 * a no-op on this target -- see src/termio.c) is only ever called once,
 * at real program exit; nothing above calls it directly. */
static unsigned char test_vttidy_cleans_up_message_line(void)
{
    unsigned char *row;
    unsigned char blank;

    mlwrite("still here");
    row = screen_row(term.t_nrow);
    blank = row[39];
    ASSERT(count_prefix(row, blank) > 0);

    vttidy();

    ASSERT(count_prefix(row, blank) == 0);
    ASSERT(mpresf == FALSE);

    return TRUE;
}

/*
 * mlyesno() loops re-prompting until it sees a recognized key (y/Y, n/N,
 * or BELL-to-abort); any other keystroke must be silently ignored and
 * the loop must go around again -- never exercised by test_buffer.c's
 * bclear tests, which always answer correctly on the first keystroke.
 * Keystroke script: "x" (ignored, reprompt) then "y" (accepted).
 */
static unsigned char test_mlyesno_reprompts_on_invalid_key(void)
{
    ASSERT(mlyesno("Confirm") == TRUE);
    return TRUE;
}

/*
 * mlreplyt()'s rubout/erase (0x7E, the real Atari backspace key) and
 * C-U kill (0x15) branches are never exercised by test_buffer.c's
 * mlreply() callers, which only ever type a clean name then Enter.
 * Keystroke script: "a","b" (buf="ab"), backspace (erase 'b': buf="a"),
 * "c" (buf="ac"), C-U (kill: buf=""), "d","e" (buf="de"), Enter.
 */
static unsigned char test_mlreplyt_erase_and_kill_edit_buffer(void)
{
    char buf[16];
    unsigned char result = mlreplyt("Name: ", buf, sizeof(buf), '\n');

    ASSERT(result == TRUE);
    ASSERT(strcmp(buf, "de") == 0);

    return TRUE;
}

/* mlreplyt()'s BELL/abort branch (C-G) is never exercised elsewhere.
 * Keystroke script: C-G (0x07). */
static unsigned char test_mlreplyt_bell_aborts(void)
{
    char buf[16];
    unsigned char result;

    buf[0] = 'X';   /* sentinel: must stay untouched on ABORT */
    result = mlreplyt("Name: ", buf, sizeof(buf), '\n');

    ASSERT(result == ABORT);

    return TRUE;
}

/*
 * Calls mlreplyt() directly (bypassing mlreply()) with a non-'\n'
 * eolchar, e.g. src/search.c's real "terminate on ESC" pattern (that
 * file isn't linked into this test binary, so this is the only way to
 * exercise that path). With a custom eolchar, an actual Enter keypress
 * no longer terminates -- it falls through to the ordinary echo branch,
 * which special-cases '\n' by echoing the 4 literal characters "<NL>"
 * instead of a real newline. A plain control character (C-B) that isn't
 * BELL/erase/kill takes the OTHER previously-uncovered echo branch:
 * caret-echo ('^' + the letter). Keystroke script: C-B, Enter, ESC.
 */
static unsigned char test_mlreplyt_echoes_ctrlchar_and_custom_eol(void)
{
    char buf[16];
    unsigned char result = mlreplyt("Pat: ", buf, sizeof(buf), 27);

    ASSERT(result == TRUE);
    ASSERT(buf[0] == 0x02);
    ASSERT(buf[1] == '\n');
    ASSERT(buf[2] == 0);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn display2_tests[NUM_DISPLAY2_TESTS] = {
    test_update_single_line_edit_fast_path,
    test_modeline_shows_filename_when_set,
    test_mlputi_handles_negative_numbers,
    test_vttidy_cleans_up_message_line,
    test_mlyesno_reprompts_on_invalid_key,
    test_mlreplyt_erase_and_kill_edit_buffer,
    test_mlreplyt_bell_aborts,
    test_mlreplyt_echoes_ctrlchar_and_custom_eol,
};
#pragma rodata-name (pop)
