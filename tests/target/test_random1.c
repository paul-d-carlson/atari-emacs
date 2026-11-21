/*
 * test_random1.c
 *
 * First half of the src/random.c unit tests (see also test_random2.c).
 * random.c "has no functional grouping" per its own file header comment
 * -- this split is purely by test-count size, matching the
 * test_file1.c/test_file2.c and test_line1.c/test_line2.c precedent.
 *
 * This file covers showcpos(), getccol(), quote(), tab(), openline(),
 * and newline(). indent(), forwdel(), backdel(), killtext(), and yank()
 * live in test_random2.c.
 *
 * SELF-CONTAINED BY DESIGN: every test below builds its own fresh buffer
 * (bfind + swbuffer) and its own line content from scratch, matching the
 * line.c suites' convention.
 *
 * quote() is the only function under test anywhere in this codebase that
 * reads a raw keystroke directly (`(*term.t_getchar)()`), rather than
 * through mlreply()/mlyesno()'s line-editing prompts -- so the 4 tests
 * below that actually reach that read need a real scripted keystroke
 * from the driver's keystroke feed (see driver/test_runner_resident.c's
 * RANDOM1_QUOTE_GATE_TEST, gated on the first of them,
 * test_quote_inserts_literal_char).
 *
 * Known, accepted, permanent gaps (see tests/README.md):
 *   - tabsize is a `static int` local to random.c, so its persistence
 *     across calls is real editor behavior (mirrors typing C-U 0 C-I
 *     once, then plain C-I repeatedly) -- the tab() tests below rely on
 *     this, and on running in a fixed order: tabsize starts at 0
 *     (BSS zero-init), so the "real tab" test must run before any test
 *     that sets a nonzero tabsize.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_random1.h"

static unsigned char line_matches(LINE *lp, const char *s)
{
    unsigned char i;
    unsigned char len = (unsigned char)strlen(s);

    if (llength(lp) != (int)len)
        return FALSE;
    for (i = 0; i < len; i++)
        if ((unsigned char)lgetc(lp, i) != (unsigned char)s[i])
            return FALSE;
    return TRUE;
}

static void insert_chars(const char *s)
{
    for (; *s; s++)
        linsert(1, (unsigned char)*s);
}

/* ------------------------------------------------------------------- */
/* showcpos(): trivial, single straight-line path.                     */
/* ------------------------------------------------------------------- */

