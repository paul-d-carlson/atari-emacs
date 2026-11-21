/*
 * test_file2.c
 *
 * Second half of the src/file.c unit tests (see test_file1.c's header
 * for the full rationale behind splitting the file suite into two
 * overlays -- in short, the free_other_buffers() call the resident
 * dispatcher makes BETWEEN overlays resets the heap high-water mark
 * halfway through, keeping each half's deep-call software-stack overrun
 * from ever landing on a live top-of-heap buffer).
 *
 * This half covers filesave() and dired()/dired_fill(), plus the two
 * readin()/writeout() edge cases (over-long line wrapping, bad-device
 * error return). All four filesave() branches are reachable WITHOUT any
 * mlreply() prompt, so they are tested directly here. The four
 * interactive, mlreply()-prompted wrappers (fileread/filefind/filewrite/
 * filename) instead need a live keyboard prompt and are covered in their
 * own overlay, test_file3.c. dired() and dired_fill() likewise need no
 * prompt (they only read already-set buffer state /
 * bfind()+dired_fill()+swbuffer()). */

#include "resident_api.h"
#include "harness.h"
#include "test_file2.h"

/* Builds real buffer content the same way test_basic.c's build_basic_
 * test_buffer() does: linsert()/lnewline() from line.c, driven off the
 * real curwp/curbp -- no LINE/BUFFER mocking. */
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
/* filesave(): all four branches, none needing an mlreply() prompt.    */
/* ------------------------------------------------------------------- */

