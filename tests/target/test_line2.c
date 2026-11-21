/*
 * test_line2.c
 *
 * Second half of the src/line.c unit tests (see test_line1.c's header
 * for the shared architecture/rationale, and its accepted-gaps list,
 * which applies here too). This file covers ldelete()/ldelnewline(),
 * the kill-buffer trio (kdelete/kinsert/kremove), and insspace().
 *
 * SELF-CONTAINED BY DESIGN: every test below builds its own fresh
 * buffer (bfind + swbuffer) and, where kill-buffer state matters, calls
 * kdelete() first to guarantee a known-empty starting state -- none of
 * these tests assume anything left behind by test_line1.c, by an
 * earlier test in this file, or by any other suite.
 *
 * Currently-uncovered-elsewhere branches this file specifically targets
 * (see the plan discussion for the full gap analysis):
 *   - ldelete(..., kflag=TRUE): the kill-while-delete path (every other
 *     suite's ldelete() calls use kflag=FALSE).
 *   - ldelnewline()'s three real branches: "at buffer end, blank line"
 *     (frees the line), "at buffer end, non-blank line" (no-op merge),
 *     "easy" in-place merge (slack available), and "hard" reallocating
 *     merge (no slack) -- ldelnewline() is never reached by ANY other
 *     suite, since nothing else ever deletes across a line boundary.
 *   - kdelete()/kinsert()/kremove(): zero coverage anywhere else.
 *   - insspace(): zero coverage anywhere else (dead code otherwise).
 */

#include "resident_api.h"
#include "harness.h"
#include "test_line2.h"

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
/* ldelete(): same-line deletes, with and without the kill buffer.     */
/* ------------------------------------------------------------------- */

static unsigned char test_ldelete_same_line_without_kill(void)
{
    BUFFER *bp = bfind("line2a", TRUE, 0);

    swbuffer(bp);
    insert_chars("HELLO");
    curwp->w_doto = 0;

    ASSERT(ldelete(2, FALSE) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "LLO"));

    return TRUE;
}

