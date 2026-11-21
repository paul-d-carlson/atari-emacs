/*
 * test_line1.c
 *
 * First half of the src/line.c unit tests (see also test_line2.c).
 * Both halves test the REAL src/line.c, linked with its real
 * collaborators, through the same resident/overlay ABI architecture
 * used by every other suite (see test_file1.c's header for the full
 * rationale, and abi_table.h for the 4 new slots this suite's
 * insspace/kdelete/kinsert/kremove calls needed -- linsert/lnewline/
 * ldelete were already exported for earlier suites).
 *
 * SELF-CONTAINED BY DESIGN: per explicit instruction, every test below
 * builds its OWN fresh buffer (bfind + swbuffer) and its own line
 * content from scratch, rather than relying on incidental line.c
 * coverage from test_buffer.c/test_basic.c/test_file1.c/etc. Each test
 * must independently demonstrate the branch it targets.
 *
 * This file covers linsert() and lnewline(). ldelete()/ldelnewline(),
 * the kill-buffer trio (kdelete/kinsert/kremove), and insspace() live in
 * test_line2.c.
 *
 * Known, accepted, permanent gaps (not chased, documented for the
 * record -- see also tests/README.md):
 *   - lalloc()'s OOM (malloc returning NULL) return path: like every
 *     other suite's mlalloc-backed calls, this cannot be forced under
 *     emulation without corrupting the heap for every later test.
 *   - ldelnewline()'s own internal "if (curbp->b_mode & MDVIEW) return
 *     rdonly();" check is dead code in practice: ldelete() (its only
 *     caller) already performs the identical MDVIEW check first and
 *     returns before ever reaching ldelnewline(). We do not contort a
 *     test to force this unreachable path; test_line2.c's MDVIEW test
 *     instead confirms ldelete()'s own (real, reachable) guard.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_line1.h"

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

/* ------------------------------------------------------------------- */
/* linsert(): true-end-of-buffer special case + BFCHG/lchange.         */
/* ------------------------------------------------------------------- */

static unsigned char test_linsert_at_buffer_end_allocates_first_line(void)
{
    BUFFER *bp = bfind("line1a", TRUE, 0);

    swbuffer(bp);

    /* Fresh buffer: dot sits on the header line itself, doto == 0. */
    ASSERT(curwp->w_dotp == curbp->b_linep);
    ASSERT(curwp->w_doto == 0);
    ASSERT((curbp->b_flag & BFCHG) == 0);

    ASSERT(linsert(3, 'A') != FALSE);

    /* A brand-new real line was allocated and linked in; dot moved off
     * the header onto it, positioned after the 3 new characters. */
    ASSERT(curwp->w_dotp != curbp->b_linep);
    ASSERT(curwp->w_doto == 3);
    ASSERT(line_matches(curwp->w_dotp, "AAA"));
    ASSERT((curbp->b_flag & BFCHG) != 0);

    return TRUE;
}

static unsigned char test_linsert_at_buffer_end_with_nonzero_doto_fails(void)
{
    BUFFER *bp = bfind("line1b", TRUE, 0);

    swbuffer(bp);
    ASSERT(curwp->w_dotp == curbp->b_linep);

    /* Force the invalid combination linsert() explicitly guards against:
     * dot still on the header line/"true end", but doto != 0. */
    curwp->w_doto = 1;
    ASSERT(linsert(1, 'X') == FALSE);

    /* Buffer must stay completely untouched. */
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep);

    return TRUE;
}

static unsigned char test_linsert_extends_existing_line_in_place(void)
{
    BUFFER *bp = bfind("line1c", TRUE, 0);

    swbuffer(bp);
    ASSERT(linsert(1, 'H') != FALSE);      /* buffer-end special case */
    ASSERT(linsert(1, 'I') != FALSE);      /* normal end-of-line append */

    ASSERT(line_matches(curwp->w_dotp, "HI"));
    ASSERT(curwp->w_doto == 2);

    return TRUE;
}

static unsigned char test_linsert_mid_line_shifts_tail_in_place(void)
{
    BUFFER *bp = bfind("line1d", TRUE, 0);

    swbuffer(bp);
    ASSERT(linsert(1, 'A') != FALSE);
    ASSERT(linsert(1, 'C') != FALSE);
    ASSERT(linsert(1, 'E') != FALSE);      /* "ACE" */
    ASSERT(line_matches(curwp->w_dotp, "ACE"));

    curwp->w_doto = 1;                     /* between 'A' and 'C' */
    ASSERT(linsert(1, 'B') != FALSE);

    ASSERT(line_matches(curwp->w_dotp, "ABCE"));
    ASSERT(curwp->w_doto == 2);            /* dot advanced past the insert */

    return TRUE;
}

/* Builds a line that is EXACTLY full (l_used == l_size, both 16 -- the
 * NBLOCK rounding for a first allocation of <=16 bytes), then inserts
 * one more character at a MID-LINE position. This forces l_used+n >
 * l_size (the hard-reallocate branch) AND doto < l_used (the shift-tail
 * loop that copies the post-insert-point remainder into the new,
 * larger block) -- both currently-uncovered by every other suite,
 * whose linsert() calls only ever append at end-of-line. */