static unsigned char test_showcpos_reports_position(void)
{
    BUFFER *bp = bfind("rand1a", TRUE, 0);

    swbuffer(bp);
    insert_chars("HELLO");

    ASSERT(showcpos(FALSE, 0) == TRUE);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* getccol(): tab/control-char column-width expansion, bflg early-stop.*/
/* ------------------------------------------------------------------- */

static unsigned char test_getccol_counts_mixed_content(void)
{
    BUFFER *bp = bfind("rand1b", TRUE, 0);

    swbuffer(bp);
    ASSERT(linsert(1, ' ') != FALSE);      /* col 0 -> 1 (plain)        */
    ASSERT(linsert(1, '\t') != FALSE);     /* col 1 -> 8 (tab stop)     */
    ASSERT(linsert(1, 0x01) != FALSE);     /* col 8 -> 10 (ctrl char)   */
    ASSERT(linsert(1, 'X') != FALSE);      /* col 10 -> 11 (plain)      */

    curwp->w_doto = 4;      /* end of the 4 chars just inserted */
    ASSERT(getccol(FALSE) == 11);

    return TRUE;
}

static unsigned char test_getccol_bflg_stops_at_first_nonblank(void)
{
    BUFFER *bp = bfind("rand1c", TRUE, 0);

    swbuffer(bp);
    insert_chars("  X");           /* 2 leading spaces, then non-blank */
    curwp->w_doto = 3;

    ASSERT(getccol(TRUE) == 2);    /* stops before counting 'X' */

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* quote(): reads one raw keystroke via term.t_getchar, always (except */
/* the MDVIEW guard, which returns before ever reading). See           */
/* driver/test_runner_resident.c's RANDOM1_QUOTE_GATE_TEST for the     */
/* scripted 'Q' '\n' 'Z' 'Z' keys these 4 tests consume, in order.      */
/* ------------------------------------------------------------------- */

static unsigned char test_quote_inserts_literal_char(void)
{
    BUFFER *bp = bfind("rand1d", TRUE, 0);

    swbuffer(bp);
    ASSERT(quote(FALSE, 1) != FALSE);      /* consumes scripted 'Q' */
    ASSERT(line_matches(curwp->w_dotp, "Q"));

    return TRUE;
}

static unsigned char test_quote_inserts_newline(void)
{
    BUFFER *bp = bfind("rand1e", TRUE, 0);

    swbuffer(bp);
    insert_chars("A");
    ASSERT(quote(FALSE, 1) != FALSE);      /* consumes scripted '\n' */

    /* Splits the line: "A" stays on the first half, dot moves onto a
     * brand-new empty second line. */
    ASSERT(llength(curwp->w_dotp) == 0);
    ASSERT(line_matches(lback(curwp->w_dotp), "A"));

    return TRUE;
}

static unsigned char test_quote_zero_count_reads_char_no_insert(void)
{
    BUFFER *bp = bfind("rand1f", TRUE, 0);

    swbuffer(bp);
    ASSERT(quote(FALSE, 0) == TRUE);       /* consumes scripted 'Z', discards it */
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep);   /* buffer still empty */

    return TRUE;
}

static unsigned char test_quote_negative_count_returns_false(void)
{
    BUFFER *bp = bfind("rand1g", TRUE, 0);

    swbuffer(bp);
    ASSERT(quote(FALSE, -1) == FALSE);     /* consumes scripted 'Z', discards it */
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep);   /* buffer still empty */

    return TRUE;
}

static unsigned char test_quote_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("rand1h", TRUE, 0);

    swbuffer(bp);
    curbp->b_mode |= MDVIEW;

    /* Returns via rdonly() before ever reading a keystroke -- consumes
     * nothing from the scripted feed. */
    ASSERT(quote(FALSE, 1) == FALSE);

    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* tab(): the file-scope `tabsize` persists across calls (real editor  */
/* behavior). These tests rely on running in dispatch order: tabsize   */
/* starts at 0 (BSS zero-init), so the "real tab" test below must run  */
/* before any test that sets a nonzero tabsize.                        */
/* ------------------------------------------------------------------- */

static unsigned char test_tab_inserts_real_tab_when_tabsize_zero(void)
{
    BUFFER *bp = bfind("rand1i", TRUE, 0);

    swbuffer(bp);
    ASSERT(tab(FALSE, 1) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "\t"));

    return TRUE;
}

static unsigned char test_tab_sets_tabsize_with_arg(void)
{
    BUFFER *bp = bfind("rand1j", TRUE, 0);

    swbuffer(bp);
    ASSERT(tab(FALSE, 4) != FALSE);        /* n>1: sets tabsize, no insert */
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep);   /* buffer still empty */

    return TRUE;
}

static unsigned char test_tab_inserts_spaces_to_next_stop(void)
{
    BUFFER *bp = bfind("rand1k", TRUE, 0);

    swbuffer(bp);
    insert_chars("AB");                    /* col 2, tabsize == 4 (prior test) */
    ASSERT(tab(FALSE, 1) != FALSE);        /* pads to next stop: 2 spaces */

    ASSERT(line_matches(curwp->w_dotp, "AB  "));

    return TRUE;
}