static unsigned char test_ldelete_same_line_with_kill_deposits_chars(void)
{
    BUFFER *bp;

    kdelete();      /* guarantee a known-empty kill buffer */

    bp = bfind("line2b", TRUE, 0);
    swbuffer(bp);
    insert_chars("GOODBYE");
    curwp->w_doto = 0;

    ASSERT(ldelete(4, TRUE) != FALSE);
    ASSERT(line_matches(curwp->w_dotp, "BYE"));

    ASSERT(kremove(0) == 'G');
    ASSERT(kremove(1) == 'O');
    ASSERT(kremove(2) == 'O');
    ASSERT(kremove(3) == 'D');
    ASSERT(kremove(4) == -1);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ldelete() crossing a line boundary -> ldelnewline()'s merge branches */
/* ------------------------------------------------------------------- */

static unsigned char test_ldelete_crosses_newline_easy_merge(void)
{
    BUFFER *bp = bfind("line2c", TRUE, 0);
    LINE *ab_line;

    swbuffer(bp);
    insert_chars("AB");
    ASSERT(lnewline() != FALSE);           /* split at end: "AB" | "" */

    ab_line = lback(curwp->w_dotp);
    ASSERT(line_matches(ab_line, "AB"));
    ASSERT(llength(curwp->w_dotp) == 0);

    insert_chars("CD");                    /* continuing (empty) line -> "CD" */
    ASSERT(line_matches(curwp->w_dotp, "CD"));

    /* Position dot at the end of "AB"; deleting 1 char merges "CD" in. */
    curwp->w_dotp = ab_line;
    curwp->w_doto = 2;

    ASSERT(ldelete(1, FALSE) != FALSE);

    ASSERT(line_matches(curwp->w_dotp, "ABCD"));
    ASSERT(curwp->w_doto == 2);
    ASSERT(lforw(curwp->w_dotp) == curbp->b_linep); /* "CD" line freed/merged */

    return TRUE;
}

/* Builds a first line that is EXACTLY full (l_size == l_used == 16, no
 * slack) followed by a second, one-character line, so the merge cannot
 * fit in place and must take ldelnewline()'s "hard" reallocate branch. */
static unsigned char test_ldelete_crosses_newline_hard_merge_reallocates(void)
{
    BUFFER *bp = bfind("line2d", TRUE, 0);
    LINE *full_line;
    unsigned char i;

    swbuffer(bp);
    ASSERT(linsert(16, 'A') != FALSE);     /* one full 16-byte line */
    ASSERT(lnewline() != FALSE);           /* split at end: "AAAA..." | "" */

    full_line = lback(curwp->w_dotp);
    ASSERT(llength(full_line) == 16);
    ASSERT(llength(curwp->w_dotp) == 0);

    ASSERT(linsert(1, 'B') != FALSE);      /* continuing (empty) line -> "B" */

    curwp->w_dotp = full_line;
    curwp->w_doto = 16;                    /* end of the full line */

    ASSERT(ldelete(1, FALSE) != FALSE);    /* forces hard reallocate-merge */

    ASSERT(llength(curwp->w_dotp) == 17);
    for (i = 0; i < 16; i++)
        ASSERT(lgetc(curwp->w_dotp, i) == 'A');
    ASSERT(lgetc(curwp->w_dotp, 16) == 'B');
    ASSERT(curwp->w_doto == 16);
    ASSERT(lforw(curwp->w_dotp) == curbp->b_linep); /* only one line left */
    ASSERT(lback(curwp->w_dotp) == curbp->b_linep);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ldelete() at the true end of the buffer -> ldelnewline()'s "at      */
/* buffer end" branch, both the blank (freed) and non-blank (kept)     */
/* sub-cases.                                                          */
/* ------------------------------------------------------------------- */

static unsigned char test_ldelete_at_buffer_end_frees_blank_last_line(void)
{
    BUFFER *bp = bfind("line2e", TRUE, 0);

    swbuffer(bp);
    ASSERT(linsert(1, 'X') != FALSE);
    curwp->w_doto = 0;
    ASSERT(ldelete(1, FALSE) != FALSE);    /* removes 'X': line now blank */
    ASSERT(llength(curwp->w_dotp) == 0);

    /* Deleting again with nothing left on the (now blank) last real
     * line forces ldelnewline()'s "at buffer end, blank" branch, which
     * lfree()s it. */
    ASSERT(ldelete(1, FALSE) != FALSE);

    ASSERT(curwp->w_dotp == curbp->b_linep);
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep); /* buffer now empty */

    return TRUE;
}

static unsigned char test_ldelete_at_buffer_end_keeps_nonblank_last_line(void)
{
    BUFFER *bp = bfind("line2f", TRUE, 0);
    LINE *lp;

    swbuffer(bp);
    ASSERT(linsert(1, 'Y') != FALSE);      /* doto now == l_used == 1 */
    lp = curwp->w_dotp;

    /* dot is at end-of-line with nothing to merge (next is the header);
     * ldelnewline()'s "at buffer end, non-blank" branch is a no-op. */
    ASSERT(ldelete(1, FALSE) != FALSE);

    ASSERT(curwp->w_dotp == lp);
    ASSERT(line_matches(lp, "Y"));

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* MDVIEW guard: ldelete() must refuse to modify a read-only buffer.   */
/* ------------------------------------------------------------------- */

static unsigned char test_ldelete_blocked_in_readonly_mode(void)
{
    BUFFER *bp = bfind("line2g", TRUE, 0);

    swbuffer(bp);
    ASSERT(linsert(1, 'Z') != FALSE);
    curwp->w_doto = 0;

    curbp->b_mode |= MDVIEW;
    ASSERT(ldelete(1, FALSE) == FALSE);
    ASSERT(line_matches(curwp->w_dotp, "Z"));
    curbp->b_mode &= ~MDVIEW;

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* Kill buffer: kdelete()/kinsert()/kremove().                          */
/* ------------------------------------------------------------------- */

static unsigned char test_kinsert_kremove_roundtrip_and_reset(void)
{
    kdelete();

    ASSERT(kinsert('A') != FALSE);
    ASSERT(kinsert('B') != FALSE);
    ASSERT(kinsert('C') != FALSE);

    ASSERT(kremove(0) == 'A');
    ASSERT(kremove(1) == 'B');
    ASSERT(kremove(2) == 'C');
    ASSERT(kremove(3) == -1);

    kdelete();
    ASSERT(kremove(0) == -1);   /* state fully reset */

    /* A fresh insert after reset starts again from index 0. */
    ASSERT(kinsert('Z') != FALSE);
    ASSERT(kremove(0) == 'Z');
    kdelete();

    return TRUE;
}

static unsigned char test_kinsert_grows_across_kblock_boundary(void)
{
    unsigned int i;

    kdelete();

    /* KBLOCK == 256: 300 inserts force the initial malloc(256) PLUS at
     * least one realloc() growth past it. */
    for (i = 0; i < 300; i++)
        ASSERT(kinsert((int)('A' + (i % 26))) != FALSE);

    ASSERT(kremove(0)   == 'A' + (0 % 26));
    ASSERT(kremove(255) == 'A' + (255 % 26));
    ASSERT(kremove(299) == 'A' + (299 % 26));
    ASSERT(kremove(300) == -1);

    kdelete();

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* insspace(): the ONLY caller of linsert()+backchar() composed         */
/* together -- otherwise dead code.                                    */
/* ------------------------------------------------------------------- */

static unsigned char test_insspace_inserts_spaces_and_moves_dot_back(void)
{
    BUFFER *bp = bfind("line2h", TRUE, 0);

    swbuffer(bp);
    insert_chars("AC");
    curwp->w_doto = 1;                     /* between 'A' and 'C' */

    ASSERT(insspace(FALSE, 2) != FALSE);

    ASSERT(line_matches(curwp->w_dotp, "A  C"));
    ASSERT(curwp->w_doto == 1);             /* backchar(f,2) undid the advance */

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn line2_tests[NUM_LINE2_TESTS] = {
    test_ldelete_same_line_without_kill,
    test_ldelete_same_line_with_kill_deposits_chars,
    test_ldelete_crosses_newline_easy_merge,
    test_ldelete_crosses_newline_hard_merge_reallocates,
    test_ldelete_at_buffer_end_frees_blank_last_line,
    test_ldelete_at_buffer_end_keeps_nonblank_last_line,
    test_ldelete_blocked_in_readonly_mode,
    test_kinsert_kremove_roundtrip_and_reset,
    test_kinsert_grows_across_kblock_boundary,
    test_insspace_inserts_spaces_and_moves_dot_back,
};
#pragma rodata-name (pop)