static unsigned char test_linsert_mid_line_hard_realloc_shifts_tail(void)
{
    BUFFER *bp = bfind("line1e", TRUE, 0);
    unsigned char i;

    swbuffer(bp);
    ASSERT(linsert(16, 'X') != FALSE);     /* one call: 16 'X's, exactly full */
    ASSERT(llength(curwp->w_dotp) == 16);

    curwp->w_doto = 8;                     /* mid-line: 8 'X's before, 8 after */
    ASSERT(linsert(1, 'Y') != FALSE);      /* forces realloc + tail shift */

    ASSERT(llength(curwp->w_dotp) == 17);
    for (i = 0; i < 8; i++)
        ASSERT(lgetc(curwp->w_dotp, i) == 'X');
    ASSERT(lgetc(curwp->w_dotp, 8) == 'Y');
    for (i = 9; i < 17; i++)
        ASSERT(lgetc(curwp->w_dotp, i) == 'X');
    ASSERT(curwp->w_doto == 9);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* lnewline(): split branches, including doto==0 (lalloc(0)) and the   */
/* w_linep/w_markp relocation branches (dot's own branch is always the */
/* "stays on the second half" case, since dot's own doto can never be  */
/* less than the split point it supplies).                            */
/* ------------------------------------------------------------------- */

static unsigned char test_lnewline_splits_line_and_relocates_linep_and_mark(void)
{
    BUFFER *bp = bfind("line1f", TRUE, 0);
    LINE *lp1;

    swbuffer(bp);
    ASSERT(linsert(1, 'H') != FALSE);
    ASSERT(linsert(1, 'E') != FALSE);
    ASSERT(linsert(1, 'L') != FALSE);
    ASSERT(linsert(1, 'L') != FALSE);
    ASSERT(linsert(1, 'O') != FALSE);      /* "HELLO" */
    lp1 = curwp->w_dotp;

    curwp->w_doto = 2;                     /* split between "HE" | "LLO" */

    /* Force the w_linep-relocation branch: pretend the window's top-of-
     * screen line is the line about to be split. */
    curwp->w_linep = lp1;

    /* Force the w_markp "lands in the first half" branch: mark sits
     * BEFORE the split point. */
    curwp->w_markp = lp1;
    curwp->w_marko = 1;

    ASSERT(lnewline() != FALSE);

    /* A new first-half line "HE" now precedes lp1, which continues as
     * "LLO". Dot's own split point equals its own doto, so it takes the
     * "stays on the (now-shrunk) second half" branch. */
    ASSERT(lback(lp1) != curbp->b_linep);
    ASSERT(line_matches(lback(lp1), "HE"));
    ASSERT(line_matches(lp1, "LLO"));

    ASSERT(curwp->w_dotp == lp1);
    ASSERT(curwp->w_doto == 0);

    /* w_linep and the mark both relocated onto the new first half. */
    ASSERT(curwp->w_linep == lback(lp1));
    ASSERT(curwp->w_markp == lback(lp1));
    ASSERT(curwp->w_marko == 1);

    return TRUE;
}

static unsigned char test_lnewline_at_line_start_creates_empty_first_half(void)
{
    BUFFER *bp = bfind("line1g", TRUE, 0);
    LINE *lp1;

    swbuffer(bp);
    ASSERT(linsert(1, 'W') != FALSE);
    ASSERT(linsert(1, 'O') != FALSE);
    ASSERT(linsert(1, 'R') != FALSE);
    ASSERT(linsert(1, 'L') != FALSE);
    ASSERT(linsert(1, 'D') != FALSE);      /* "WORLD" */
    lp1 = curwp->w_dotp;

    curwp->w_doto = 0;                     /* split at the very start */
    ASSERT(lnewline() != FALSE);

    /* First half is a brand-new, EMPTY line (exercises lalloc(0)'s
     * size==0 -> NBLOCK special case within this suite's own call). */
    ASSERT(lback(lp1) != curbp->b_linep);
    ASSERT(llength(lback(lp1)) == 0);
    ASSERT(line_matches(lp1, "WORLD"));
    ASSERT(curwp->w_dotp == lp1);
    ASSERT(curwp->w_doto == 0);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* MDVIEW guard: linsert()/lnewline() must both refuse to modify a     */
/* read-only buffer. Never exercised by any other suite.               */
/* ------------------------------------------------------------------- */

static unsigned char test_linsert_and_lnewline_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("line1h", TRUE, 0);

    swbuffer(bp);
    ASSERT(linsert(1, 'Z') != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "Z"));

    curbp->b_mode |= MDVIEW;

    ASSERT(linsert(1, 'Q') == FALSE);
    ASSERT(lnewline() == FALSE);

    /* Buffer content must be completely unchanged. */
    ASSERT(line_matches(curwp->w_dotp, "Z"));
    ASSERT(lforw(curwp->w_dotp) == curbp->b_linep);

    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn line1_tests[NUM_LINE1_TESTS] = {
    test_linsert_at_buffer_end_allocates_first_line,
    test_linsert_at_buffer_end_with_nonzero_doto_fails,
    test_linsert_extends_existing_line_in_place,
    test_linsert_mid_line_shifts_tail_in_place,
    test_linsert_mid_line_hard_realloc_shifts_tail,
    test_lnewline_splits_line_and_relocates_linep_and_mark,
    test_lnewline_at_line_start_creates_empty_first_half,
    test_linsert_and_lnewline_blocked_in_readonly_mode,
};
#pragma rodata-name (pop)