static unsigned char test_tab_negative_returns_false(void)
{
    BUFFER *bp = bfind("rand1l", TRUE, 0);

    swbuffer(bp);
    ASSERT(tab(FALSE, -1) == FALSE);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* openline(): inserts n blank lines, then backs the cursor up over    */
/* them all via backchar().                                            */
/* ------------------------------------------------------------------- */

static unsigned char test_openline_inserts_blank_lines_and_returns(void)
{
    BUFFER *bp = bfind("rand1m", TRUE, 0);
    LINE *lp;

    swbuffer(bp);
    insert_chars("HELLO");
    curwp->w_doto = 0;                     /* start of "HELLO" */

    ASSERT(openline(FALSE, 2) != FALSE);

    /* Two blank lines were inserted before "HELLO". Check the resulting
     * line structure rather than dot's exact resting spot. */
    lp = lforw(curbp->b_linep);
    ASSERT(llength(lp) == 0);
    lp = lforw(lp);
    ASSERT(llength(lp) == 0);
    lp = lforw(lp);
    ASSERT(line_matches(lp, "HELLO"));

    return TRUE;
}

static unsigned char test_openline_zero_is_noop(void)
{
    BUFFER *bp = bfind("rand1n", TRUE, 0);

    swbuffer(bp);
    ASSERT(openline(FALSE, 0) == TRUE);
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep);   /* buffer still empty */

    return TRUE;
}

static unsigned char test_openline_negative_returns_false(void)
{
    BUFFER *bp = bfind("rand1o", TRUE, 0);

    swbuffer(bp);
    ASSERT(openline(FALSE, -1) == FALSE);

    return TRUE;
}

static unsigned char test_openline_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("rand1p", TRUE, 0);

    swbuffer(bp);
    insert_chars("X");
    curbp->b_mode |= MDVIEW;

    ASSERT(openline(FALSE, 1) == FALSE);
    ASSERT(line_matches(curwp->w_dotp, "X"));

    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* newline(): repeated lnewline() loop, negative guard, MDVIEW guard.  */
/* ------------------------------------------------------------------- */

static unsigned char test_newline_inserts_multiple_lines(void)
{
    BUFFER *bp = bfind("rand1q", TRUE, 0);
    LINE *lp;

    swbuffer(bp);
    insert_chars("HI");
    /* dot already at true end (doto == llength) after insert_chars() */

    ASSERT(newline(FALSE, 3) != FALSE);

    /* "HI" followed by 3 blank lines, dot on the last of them. */
    lp = lforw(curbp->b_linep);
    ASSERT(line_matches(lp, "HI"));
    lp = lforw(lp);
    ASSERT(llength(lp) == 0);
    lp = lforw(lp);
    ASSERT(llength(lp) == 0);
    lp = lforw(lp);
    ASSERT(llength(lp) == 0);
    ASSERT(curwp->w_dotp == lp);

    return TRUE;
}

static unsigned char test_newline_negative_returns_false(void)
{
    BUFFER *bp = bfind("rand1r", TRUE, 0);

    swbuffer(bp);
    ASSERT(newline(FALSE, -1) == FALSE);

    return TRUE;
}

static unsigned char test_newline_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("rand1s", TRUE, 0);

    swbuffer(bp);
    insert_chars("X");
    curbp->b_mode |= MDVIEW;

    ASSERT(newline(FALSE, 1) == FALSE);
    ASSERT(line_matches(curwp->w_dotp, "X"));

    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn random1_tests[NUM_RANDOM1_TESTS] = {
    test_showcpos_reports_position,
    test_getccol_counts_mixed_content,
    test_getccol_bflg_stops_at_first_nonblank,
    test_quote_inserts_literal_char,
    test_quote_inserts_newline,
    test_quote_zero_count_reads_char_no_insert,
    test_quote_negative_count_returns_false,
    test_quote_blocked_in_readonly_mode,
    test_tab_inserts_real_tab_when_tabsize_zero,
    test_tab_sets_tabsize_with_arg,
    test_tab_inserts_spaces_to_next_stop,
    test_tab_negative_returns_false,
    test_openline_inserts_blank_lines_and_returns,
    test_openline_zero_is_noop,
    test_openline_negative_returns_false,
    test_openline_blocked_in_readonly_mode,
    test_newline_inserts_multiple_lines,
    test_newline_negative_returns_false,
    test_newline_blocked_in_readonly_mode,
};
#pragma rodata-name (pop)