static unsigned char test_filesave_skips_when_no_changes(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("savenochg", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    ASSERT((curbp->b_flag & BFCHG) == 0);
    ASSERT(curbp->b_fname[0] == 0);

    s = filesave(FALSE, 1);
    ASSERT(s == TRUE);
    ASSERT(curbp->b_fname[0] == 0); /* nothing written: still no fname */

    return TRUE;
}

static unsigned char test_filesave_reports_no_fname(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("savenofn", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    insert_text("changed but unnamed");
    ASSERT((curbp->b_flag & BFCHG) != 0); /* linsert's lchange() sets it */
    ASSERT(curbp->b_fname[0] == 0);

    s = filesave(FALSE, 1);
    ASSERT(s == FALSE);

    return TRUE;
}

static unsigned char test_filesave_blocked_in_readonly_mode(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("savero", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    curbp->b_mode |= MDVIEW;

    s = filesave(FALSE, 1);
    ASSERT(s == FALSE); /* rdonly()'s return value, checked before BFCHG */

    curbp->b_mode &= ~MDVIEW; /* cleanup for subsequent tests */

    return TRUE;
}

static unsigned char test_filesave_writes_when_changed(void)
{
    BUFFER *bp;
    char fname[NFILEN] = "D:TESTFS.TXT";
    char rbuf[NLINE];
    unsigned char s;

    bp = bfind("savewrites", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    insert_text("Persisted by filesave");
    strcpy(curbp->b_fname, fname);
    ASSERT((curbp->b_flag & BFCHG) != 0);

    s = filesave(FALSE, 1);
    ASSERT(s == TRUE);
    ASSERT((curbp->b_flag & BFCHG) == 0); /* cleared on successful save */

    ASSERT(ffropen(fname) == FIOSUC);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "Persisted by filesave") == 0);
    ASSERT(ffgetline(rbuf, NLINE) == FIOEOF);
    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* dired(): the real ^X-D command -- bfind("*dired*")+dired_fill()+    */
/* swbuffer(), no mlreply() prompt involved.                           */
/* ------------------------------------------------------------------- */

static unsigned char test_dired_command_fills_and_switches(void)
{
    unsigned char s;
    unsigned char count = 0;
    LINE *lp;

    s = dired(FALSE, 1);
    ASSERT(s == TRUE);
    ASSERT(strcmp(curbp->b_bname, "*dired*") == 0);
    ASSERT(strcmp(curbp->b_fname, "D:") == 0);
    ASSERT((curbp->b_mode & MDVIEW) != 0);
    ASSERT((curbp->b_flag & BFDIR) != 0);

    lp = lforw(curbp->b_linep);
    ASSERT(lp != curbp->b_linep);
    while (lp != curbp->b_linep) {
        ASSERT(llength(lp) > 0);
        lp = lforw(lp);
        count++;
    }
    ASSERT(count >= 3);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* dired_fill(): real "D:*.*" directory listing.                       */
/* ------------------------------------------------------------------- */

static unsigned char test_dired_fill_lists_real_directory(void)
{
    BUFFER *bp;
    unsigned char s;
    unsigned char count = 0;
    LINE *lp;

    bp = bfind("*dired-test*", TRUE, 0);
    ASSERT(bp != NULL);

    s = dired_fill(bp);
    ASSERT(s == TRUE);
    ASSERT((bp->b_mode & MDVIEW) != 0);
    ASSERT((bp->b_flag & BFDIR) != 0);
    ASSERT(bp->b_active == TRUE);
    ASSERT(strcmp(bp->b_fname, "D:") == 0);

    /* Don't assert exact names/order/count (disk contents legitimately
     * vary -- other suites and this one have already written real files
     * to this same disk by the time this test runs). Just confirm a
     * real listing of real, non-empty names came back: the disk this
     * booted from always has at least DOS.SYS/DUP.SYS/AUTORUN.SYS. */
    lp = lforw(bp->b_linep);
    ASSERT(lp != bp->b_linep);
    while (lp != bp->b_linep) {
        ASSERT(llength(lp) > 0);
        lp = lforw(lp);
        count++;
    }
    ASSERT(count >= 3);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* readin(): a single physical line far longer than NLINE (128) forces */
/* ffgetline() to return FIOLNG repeatedly, splitting it into several   */
/* truncated LINE records instead of one -- readin() must keep going    */
/* (not abort) and still finish with FIOEOF/TRUE.                       */
/* ------------------------------------------------------------------- */

static unsigned char test_readin_wraps_overlong_line(void)
{
    char fname[NFILEN] = "D:TESTLNG.TXT";
    /* static: avoids cc65's tiny __STACKSIZE__=0x100 (256-byte) shared
     * software parameter stack -- readin() below ALSO has its own
     * 128-byte `line[NLINE]` automatic local live at the same time, so
     * an automatic array here would risk corrupting unrelated memory.
     * Kept STRICTLY under 256 bytes for a second, independent reason:
     * src/fileio.c's ffputline() loops with `unsigned char i` against
     * an `int nbuf` -- passing nbuf > 255 makes `i` wrap and never
     * reach nbuf, an actual (if dormant) infinite-loop bug in
     * production code that's simply never triggered there because
     * NLINE=128 keeps every real line under 256 bytes. Confirmed by
     * hitting it firsthand with a 300-byte buffer here. Not fixed in
     * src/fileio.c since the real application never exercises this
     * path; kept well clear of it (200 bytes) instead. */
    static char longbuf[200];
    BUFFER *bp;
    unsigned char s;
    LINE *lp;
    int total;

    memset(longbuf, 'A', sizeof(longbuf));

    ASSERT(ffwopen(fname) == FIOSUC);
    ASSERT(ffputline(longbuf, sizeof(longbuf)) == FIOSUC); /* one 200-char line + '\n' */
    ASSERT(ffclose() == FIOSUC);

    bp = bfind("readlong", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);

    s = readin(fname, FALSE);
    ASSERT(s == TRUE); /* FIOLNG mid-stream must not turn into FALSE */

    /* NLINE=128, so the FIOLNG chunk is 127 bytes (126 normal + 1
     * overflow byte), then the 73-byte remainder finishes normally:
     * 127 + 73 = 200. */
    total = 0;
    lp = lforw(curbp->b_linep);
    while (lp != curbp->b_linep) {
        total += llength(lp);
        lp = lforw(lp);
    }
    ASSERT(total == 200);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* writeout(): a bad device qualifier makes ffwopen() itself fail      */
/* (FIOERR), exercising writeout()'s own error return -- no OOM or      */
/* mid-write I/O fault required.                                        */
/* ------------------------------------------------------------------- */

static unsigned char test_writeout_reports_error_on_bad_device(void)
{
    BUFFER *bp;
    unsigned char s;

    bp = bfind("writebad", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    insert_text("never actually written");

    s = writeout("Q:NODEV.TXT"); /* "Q:" is not a live CIO device here */
    ASSERT(s == FALSE);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn file2_tests[NUM_FILE2_TESTS] = {
    test_filesave_skips_when_no_changes,
    test_filesave_reports_no_fname,
    test_filesave_blocked_in_readonly_mode,
    test_filesave_writes_when_changed,
    test_dired_command_fills_and_switches,
    test_dired_fill_lists_real_directory,
    test_readin_wraps_overlong_line,
    test_writeout_reports_error_on_bad_device,
};
#pragma rodata-name (pop)
