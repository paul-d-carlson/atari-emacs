/*
 * test_random3.c
 *
 * Third of three src/random.c test files (see also test_random1.c,
 * test_random2.c). This file covers killtext() and yank() -- split out
 * of the original test_random2.c (which had grown to cover indent(),
 * forwdel(), backdel(), killtext(), and yank() together, 23 tests,
 * 6677 bytes) because that single overlay had become the largest in
 * the whole suite, which drives the shared OVERLAY_SIZE floor (the
 * fixed-size disk-overlay window every suite's compiled test binary is
 * loaded into, one at a time -- see abi_table.h). Once src/search.c's
 * suite added ~1.3KB of new *resident* CODE (not overlay code), the
 * resident heap/stack margin needed to shrink OVERLAY_SIZE further than
 * random2's 6677-byte floor allowed. Splitting random2 (and
 * test_display.c, see test_display2.c) lowers that floor, freeing more
 * heap/stack margin without touching any real application logic.
 *
 * SELF-CONTAINED BY DESIGN: every test below builds its own fresh buffer
 * (bfind + swbuffer) and its own line content from scratch, matching the
 * line.c suites' convention.
 *
 * killtext()/yank() both interact with the shared kill buffer and the
 * lastflag/thisflag CFKILL bookkeeping (like test_line2.c's
 * kdelete/kinsert/kremove tests) -- every test that exercises the
 * "kill" path explicitly calls kdelete() and clears lastflag first, to
 * guarantee a known-empty starting kill buffer regardless of what any
 * earlier test in this suite (or run) left behind.
 *
 * Known, accepted, permanent gaps (see tests/README.md):
 *   - None of killtext()/yank()/ldelete()/linsert()'s own OOM (malloc
 *     failure) paths are forced here, same as every other suite that
 *     touches mlalloc-backed calls under emulation.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_random3.h"

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
/* killtext(): the four f/n combinations, plus the n-exceeds-buffer and */
/* negative-n edge cases. Always deposits to the kill buffer.          */
/* ------------------------------------------------------------------- */

static unsigned char test_killtext_kills_to_end_of_line(void)
{
    BUFFER *bp = bfind("rand3n", TRUE, 0);

    kdelete();
    swbuffer(bp);
    insert_chars("HELLO WORLD");
    curwp->w_doto = 5;
    lastflag = 0;

    ASSERT(killtext(FALSE, 0) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "HELLO"));
    ASSERT(kremove(0) == ' ');
    ASSERT(kremove(5) == 'D');
    ASSERT(kremove(6) == -1);

    return TRUE;
}

static unsigned char test_killtext_kills_newline_when_at_end_of_line(void)
{
    BUFFER *bp = bfind("rand3o", TRUE, 0);

    kdelete();
    swbuffer(bp);
    insert_chars("AB");
    ASSERT(lnewline() != FALSE);
    insert_chars("CD");
    gotobob(FALSE, 0);
    curwp->w_doto = 2;                     /* end of "AB", before the newline */
    lastflag = 0;

    ASSERT(killtext(FALSE, 0) != FALSE);

    /* The newline between "AB" and "CD" is gone: one merged line. */
    ASSERT(line_matches(curwp->w_dotp, "ABCD"));
    ASSERT(kremove(0) == '\n');
    ASSERT(kremove(1) == -1);

    return TRUE;
}

static unsigned char test_killtext_kills_from_bol_to_dot_when_n_zero(void)
{
    BUFFER *bp = bfind("rand3p", TRUE, 0);

    kdelete();
    swbuffer(bp);
    insert_chars("HELLO");
    curwp->w_doto = 3;
    lastflag = 0;

    ASSERT(killtext(TRUE, 0) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "LO"));
    ASSERT(kremove(0) == 'H');
    ASSERT(kremove(2) == 'L');
    ASSERT(kremove(3) == -1);

    return TRUE;
}

static unsigned char test_killtext_kills_n_lines_forward(void)
{
    BUFFER *bp = bfind("rand3q", TRUE, 0);

    kdelete();
    swbuffer(bp);
    insert_chars("AB");
    ASSERT(lnewline() != FALSE);
    insert_chars("CD");
    ASSERT(lnewline() != FALSE);
    insert_chars("EF");
    gotobob(FALSE, 0);
    lastflag = 0;

    ASSERT(killtext(TRUE, 2) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "EF"));
    ASSERT(lforw(curwp->w_dotp) == curbp->b_linep);   /* only one line left */

    return TRUE;
}

static unsigned char test_killtext_returns_false_when_n_exceeds_remaining_lines(void)
{
    BUFFER *bp = bfind("rand3r", TRUE, 0);

    kdelete();
    swbuffer(bp);
    insert_chars("AB");
    gotobob(FALSE, 0);
    lastflag = 0;

    ASSERT(killtext(TRUE, 5) == FALSE);

    return TRUE;
}

static unsigned char test_killtext_negative_n_reports_neg_message(void)
{
    BUFFER *bp = bfind("rand3s", TRUE, 0);

    kdelete();
    swbuffer(bp);
    lastflag = 0;

    ASSERT(killtext(TRUE, -1) == FALSE);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* yank(): re-inserts the kill buffer, handling embedded newlines.     */
/* ------------------------------------------------------------------- */

static unsigned char test_yank_inserts_previously_killed_text(void)
{
    BUFFER *bp = bfind("rand3t", TRUE, 0);

    kdelete();
    ASSERT(kinsert('A') != FALSE);
    ASSERT(kinsert('B') != FALSE);
    ASSERT(kinsert('C') != FALSE);

    swbuffer(bp);
    ASSERT(yank(FALSE, 1) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "ABC"));

    return TRUE;
}

static unsigned char test_yank_handles_embedded_newline(void)
{
    BUFFER *bp = bfind("rand3u", TRUE, 0);

    kdelete();
    ASSERT(kinsert('A') != FALSE);
    ASSERT(kinsert('\n') != FALSE);
    ASSERT(kinsert('B') != FALSE);

    swbuffer(bp);
    ASSERT(yank(FALSE, 1) != FALSE);

    ASSERT(line_matches(lback(curwp->w_dotp), "A"));
    ASSERT(line_matches(curwp->w_dotp, "B"));

    return TRUE;
}

static unsigned char test_yank_negative_returns_false(void)
{
    BUFFER *bp = bfind("rand3v", TRUE, 0);

    swbuffer(bp);
    ASSERT(yank(FALSE, -1) == FALSE);

    return TRUE;
}

static unsigned char test_yank_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("rand3w", TRUE, 0);

    kdelete();
    ASSERT(kinsert('Z') != FALSE);

    swbuffer(bp);
    curbp->b_mode |= MDVIEW;

    ASSERT(yank(FALSE, 1) == FALSE);
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep);   /* buffer still empty */

    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn random3_tests[NUM_RANDOM3_TESTS] = {
    test_killtext_kills_to_end_of_line,
    test_killtext_kills_newline_when_at_end_of_line,
    test_killtext_kills_from_bol_to_dot_when_n_zero,
    test_killtext_kills_n_lines_forward,
    test_killtext_returns_false_when_n_exceeds_remaining_lines,
    test_killtext_negative_n_reports_neg_message,
    test_yank_inserts_previously_killed_text,
    test_yank_handles_embedded_newline,
    test_yank_negative_returns_false,
    test_yank_blocked_in_readonly_mode,
};
#pragma rodata-name (pop)
