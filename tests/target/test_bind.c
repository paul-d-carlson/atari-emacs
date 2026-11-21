/*
 * test_bind.c
 *
 * Unit tests for the REAL src/bind.c -- which, in this Atari port, has
 * been trimmed down to a single function: help(). (The desktop-EMACS
 * key-binding/command-name-lookup machinery bind.c traditionally holds
 * -- getname(), desbind(), bindtokey(), etc. -- doesn't exist in this
 * codebase; the Atari build uses a fixed key table, so bind.c only kept
 * the "read the on-disk help file into a buffer" logic.)
 *
 * help() tries to ffropen() "D:EMACS.HLP" (via a couple of path-prefix
 * variations from src/epath.h's pathname[]), and on success reads it
 * into a new buffer with getfile(). This test harness boots a real
 * Atari DOS 2.5 disk image (see README.md's "Architecture" section), so
 * there is a genuine "D:" CIO device handler -- both of help()'s paths
 * are exercised for real, with no mocking:
 *
 *   - test_help_reports_not_online runs FIRST, while D:EMACS.HLP
 *     genuinely does not exist yet on the mounted disk, so ffropen()
 *     genuinely fails with FIOFNF -- the same failure a real machine
 *     with no help file installed would hit.
 *   - test_help_reads_real_help_file runs SECOND: it writes a real
 *     D:EMACS.HLP file using the real ffwopen()/ffputline()/ffclose()
 *     primitives from src/fileio.c (rather than staging a fixture file
 *     on the disk image at build time), so the first test's "really not
 *     there" failure path stays genuinely exercised, and this test's
 *     success path -- ffclose()/getfile()/MDVIEW/WFMODE -- is exercised
 *     against real disk I/O afterward.
 *
 * Order matters here and is fixed by bind_tests[] below.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_bind.h"

static unsigned char test_help_reports_not_online(void)
{
    BUFFER *before = curbp;

    ASSERT(help(FALSE, 0) == FALSE);

    /* No disk means no file was ever read in: the current buffer must
     * be unchanged, and it must not have picked up MDVIEW from a
     * successful getfile(). */
    ASSERT(curbp == before);
    ASSERT((curbp->b_mode & MDVIEW) == 0);

    return TRUE;
}

/* Writes a real help file to the real "D:" device via src/fileio.c's
 * actual ffwopen()/ffputline()/ffclose() -- no mocking -- then calls the
 * real help() and checks that it read the real file back correctly. */
static unsigned char test_help_reads_real_help_file(void)
{
    char line1[] = "Fake help line one";
    char line2[] = "Fake help line two";
    LINE *lp;
    unsigned char i;

    ASSERT(ffwopen("D:EMACS.HLP") == FIOSUC);
    ASSERT(ffputline(line1, sizeof(line1) - 1) == FIOSUC);
    ASSERT(ffputline(line2, sizeof(line2) - 1) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    ASSERT(help(FALSE, 0) == TRUE);

    /* help() should have created/switched to a new buffer holding the
     * real file contents, and put it in view mode. */
    ASSERT((curbp->b_mode & MDVIEW) != 0);
    ASSERT((curwp->w_flag & WFMODE) != 0);

    lp = lforw(curbp->b_linep);
    ASSERT(lp != curbp->b_linep);
    ASSERT(llength(lp) == (int)(sizeof(line1) - 1));
    for (i = 0; i < sizeof(line1) - 1; i++)
        ASSERT((unsigned char)lgetc(lp, i) == (unsigned char)line1[i]);

    lp = lforw(lp);
    ASSERT(lp != curbp->b_linep);
    ASSERT(llength(lp) == (int)(sizeof(line2) - 1));
    for (i = 0; i < sizeof(line2) - 1; i++)
        ASSERT((unsigned char)lgetc(lp, i) == (unsigned char)line2[i]);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn bind_tests[NUM_BIND_TESTS] = {
    test_help_reports_not_online,
    test_help_reads_real_help_file,
};
#pragma rodata-name (pop)
