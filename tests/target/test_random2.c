/*
 * test_random2.c
 *
 * Second of three src/random.c test files (see also test_random1.c,
 * test_random3.c). This file covers indent(), forwdel(), and backdel();
 * killtext()/yank() moved to test_random3.c (see that file's header for
 * why the original single test_random2.c -- indent+forwdel+backdel+
 * killtext+yank, 23 tests, 6677 bytes -- was split in two: it had become
 * the single largest overlay, driving the whole architecture's shared
 * OVERLAY_SIZE floor and starving the resident heap/stack margin once
 * src/search.c's suite was added).
 *
 * SELF-CONTAINED BY DESIGN: every test below builds its own fresh buffer
 * (bfind + swbuffer) and its own line content from scratch, matching the
 * line.c suites' convention.
 *
 * forwdel()/backdel() both interact with the shared kill buffer and the
 * lastflag/thisflag CFKILL bookkeeping (like test_line2.c's
 * kdelete/kinsert/kremove tests) -- every test that exercises the
 * "kill" path explicitly calls kdelete() and clears lastflag first, to
 * guarantee a known-empty starting kill buffer regardless of what any
 * earlier test in this suite (or run) left behind.
 *
 * Known, accepted, permanent gaps (see tests/README.md):
 *   - None of forwdel()/backdel()/ldelete()'s own OOM (malloc failure)
 *     paths are forced here, same as every other suite that touches
 *     mlalloc-backed calls under emulation.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_random2.h"

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
/* indent(): duplicates the leading whitespace of the current line     */
/* onto a freshly-inserted line below it.                              */
/* ------------------------------------------------------------------- */

static unsigned char test_indent_duplicates_space_indentation(void)
{
    BUFFER *bp = bfind("rand2a", TRUE, 0);

    swbuffer(bp);
    insert_chars("   abc");
    /* dot already at true end (doto == llength) after insert_chars() */

    ASSERT(indent(FALSE, 1) != FALSE);

    ASSERT(line_matches(lback(curwp->w_dotp), "   abc"));
    ASSERT(line_matches(curwp->w_dotp, "   "));

    return TRUE;
}

static unsigned char test_indent_duplicates_tab_and_space_indentation(void)
{
    BUFFER *bp = bfind("rand2b", TRUE, 0);

    swbuffer(bp);
    insert_chars("\t   abc");

    ASSERT(indent(FALSE, 1) != FALSE);

    ASSERT(line_matches(lback(curwp->w_dotp), "\t   abc"));
    ASSERT(line_matches(curwp->w_dotp, "\t   "));

    return TRUE;
}

static unsigned char test_indent_negative_returns_false(void)
{
    BUFFER *bp = bfind("rand2c", TRUE, 0);

    swbuffer(bp);
    ASSERT(indent(FALSE, -1) == FALSE);

    return TRUE;
}

static unsigned char test_indent_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("rand2d", TRUE, 0);

    swbuffer(bp);
    insert_chars("  x");
    curbp->b_mode |= MDVIEW;

    ASSERT(indent(FALSE, 1) == FALSE);
    ASSERT(line_matches(curwp->w_dotp, "  x"));

    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* forwdel(): deletes forward, kill-mode deposits to the kill buffer,  */
/* negative delegates to backdel().                                    */
/* ------------------------------------------------------------------- */

static unsigned char test_forwdel_deletes_forward_simple(void)
{
    BUFFER *bp = bfind("rand2e", TRUE, 0);

    swbuffer(bp);
    insert_chars("HELLO");
    curwp->w_doto = 0;

    ASSERT(forwdel(FALSE, 2) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "LLO"));

    return TRUE;
}

