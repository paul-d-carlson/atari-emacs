/*
 * test_file3.c
 *
 * Third src/file.c overlay: the four interactive, mlreply()-prompted
 * command wrappers -- fileread() (^X-R), filefind() (^X-C-F),
 * filewrite() (^X-C-W) and filename() (^X-N). test_file1.c/test_file2.c
 * deliberately left these as "documented gaps" on the theory that they
 * only re-exercise mlreply(); this suite closes that gap by driving each
 * wrapper end-to-end through a REAL keyboard prompt (a dedicated,
 * per-test keystroke segment in driver/test_runner_resident.c, exactly
 * like test_display.c's mlreplyt tests), covering BOTH branches of each
 * wrapper: the C-G abort return AND the success path.
 *
 * WHY EACH TEST IS ITS OWN GATED KEYSTROKE SEGMENT
 * ------------------------------------------------
 * The host harness feeds the keystroke script open-loop (one char every
 * few frames, independent of what the program is doing). Each of these
 * wrappers does real DISK I/O (readin()/getfile()/writeout()) AFTER it
 * has consumed its filename+Enter, for many frames during which the
 * program is NOT reading the keyboard. If the following test's keys were
 * in the same segment they'd be fed into the single-byte OS key register
 * and lost while that disk I/O ran. So every test gets its OWN segment,
 * gated on that test's own global index -- once a segment is fully
 * consumed the harness WAITS (feeds nothing) until the next segment's
 * gate opens, which is precisely when the next test starts and blocks in
 * mlreply(). See the FILE3_*_GATE_TEST block in the runner.
 *
 * CRITICAL: the mlreply()-driven wrapper call must be the FIRST thing
 * each test does -- the gate opens the instant the test begins and the
 * first char is fed on the very next frame, so any multi-frame work
 * (especially disk I/O) BEFORE the first prompt would swallow keys. Any
 * buffer setup that a test needs is therefore done BETWEEN the C-G abort
 * probe and the real call (there is a multi-frame inter-key gap there,
 * and such setup is memory-only -- never disk I/O).
 */

#include "resident_api.h"
#include "harness.h"
#include "test_file3.h"

/* Builds real buffer content via line.c's linsert()/lnewline(), same as
 * test_file2.c -- memory only, no disk I/O (safe to run between the
 * abort probe and the real prompt). */
static void insert_text(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n')
            lnewline();
        else
            linsert(1, (unsigned char)*s);
    }
}

/* TRUE iff curbp's first text line exactly equals s. */
static unsigned char first_line_eq(const char *s)
{
    LINE *lp = lforw(curbp->b_linep);
    int n = (int)strlen(s);
    int i;

    if (lp == curbp->b_linep)
        return FALSE;
    if (llength(lp) != n)
        return FALSE;
    for (i = 0; i < n; i++)
        if (lgetc(lp, i) != (unsigned char)s[i])
            return FALSE;
    return TRUE;
}

