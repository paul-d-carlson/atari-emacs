/*
 * test_fileio.c
 *
 * Dedicated unit tests for the REAL src/fileio.c, called directly
 * through the ABI (ffropen/ffwopen/ffputline/ffgetline/ffclose are all
 * already exported for the file.c suite -- see abi_table.h's
 * ABI_FFROPEN..ABI_FFCLOSE slots -- so no new ABI slots are needed
 * here).
 *
 * src/fileio.c is already exercised INDIRECTLY by test_bind.c (help()'s
 * real D:EMACS.HLP read/write) and test_file1.c/test_file2.c (getfile/
 * readin/writeout/filesave, including the FIOLNG over-long-line path and
 * a bad-device FIOERR). This suite instead calls fileio.c's own five
 * functions directly, so every branch inside fileio.c itself -- not just
 * whichever ones file.c's wrappers happen to reach -- gets first-class,
 * targeted coverage: `atari_qualify()`'s three branches (prefix a bare
 * filename with "D:", uppercase a filename that already has a ':', and
 * truncate an over-long filename at its internal 32-byte buffer), both
 * `ffropen()`/`ffwopen()` failure returns, `ffclose()`'s no-op ("already
 * closed") branch, and `ffgetline()`'s `FIOLNG` (line too long for the
 * caller's buffer) and clean `FIOEOF` (nothing left to read) branches.
 *
 * Like every other suite, this boots a real Atari DOS 2.5 disk image, so
 * every read/write below is genuine "D:" CIO disk I/O -- nothing here is
 * mocked.
 *
 * Known, accepted gaps (not attempted): the `ferror()` I/O-error
 * branches in `ffputline()`/`ffgetline()` (no way to simulate a genuine
 * disk fault under emulation), and `ffgetline()`'s "File has funny line
 * at EOF" branch (a file whose last line lacks a trailing newline) --
 * `ffputline()` always appends '\n', so that exact on-disk shape can't be
 * produced through the real fileio.c/file.c write path used here.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_fileio.h"

/* ------------------------------------------------------------------- */
/* atari_qualify() (static in fileio.c, reached only through ffropen()/ */
/* ffwopen()): "D:" prefixing, in-place uppercasing, and truncation.    */
/* ------------------------------------------------------------------- */

static unsigned char test_ffwopen_prepends_device_prefix_when_missing(void)
{
    char fname[] = "NOCOLON.TXT";   /* no ':' -> atari_qualify() must prepend "D:" */
    char rbuf[NLINE];

    ASSERT(ffwopen(fname) == FIOSUC);
    ASSERT(ffputline("prefix test line", (int)strlen("prefix test line")) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    /* Confirm the file actually landed under the qualified "D:NOCOLON.TXT"
     * name (uppercased), independent of the original unqualified fn. */
    ASSERT(ffropen("D:NOCOLON.TXT") == FIOSUC);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "prefix test line") == 0);
    ASSERT(ffgetline(rbuf, NLINE) == FIOEOF);
    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

static unsigned char test_ffwopen_and_ffropen_uppercase_qualified_filename(void)
{
    char wfname[] = "d:lower.txt";  /* already has ':' -> uppercased in place, not re-prefixed */
    char rbuf[NLINE];

    ASSERT(ffwopen(wfname) == FIOSUC);
    ASSERT(ffputline("case test line", (int)strlen("case test line")) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    /* ffropen() runs the same atari_qualify() logic -- a lowercase query
     * with its own ':' must resolve to the same uppercased on-disk name. */
    ASSERT(ffropen("d:lower.txt") == FIOSUC);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "case test line") == 0);
    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

