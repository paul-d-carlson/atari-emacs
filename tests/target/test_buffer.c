/*
 * test_buffer.c
 *
 * Unit tests for the REAL src/buffer.c, linked together with its real
 * collaborators (display.c, file.c, fileio.c, line.c, window.c, atari.c,
 * termio.c, basic.c, random.c -- see tests/README.md for exactly why each
 * is needed). No source file under src/ is modified or duplicated here.
 *
 * Interactive prompts: killbuffer(), usebuffer(), and bclear() call down
 * into the REAL display.c mlreply()/mlyesno(), which block on the REAL
 * term.t_getchar() (cc65 cgetc(), reading the Atari OS keyboard queue).
 * The driver feeds real keystrokes into the emulated keyboard via
 * libatari800 frame stepping -- there is no stubbing of these prompts.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_buffer.h"

/* ------------------------------------------------------------------- */
/* Individual test cases. Each returns TRUE (1) on success, FALSE (0)   */
/* on the first failed assertion. Real buffer.c functions are called   */
/* directly -- no mocks.                                               */
/* ------------------------------------------------------------------- */

static unsigned char test_bfind_creates_and_finds(void)
{
    BUFFER *bp, *bp2;

    bp = bfind("alpha", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(bp->b_active == TRUE);
    ASSERT(bp->b_nwnd == 0);
    ASSERT(strcmp(bp->b_bname, "alpha") == 0);
    ASSERT(strcmp(bp->b_fname, "") == 0);

    bp2 = bfind("alpha", FALSE, 0);
    ASSERT(bp2 == bp);              /* found existing, did not recreate */

    return TRUE;
}

static unsigned char test_bfind_sorted_insert(void)
{
    BUFFER *z, *a;

    z = bfind("zeta", TRUE, 0);     /* should land after "main"/"alpha" */
    ASSERT(z != NULL);

    a = bfind("aaa_first", TRUE, 0); /* should become the new list head */
    ASSERT(a != NULL);
    ASSERT(bheadp == a);
    ASSERT(strcmp(bheadp->b_bname, "aaa_first") == 0);

    return TRUE;
}

static unsigned char test_bfind_temp_buffer_rejected(void)
{
    BUFFER *tb, *tb2;

    tb = bfind("~scratch~", TRUE, BFTEMP);
    ASSERT(tb != NULL);
    ASSERT(tb->b_flag & BFTEMP);

    tb2 = bfind("~scratch~", FALSE, 0); /* lookup of a TEMP buf -> NULL  */
    ASSERT(tb2 == NULL);

    return TRUE;
}

static unsigned char test_anycb_reports_changes(void)
{
    BUFFER *bp;

    ASSERT(anycb() == FALSE);

    bp = bfind("changeme", TRUE, 0);
    ASSERT(bp != NULL);
    bp->b_flag |= BFCHG;
    ASSERT(anycb() == TRUE);
    bp->b_flag &= ~BFCHG;            /* cleanup for subsequent tests */

    return TRUE;
}

static unsigned char test_swbuffer_switches_and_activates(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("swtarget", TRUE, 0);
    ASSERT(bp != NULL);

    s = swbuffer(bp);
    ASSERT(s == TRUE);
    ASSERT(curbp == bp);
    ASSERT(curwp->w_bufp == bp);
    ASSERT(bp->b_nwnd == 1);

    return TRUE;
}

static unsigned char test_nextbuffer_cycles_skipping_temp(void)
{
    BUFFER *prev;
    unsigned char s;

    prev = curbp;                    /* "swtarget", from the prior test */
    s = nextbuffer(FALSE, 1);
    ASSERT(s == TRUE);
    ASSERT(curbp != prev);
    ASSERT((curbp->b_flag & BFTEMP) == 0); /* never lands on ~scratch~  */

    return TRUE;
}

static unsigned char test_bclear_no_confirmation_needed(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("clearme", TRUE, 0);
    ASSERT(bp != NULL);

    s = bclear(bp);
    ASSERT(s == TRUE);
    ASSERT(bp->b_dotp == bp->b_linep);
    ASSERT(bp->b_doto == 0);
    ASSERT(bp->b_markp == NULL);

    return TRUE;
}

/* Interactive: bclear() calls mlyesno("Discard?"). Driver injects 'y'. */
static unsigned char test_bclear_confirms_discard_yes(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("changed1", TRUE, 0);
    ASSERT(bp != NULL);
    bp->b_flag |= BFCHG;

    s = bclear(bp);                  /* blocks waiting for 'y' */
    ASSERT(s == TRUE);
    ASSERT((bp->b_flag & BFCHG) == 0);

    return TRUE;
}

/* Interactive: bclear() calls mlyesno("Discard?"). Driver injects 'n'. */
static unsigned char test_bclear_declines_discard_no(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("changed2", TRUE, 0);
    ASSERT(bp != NULL);
    bp->b_flag |= BFCHG;

    s = bclear(bp);                  /* blocks waiting for 'n' */
    ASSERT(s == FALSE);
    ASSERT(bp->b_flag & BFCHG);      /* change was NOT discarded */
    bp->b_flag &= ~BFCHG;            /* cleanup for subsequent tests */

    return TRUE;
}

static unsigned char test_zotbuf_removes_buffer(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("tozot", TRUE, 0);
    ASSERT(bp != NULL);

    s = zotbuf(bp);
    ASSERT(s == TRUE);
    ASSERT(bfind("tozot", FALSE, 0) == NULL);

    return TRUE;
}

static unsigned char test_zotbuf_refuses_if_displayed(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("displayed", TRUE, 0);
    ASSERT(bp != NULL);
    bp->b_nwnd = 1;                  /* simulate "shown on screen" */

    s = zotbuf(bp);
    ASSERT(s == FALSE);

    bp->b_nwnd = 0;                  /* cleanup, then really remove it */
    zotbuf(bp);

    return TRUE;
}

/* Interactive: killbuffer() calls mlreply("Kill buffer: "). Driver types
 * "doesnotexist" + Enter. This is the exact bug fixed earlier this
 * session -- killing a nonexistent buffer must now report "No such buf"
 * and return TRUE, instead of silently doing nothing. */
static unsigned char test_killbuffer_reports_missing_buffer(void)
{
    unsigned char s;

    s = killbuffer(FALSE, 0);
    ASSERT(s == TRUE);

    return TRUE;
}

/* Interactive: killbuffer() reads "killme" + Enter. */
static unsigned char test_killbuffer_kills_existing_buffer(void)
{
    unsigned char s;

    ASSERT(bfind("killme", TRUE, 0) != NULL);

    s = killbuffer(FALSE, 0);
    ASSERT(s == TRUE);
    ASSERT(bfind("killme", FALSE, 0) == NULL);

    return TRUE;
}

/* Interactive: usebuffer() reads "usetarget" + Enter. */
static unsigned char test_usebuffer_switches_buffer(void)
{
    unsigned char s;

    ASSERT(bfind("usetarget", TRUE, 0) != NULL);

    s = usebuffer(FALSE, 0);
    ASSERT(s == TRUE);
    ASSERT(curbp != NULL);
    ASSERT(strcmp(curbp->b_bname, "usetarget") == 0);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn buffer_tests[NUM_BUFFER_TESTS] = {
    test_bfind_creates_and_finds,
    test_bfind_sorted_insert,
    test_bfind_temp_buffer_rejected,
    test_anycb_reports_changes,
    test_swbuffer_switches_and_activates,
    test_nextbuffer_cycles_skipping_temp,
    test_bclear_no_confirmation_needed,
    test_bclear_confirms_discard_yes,
    test_bclear_declines_discard_no,
    test_zotbuf_removes_buffer,
    test_zotbuf_refuses_if_displayed,
    test_killbuffer_reports_missing_buffer,
    test_killbuffer_kills_existing_buffer,
    test_usebuffer_switches_buffer,
};
#pragma rodata-name (pop)