/* ------------------------------------------------------------------- */
/* fileread(): ^X-R -- prompt, then readin() into the current buffer.  */
/* Keystroke script: C-G (abort), then "D:TESTFS.TXT" + Enter.         */
/* D:TESTFS.TXT was written earlier this run by test_file2.c's         */
/* filesave_writes_when_changed test (one line, "Persisted by          */
/* filesave"); the resident dispatches file1 -> file2 -> file3 in that */
/* order, so it is on the disk by now.                                 */
/* ------------------------------------------------------------------- */
static unsigned char test_fileread_reads_named_file(void)
{
    unsigned char s;

    s = fileread(FALSE, 0);         /* "D:TESTFS.TXT" + Enter */
    ASSERT(s == TRUE);
    ASSERT(strcmp(curbp->b_fname, "D:TESTFS.TXT") == 0);
    ASSERT(first_line_eq("Persisted by filesave"));

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* filefind(): ^X-C-F -- prompt, then getfile() (new buffer + readin). */
/* Keystroke script: C-G (abort), then "D:TESTFS.TXT" + Enter.         */
/* ------------------------------------------------------------------- */
static unsigned char test_filefind_finds_named_file(void)
{
    unsigned char s;

    s = filefind(FALSE, 0);         /* C-G at the prompt */
    ASSERT(s == ABORT);

    s = filefind(FALSE, 0);         /* "D:TESTFS.TXT" + Enter */
    ASSERT(s == TRUE);
    /* getfile() created a fresh buffer, made it current, and readin()
     * filled it: the current buffer now names and holds that file. */
    ASSERT(strcmp(curbp->b_fname, "D:TESTFS.TXT") == 0);
    ASSERT(first_line_eq("Persisted by filesave"));

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* filewrite(): ^X-C-W -- prompt, writeout(), then adopt the new name  */
/* and clear BFCHG. Keystroke script: C-G (abort), then "D:FW3.TXT" +  */
/* Enter. Buffer content is built BETWEEN the abort probe and the real */
/* prompt (memory-only, safe).                                         */
/* ------------------------------------------------------------------- */
static unsigned char test_filewrite_writes_named_file(void)
{
    static char rbuf[NLINE];
    BUFFER *bp;
    unsigned char s;

    s = filewrite(FALSE, 0);        /* C-G at the prompt */
    ASSERT(s == ABORT);

    /* Fresh, changed, still-unnamed buffer to write out. */
    bp = bfind("fw3buf", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    insert_text("Written by filewrite");
    ASSERT((curbp->b_flag & BFCHG) != 0);
    ASSERT(curbp->b_fname[0] == 0);

    s = filewrite(FALSE, 0);        /* "D:FW3.TXT" + Enter */
    ASSERT(s == TRUE);
    ASSERT(strcmp(curbp->b_fname, "D:FW3.TXT") == 0); /* name adopted */
    ASSERT((curbp->b_flag & BFCHG) == 0);             /* BFCHG cleared */

    /* Read the bytes back off the real disk to prove writeout() ran. */
    ASSERT(ffropen("D:FW3.TXT") == FIOSUC);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "Written by filewrite") == 0);
    ASSERT(ffgetline(rbuf, NLINE) == FIOEOF);
    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* filename(): ^X-N -- rename the current buffer, clear MDVIEW, set     */
/* WFMODE. Keystroke script: C-G (abort), then "D:NEWNAME.TXT" + Enter. */
/* MDVIEW is set on a fresh buffer BETWEEN the abort probe and the real */
/* prompt so the clear is observable.                                  */
/* ------------------------------------------------------------------- */
static unsigned char test_filename_sets_buffer_name(void)
{
    BUFFER *bp;
    unsigned char s;

    s = filename(FALSE, 0);         /* C-G at the prompt */
    ASSERT(s == ABORT);

    bp = bfind("nm3buf", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    curbp->b_mode |= MDVIEW;        /* filename() must clear this */

    s = filename(FALSE, 0);         /* "D:NEWNAME.TXT" + Enter */
    ASSERT(s == TRUE);
    ASSERT(strcmp(curbp->b_fname, "D:NEWNAME.TXT") == 0);
    ASSERT((curbp->b_mode & MDVIEW) == 0);   /* no longer read-only */
    ASSERT((curwp->w_flag & WFMODE) != 0);   /* modeline redraw flagged */

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* filename() with an EMPTY reply (bare Enter -> mlreply() returns      */
/* FALSE, not ABORT): the s==FALSE branch clears the buffer's file      */
/* name. Runs right after the previous test, whose buffer (nm3buf) is   */
/* still current and now named "D:NEWNAME.TXT".                         */
/* Keystroke script: Enter only.                                       */
/* ------------------------------------------------------------------- */
static unsigned char test_filename_blank_clears_name(void)
{
    unsigned char s;

    ASSERT(curbp->b_fname[0] != 0);  /* inherited non-empty name */

    s = filename(FALSE, 0);          /* bare Enter -> FALSE branch */
    ASSERT(s == TRUE);               /* filename() itself returns TRUE */
    ASSERT(curbp->b_fname[0] == 0);  /* name cleared */

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to OVLENTRY so the table lands at OVERLAY_BASE (see
 * test_file2.c for the full rationale). */
#pragma rodata-name (push, "OVLENTRY")
const test_fn file3_tests[NUM_FILE3_TESTS] = {
    test_fileread_reads_named_file,
    test_filefind_finds_named_file,
    test_filewrite_writes_named_file,
    test_filename_sets_buffer_name,
    test_filename_blank_clears_name,
};
#pragma rodata-name (pop)
