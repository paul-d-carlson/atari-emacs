/*
 * test_file1.c
 *
 * First half of the src/file.c unit tests (see also test_file2.c). Both
 * halves test the REAL src/file.c, linked with its real collaborators
 * (fileio.c, buffer.c, line.c, display.c). No source file under src/ is
 * modified or duplicated. Like test_bind.c, this suite boots a real
 * Atari DOS 2.5 disk image, so there is a genuine "D:" CIO device --
 * every file read/write below is real disk I/O, with no mocking.
 *
 * WHY THE FILE SUITE IS SPLIT INTO TWO OVERLAYS (test_file1 + test_file2)
 * ---------------------------------------------------------------------
 * cc65's software parameter stack is only __STACKSIZE__ = 0x100 (256)
 * bytes, sitting just ABOVE the malloc heap (stack grows DOWN from
 * ~$9B9F toward the heap ceiling at ~$9AA0). Several file.c code paths
 * carry large automatic locals down deep call chains -- notably
 * readin()'s own `char line[NLINE=128]` and dired_fill()'s opendir/CIO
 * path -- so a single file test can momentarily push the software stack
 * ~350 bytes deep, i.e. ~90 bytes PAST its 256-byte budget.
 *
 * That overrun is only harmful if it lands on a live heap object. Each
 * file test also leaves a named buffer (+ its lines) allocated, so as
 * the suite runs the heap high-water mark climbs toward the stack floor.
 * With all 16 file tests packed into ONE overlay, by the ~9th test the
 * top-of-heap buffer sits right under the stack floor and the next deep
 * call's stack overrun corrupts that buffer's b_bufp link -- turning the
 * buffer list cyclic and hanging the next bfind()/lfree() walk forever.
 * (Confirmed firsthand: original 9-test suite passed; 16 tests hung in
 * bfind at the 9th, on a buffer trashed at the very top of the heap.)
 *
 * The fix leverages test_main_resident.c's existing free_other_buffers()
 * call BETWEEN every overlay: splitting the 16 tests across two overlays
 * inserts a heap-high-water RESET halfway through. Each half then behaves
 * like the known-good <=9-test suite -- the heap top stays well below the
 * stack floor, so the (still-present) deep-call stack overrun lands on
 * dead heap space instead of a live buffer. This trades zero heap away
 * from the display suite (unlike simply enlarging __STACKSIZE__, which
 * starves display.c's own buffer-heavy tests).
 *
 * Only makename()/getfile()/readin()/writeout() are exercised here;
 * filesave()/dired()/dired_fill() and the readin()/writeout() edge cases
 * live in test_file2.c. fileread(), filefind(), filewrite(), and
 * filename() are thin mlreply()-prompted wrappers (see test_file2.c's
 * header) and are intentionally not re-tested.
 *
 * Order matters here: test_getfile_reactivates_evicted_buffer_with_
 * fresh_content depends on the buffer test_getfile_creates_new_buffer_
 * and_reads_file left behind, and test_readin_replaces_buffer_content
 * deliberately performs two successive readin() calls on one buffer.
 * Both dependent pairs are kept together in THIS overlay.
 *
 * GOTCHA this suite deliberately exercises: src/buffer.c's swbuffer()
 * EVICTS any clean, named (non-BFTEMP) buffer being switched AWAY FROM
 * -- it bclear()s its lines and sets b_active=FALSE to conserve heap,
 * relying on readin() to transparently restore it if ever revisited.
 * So getfile()'s "[Exists]" fast path does NOT generally mean "skip
 * re-reading": if the buffer was evicted in between, revisiting it via
 * getfile()/swbuffer() re-reads current on-disk content, not stale
 * in-memory content. */

#include "resident_api.h"
#include "harness.h"
#include "test_file1.h"

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
/* makename(): pure path -> buffer-name logic, no disk/buffer state.    */
/* ------------------------------------------------------------------- */

static unsigned char test_makename_strips_directory_prefix(void)
{
    char fname[] = "/dir/sub/foo.c";
    char bname[NBUFN];

    makename(bname, fname);
    ASSERT(strcmp(bname, "foo.c") == 0);

    return TRUE;
}

static unsigned char test_makename_stops_at_semicolon_version_suffix(void)
{
    char fname[] = "FILE.TXT;3";
    char bname[NBUFN];

    makename(bname, fname);
    ASSERT(strcmp(bname, "FILE.TXT") == 0);

    return TRUE;
}