static unsigned char test_forwdel_kill_mode_deposits_to_kill_buffer(void)
{
    BUFFER *bp = bfind("rand2f", TRUE, 0);

    kdelete();
    swbuffer(bp);
    insert_chars("GOODBYE");
    curwp->w_doto = 0;
    lastflag = 0;                          /* previous command wasn't a kill */

    ASSERT(forwdel(TRUE, 4) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "BYE"));
    ASSERT((thisflag & CFKILL) != 0);
    ASSERT(kremove(0) == 'G');
    ASSERT(kremove(3) == 'D');
    ASSERT(kremove(4) == -1);

    return TRUE;
}

static unsigned char test_forwdel_negative_delegates_to_backdel(void)
{
    BUFFER *bp = bfind("rand2g", TRUE, 0);

    swbuffer(bp);
    insert_chars("HELLO");
    /* dot at true end (doto == 5) after insert_chars() */

    ASSERT(forwdel(FALSE, -2) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "HEL"));

    return TRUE;
}

static unsigned char test_forwdel_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("rand2h", TRUE, 0);

    swbuffer(bp);
    insert_chars("X");
    curwp->w_doto = 0;
    curbp->b_mode |= MDVIEW;

    ASSERT(forwdel(FALSE, 1) == FALSE);
    ASSERT(line_matches(curwp->w_dotp, "X"));

    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* backdel(): moves back then deletes forward; fails at buffer start.  */
/* ------------------------------------------------------------------- */

static unsigned char test_backdel_deletes_backward_simple(void)
{
    BUFFER *bp = bfind("rand2i", TRUE, 0);

    swbuffer(bp);
    insert_chars("HELLO");
    /* dot at true end (doto == 5) */

    ASSERT(backdel(FALSE, 2) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "HEL"));

    return TRUE;
}

static unsigned char test_backdel_kill_mode_deposits_to_kill_buffer(void)
{
    BUFFER *bp = bfind("rand2j", TRUE, 0);

    kdelete();
    swbuffer(bp);
    insert_chars("GOODBYE");
    lastflag = 0;

    ASSERT(backdel(TRUE, 4) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "GOO"));
    ASSERT((thisflag & CFKILL) != 0);
    ASSERT(kremove(0) == 'D');
    ASSERT(kremove(1) == 'B');
    ASSERT(kremove(2) == 'Y');
    ASSERT(kremove(3) == 'E');
    ASSERT(kremove(4) == -1);

    return TRUE;
}

static unsigned char test_backdel_fails_at_buffer_start(void)
{
    BUFFER *bp = bfind("rand2k", TRUE, 0);

    swbuffer(bp);
    insert_chars("X");
    gotobob(FALSE, 0);

    ASSERT(backdel(FALSE, 1) == FALSE);
    ASSERT(line_matches(curwp->w_dotp, "X"));

    return TRUE;
}

static unsigned char test_backdel_negative_delegates_to_forwdel(void)
{
    BUFFER *bp = bfind("rand2l", TRUE, 0);

    swbuffer(bp);
    insert_chars("HELLO");
    curwp->w_doto = 0;

    ASSERT(backdel(FALSE, -2) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "LLO"));

    return TRUE;
}

static unsigned char test_backdel_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("rand2m", TRUE, 0);

    swbuffer(bp);
    insert_chars("X");
    curbp->b_mode |= MDVIEW;

    ASSERT(backdel(FALSE, 1) == FALSE);
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
const test_fn random2_tests[NUM_RANDOM2_TESTS] = {
    test_indent_duplicates_space_indentation,
    test_indent_duplicates_tab_and_space_indentation,
    test_indent_negative_returns_false,
    test_indent_blocked_in_readonly_mode,
    test_forwdel_deletes_forward_simple,
    test_forwdel_kill_mode_deposits_to_kill_buffer,
    test_forwdel_negative_delegates_to_backdel,
    test_forwdel_blocked_in_readonly_mode,
    test_backdel_deletes_backward_simple,
    test_backdel_kill_mode_deposits_to_kill_buffer,
    test_backdel_fails_at_buffer_start,
    test_backdel_negative_delegates_to_forwdel,
    test_backdel_blocked_in_readonly_mode,
};
#pragma rodata-name (pop)