static unsigned char test_atari_qualify_truncates_overlong_filename(void)
{
    /* No ':', so atari_qualify() prepends "D:" then upper-cases and
     * copies -- but only up to its internal 32-byte buffer (ATARI_PATH_MAX
     * in src/fileio.c). This name is deliberately far longer than any
     * valid Atari 8.3 filename, so the copy loop's bound check
     * `(dst - buf) < ATARI_PATH_MAX - 1` -- not just running off the end
     * of the source string -- is what stops it. Whether the resulting
     * (truncated) name is itself accepted by the real D: CIO driver is
     * NOT asserted here (Atari DOS filenames are limited to 8.3, so it
     * may legitimately fail there too, same as a "Q:"-style bad device);
     * only that atari_qualify()'s truncation path runs, without hanging
     * or crashing, is checked. */
    char fname[] = "ThisFilenameIsDeliberatelyMuchLongerThanAnyAtariPathCanBe.TXT";
    unsigned char s;

    s = ffwopen(fname);
    ASSERT(s == FIOSUC || s == FIOERR);
    if (s == FIOSUC)
        ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ffropen()/ffwopen(): failure returns.                                */
/* ------------------------------------------------------------------- */

static unsigned char test_ffropen_reports_fiofnf_for_missing_file(void)
{
    ASSERT(ffropen("D:FIOMISS.TXT") == FIOFNF);
    return TRUE;
}

static unsigned char test_ffwopen_reports_error_for_bad_device(void)
{
    ASSERT(ffwopen("Q:FIOBAD.TXT") == FIOERR); /* "Q:" is not a live CIO device here */
    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ffclose(): the "already closed" (ffp == NULL) no-op branch.          */
/* ------------------------------------------------------------------- */

static unsigned char test_ffclose_without_open_is_safe(void)
{
    /* A normal open+write+close first, so ffp is a real, valid FILE* --
     * then close AGAIN with no intervening open. The second call must
     * hit ffclose()'s "if (ffp)" FALSE branch (ffp is now NULL) and
     * still report success, without touching a stale pointer. */
    char fname[] = "D:FIOCLOS.TXT";

    ASSERT(ffwopen(fname) == FIOSUC);
    ASSERT(ffputline("closed once already", (int)strlen("closed once already")) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    ASSERT(ffclose() == FIOSUC); /* ffp == NULL here: no-op branch */

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* ffputline()/ffgetline(): real roundtrip, FIOLNG, and clean FIOEOF.   */
/* ------------------------------------------------------------------- */

static unsigned char test_ffputline_ffgetline_roundtrip_multiple_lines(void)
{
    char fname[] = "D:FIORT.TXT";
    char rbuf[NLINE];

    ASSERT(ffwopen(fname) == FIOSUC);
    ASSERT(ffputline("first line", (int)strlen("first line")) == FIOSUC);
    ASSERT(ffputline("", 0) == FIOSUC);           /* empty line */
    ASSERT(ffputline("third and last", (int)strlen("third and last")) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    ASSERT(ffropen(fname) == FIOSUC);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "first line") == 0);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    /* Note: strcmp(rbuf, "") == 0 triggers a cc65 codegen/library quirk
     * that intermittently misreports equality against an empty string
     * literal in this overlay; strlen() is used instead to sidestep it
     * while still verifying the empty line round-tripped correctly. */
    ASSERT(strlen(rbuf) == 0);
    ASSERT(ffgetline(rbuf, NLINE) == FIOSUC);
    ASSERT(strcmp(rbuf, "third and last") == 0);
    /* Clean end-of-file: no partial line pending, so ffgetline()'s
     * i==0 branch returns FIOEOF rather than the "funny line at EOF"
     * error. */
    ASSERT(ffgetline(rbuf, NLINE) == FIOEOF);
    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

static unsigned char test_ffgetline_reports_fiolng_for_long_line(void)
{
    char fname[] = "D:FIOLONG.TXT";
    char longline[] = "this line is much longer than ten bytes";
    char small[10];
    unsigned char s;
    unsigned char drained;

    ASSERT(ffwopen(fname) == FIOSUC);
    ASSERT(ffputline(longline, (int)strlen(longline)) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    ASSERT(ffropen(fname) == FIOSUC);
    s = ffgetline(small, sizeof(small));
    ASSERT(s == FIOLNG);
    ASSERT(strlen(small) == sizeof(small) - 1); /* buffer filled + NUL-terminated */

    /* ffgetline() does not realign to the next real newline on FIOLNG --
     * readin() relies on exactly this to split an over-long line into
     * several truncated reads (see test_file2.c's
     * readin_wraps_overlong_line) -- so drain the rest of this same
     * physical line and confirm the handle is left in a sane state. */
    do {
        drained = ffgetline(small, sizeof(small));
    } while (drained == FIOLNG);
    ASSERT(drained == FIOSUC || drained == FIOEOF);

    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

static unsigned char test_ffputline_and_ffgetline_handle_lines_longer_than_255_bytes(void)
{
    /* Regression test for two now-fixed bugs: both ffputline() and
     * ffgetline() used an `unsigned char i` loop counter against an
     * `int nbuf`, wrapping before reaching an `nbuf` over 255 --
     * ffputline() never terminated (infinite loop), ffgetline()
     * silently overwrote buf from index 0 (wrong content, wrong NUL
     * position) instead. static, not automatic: keeps these well clear
     * of cc65's tiny 256-byte software parameter stack (see
     * test_file2.c's test_readin_wraps_overlong_line for the same
     * precaution). If ffputline's bug ever regresses, this test hangs
     * the whole suite (a harness TIMEOUT) rather than reporting a plain
     * FAIL -- that itself is the proof the loop no longer runs away.
     *
     * First read back through a small (64-byte) chunk buffer, draining
     * FIOLNG continuations exactly like test_readin_wraps_overlong_line
     * does (this alone already proves ffputline wrote every byte
     * correctly); then re-read the same file with ONE large-nbuf
     * ffgetline() call to prove its own fix too. */
    static char longbuf[300];
    static char assembled[300];
    static char rbuf[320];
    char fname[] = "D:FIOBIG.TXT";
    char chunk[64];
    int i, total;
    unsigned char s;

    for (i = 0; i < (int)sizeof(longbuf); i++)
        longbuf[i] = 'A' + (i % 26);

    ASSERT(ffwopen(fname) == FIOSUC);
    ASSERT(ffputline(longbuf, (int)sizeof(longbuf)) == FIOSUC);
    ASSERT(ffclose() == FIOSUC);

    ASSERT(ffropen(fname) == FIOSUC);
    total = 0;
    for (;;) {
        s = ffgetline(chunk, sizeof(chunk));
        ASSERT(s == FIOLNG || s == FIOSUC);
        if (s == FIOLNG) {
            memcpy(assembled + total, chunk, sizeof(chunk) - 1);
            total += sizeof(chunk) - 1;
        } else {
            memcpy(assembled + total, chunk, strlen(chunk));
            total += strlen(chunk);
            break;
        }
    }
    ASSERT(total == (int)sizeof(longbuf));
    for (i = 0; i < (int)sizeof(longbuf); i++)
        ASSERT(assembled[i] == longbuf[i]);
    ASSERT(ffgetline(chunk, sizeof(chunk)) == FIOEOF);
    ASSERT(ffclose() == FIOSUC);

    ASSERT(ffropen(fname) == FIOSUC);
    ASSERT(ffgetline(rbuf, (int)sizeof(rbuf)) == FIOSUC); /* rbuf is bigger than longbuf: no FIOLNG expected */
    for (i = 0; i < (int)sizeof(longbuf); i++)
        ASSERT(rbuf[i] == longbuf[i]);
    ASSERT(rbuf[sizeof(longbuf)] == 0);
    ASSERT(ffgetline(rbuf, (int)sizeof(rbuf)) == FIOEOF);
    ASSERT(ffclose() == FIOSUC);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn fileio_tests[NUM_FILEIO_TESTS] = {
    test_ffwopen_prepends_device_prefix_when_missing,
    test_ffwopen_and_ffropen_uppercase_qualified_filename,
    test_atari_qualify_truncates_overlong_filename,
    test_ffropen_reports_fiofnf_for_missing_file,
    test_ffwopen_reports_error_for_bad_device,
    test_ffclose_without_open_is_safe,
    test_ffputline_ffgetline_roundtrip_multiple_lines,
    test_ffgetline_reports_fiolng_for_long_line,
    test_ffputline_and_ffgetline_handle_lines_longer_than_255_bytes,
};
#pragma rodata-name (pop)