static unsigned char test_makename_truncates_long_names(void)
{
    char fname[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; /* no '/' or ';' */
    char bname[NBUFN];

    makename(bname, fname);
    ASSERT(strlen(bname) == NBUFN - 1);
    ASSERT(strncmp(bname, fname, NBUFN - 1) == 0);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* getfile(): new-buffer-creation path, then already-open path.        */
/* ------------------------------------------------------------------- */

static BUFFER *g_getfile_bp;
static char g_getfile_fname[NFILEN];

static unsigned char test_getfile_creates_new_buffer_and_reads_file(void)
{
    char fname[NFILEN] = "D:TESTGF.TXT";
    unsigned char s;
    LINE *lp;

    ASSERT(ffwopen(fname) == FIOSUC);
    ASSERT(ffputline("Alpha line one", 14) == FIOSUC);
    ASSERT(ffputline("Beta line two", 13) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    s = getfile(fname, FALSE);
    ASSERT(s == TRUE);
    ASSERT(curbp != NULL);
    ASSERT(strcmp(curbp->b_fname, fname) == 0);
    /* No '/' in an Atari "D:name" path, so makename() keeps it verbatim
     * as the buffer name too. */
    ASSERT(strcmp(curbp->b_bname, fname) == 0);

    lp = lforw(curbp->b_linep);
    ASSERT(lp != curbp->b_linep);
    ASSERT(line_matches(lp, "Alpha line one"));

    lp = lforw(lp);
    ASSERT(lp != curbp->b_linep);
    ASSERT(line_matches(lp, "Beta line two"));

    lp = lforw(lp);
    ASSERT(lp == curbp->b_linep); /* exactly two lines, nothing more */

    g_getfile_bp = curbp;
    strcpy(g_getfile_fname, fname);

    return TRUE;
}

static unsigned char test_getfile_reactivates_evicted_buffer_with_fresh_content(void)
{
    BUFFER *scratch;
    unsigned char s;
    LINE *lp;

    /* Switching to any other buffer evicts g_getfile_bp (see swbuffer()'s
     * "Evict clean named buffers" comment): it's clean and has a real
     * fname, so its lines are freed and b_active goes FALSE. */
    scratch = bfind("filescratch", TRUE, 0);
    ASSERT(scratch != NULL);
    ASSERT(swbuffer(scratch) == TRUE);
    ASSERT(g_getfile_bp->b_active == FALSE);
    ASSERT(lforw(g_getfile_bp->b_linep) == g_getfile_bp->b_linep);

    /* Rewrite the SAME on-disk file with different content. */
    ASSERT(ffwopen(g_getfile_fname) == FIOSUC);
    ASSERT(ffputline("Zulu changed content", 21) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    /* getfile() finds the SAME buffer object by fname (no new buffer
     * created), and swbuffer() transparently re-reads it from disk
     * because it was evicted -- so the reactivated content must be the
     * NEW on-disk content, not the stale original. */
    s = getfile(g_getfile_fname, FALSE);
    ASSERT(s == TRUE);
    ASSERT(curbp == g_getfile_bp);
    ASSERT(curbp->b_active == TRUE);

    lp = lforw(curbp->b_linep);
    ASSERT(lp != curbp->b_linep);
    ASSERT(line_matches(lp, "Zulu changed content"));
    lp = lforw(lp);
    ASSERT(lp == curbp->b_linep); /* exactly one line now */

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* readin(): missing-file path, then real replace-content path.        */
/* ------------------------------------------------------------------- */

static unsigned char test_readin_reports_new_for_missing_file(void)
{
    char fname[NFILEN] = "D:NOPE1.TXT";
    BUFFER *bp;
    unsigned char s;

    bp = bfind("readnew", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);

    s = readin(fname, FALSE);
    ASSERT(s == FALSE); /* FIOFNF path: readin() itself returns FALSE  */
    ASSERT(strcmp(curbp->b_fname, fname) == 0); /* name recorded anyway */
    ASSERT(lforw(curbp->b_linep) == curbp->b_linep); /* left empty      */

    return TRUE;
}

static unsigned char test_readin_replaces_buffer_content(void)
{
    char fnameA[NFILEN] = "D:TESTRA.TXT";
    char fnameB[NFILEN] = "D:TESTRB.TXT";
    BUFFER *bp;
    unsigned char s;
    LINE *lp;

    ASSERT(ffwopen(fnameA) == FIOSUC);
    ASSERT(ffputline("First file content", 19) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    ASSERT(ffwopen(fnameB) == FIOSUC);
    ASSERT(ffputline("Second file replaces", 20) == FIOSUC);
    ASSERT(ffputline("Now two lines", 13) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    bp = bfind("readreplace", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);

    s = readin(fnameA, FALSE);
    ASSERT(s == TRUE);
    lp = lforw(curbp->b_linep);
    ASSERT(lp != curbp->b_linep);
    ASSERT(line_matches(lp, "First file content"));
    lp = lforw(lp);
    ASSERT(lp == curbp->b_linep); /* just one line so far */

    /* A second readin() on the SAME buffer must REPLACE, not append. */
    s = readin(fnameB, FALSE);
    ASSERT(s == TRUE);
    ASSERT(strcmp(curbp->b_fname, fnameB) == 0);

    lp = lforw(curbp->b_linep);
    ASSERT(lp != curbp->b_linep);
    ASSERT(line_matches(lp, "Second file replaces"));
    lp = lforw(lp);
    ASSERT(lp != curbp->b_linep);
    ASSERT(line_matches(lp, "Now two lines"));
    lp = lforw(lp);
    ASSERT(lp == curbp->b_linep); /* exactly two lines: old content gone */

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* writeout(): real buffer -> real file, verified by reading it back.  */
/* ------------------------------------------------------------------- */

static unsigned char test_writeout_writes_buffer_content_to_disk(void)
{
    BUFFER *bp;
    char fname[NFILEN] = "D:TESTWO.TXT";
    char rbuf[NLINE];
    unsigned char s;

    bp = bfind("writetest", TRUE, 0);
    ASSERT(bp != NULL);
    ASSERT(swbuffer(bp) == TRUE);
    insert_text("Hello world\nSecond line here");

    s = writeout(fname);
    ASSERT(s == TRUE);

    /* Read it back with the same real disk primitives readin()/getfile()
     * use, independent of writeout()'s own internals. */
    ASSERT(ffropen(fname) == FIOSUC);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "Hello world") == 0);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "Second line here") == 0);
    ASSERT(ffgetline(rbuf, NLINE) == FIOEOF); /* exactly two lines */
    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn file1_tests[NUM_FILE1_TESTS] = {
    test_makename_strips_directory_prefix,
    test_makename_stops_at_semicolon_version_suffix,
    test_makename_truncates_long_names,
    test_getfile_creates_new_buffer_and_reads_file,
    test_getfile_reactivates_evicted_buffer_with_fresh_content,
    test_readin_reports_new_for_missing_file,
    test_readin_replaces_buffer_content,
    test_writeout_writes_buffer_content_to_disk,
};
#pragma rodata-name (pop)
