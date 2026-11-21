/*
 * test_search.c
 *
 * src/search.c unit tests: eq(), expandp(), forscan(), readpattern(),
 * and forwsearch().
 *
 * src/search.c has no header of its own (no search.h) -- PTBEG/PTEND
 * are #defined locally inside search.c itself, not exposed anywhere
 * else. Mirrored here as plain literals with the same names/values,
 * purely for readability at forscan() call sites; they are NOT the same
 * preprocessor symbols as search.c's (there is no way to share them
 * without a shared header, and adding one just for two constants isn't
 * worth it).
 *
 * SELF-CONTAINED BY DESIGN: every test builds its own fresh buffer
 * (bfind + swbuffer), matching the random.c/line.c suites' convention.
 *
 * readpattern()/forwsearch() are the only functions here that read a
 * keystroke (via mlreplyt(), same custom ESC(27) eolchar already proven
 * reliable by test_display.c's mlreplyt_echoes_ctrlchar_and_custom_eol)
 * -- so the 7 tests below that reach them need a real scripted
 * keystroke from the driver's keystroke feed (see
 * driver/test_runner_resident.c's SEARCH_GATE_TEST, gated on the first
 * of them). Unlike test_file3.c's disabled interactive tests, search.c
 * performs NO disk I/O between prompts, so all 7 safely share ONE
 * keystroke segment (matching test_display.c's 3 mlreplyt tests and
 * test_random1.c's 4 quote() tests, not test_file3.c's one-segment-
 * per-test workaround).
 *
 * Ordering requirement: test_readpattern_fails_when_blank_and_no_old_pattern
 * MUST run before any other test in this suite that sets the global
 * "pat" (readpattern()/forwsearch() are the ONLY functions anywhere in
 * the app that touch it, and no earlier suite in the whole test run
 * calls either) -- it relies on "pat" still holding its BSS zero-init
 * value. It is therefore placed as the FIRST of the interactive block;
 * every non-interactive test above it (eq/expandp/forscan) never
 * touches "pat", so ordering them first is safe.
 *
 * Known, accepted, permanent gap (see tests/README.md): forwsearch()
 * with a negative n never enters its `while (n-- > 0)` loop, so its
 * local `status` variable is read uninitialized before being returned/
 * passed to mlwrite() -- undefined behavior in the real application,
 * not something a deterministic test can exercise meaningfully.
 */

#include "resident_api.h"
#include "harness.h"
#include "test_search.h"

#define PTBEG 1
#define PTEND 2

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
/* eq(): case-folding character compare, gated by MDEXACT.              */
/* ------------------------------------------------------------------- */

static unsigned char test_eq_case_insensitive_matches_by_default(void)
{
    BUFFER *bp = bfind("srcha", TRUE, 0);

    swbuffer(bp);
    curbp->b_mode &= ~MDEXACT;

    ASSERT(eq('a', 'A') == TRUE);
    ASSERT(eq('A', 'a') == TRUE);
    ASSERT(eq('a', 'b') == FALSE);
    ASSERT(eq('Z', 'z') == TRUE);

    return TRUE;
}

static unsigned char test_eq_exact_mode_distinguishes_case(void)
{
    BUFFER *bp = bfind("srchb", TRUE, 0);

    swbuffer(bp);
    curbp->b_mode |= MDEXACT;

    ASSERT(eq('a', 'A') == FALSE);
    ASSERT(eq('A', 'A') == TRUE);
    ASSERT(eq('a', 'a') == TRUE);

    curbp->b_mode &= ~MDEXACT;
    return TRUE;
}

/* ------------------------------------------------------------------- */
/* expandp(): control-key/newline/percent expansion for display, with  */
/* a maxlength truncation guard.                                       */
/* ------------------------------------------------------------------- */

static unsigned char test_expandp_passes_plain_text_unchanged(void)
{
    char dest[32];

    ASSERT(expandp("hello", dest, 30) == TRUE);
    ASSERT(strcmp(dest, "hello") == 0);

    return TRUE;
}

static unsigned char test_expandp_expands_newline_and_percent(void)
{
    char dest[32];

    ASSERT(expandp("a\nb%c", dest, 30) == TRUE);
    ASSERT(strcmp(dest, "a<NL>b%%c") == 0);

    return TRUE;
}

static unsigned char test_expandp_expands_control_and_del_chars(void)
{
    char dest[32];
    char src[3];

    src[0] = 0x01; /* ^A */
    src[1] = 0x7f; /* DEL, expands the same way as any control char */
    src[2] = 0;

    ASSERT(expandp(src, dest, 30) == TRUE);
    ASSERT(strcmp(dest, "^A^?") == 0);

    return TRUE;
}

static unsigned char test_expandp_truncates_when_maxlength_exceeded(void)
{
    char dest[16];

    /* First char is a control char: costs 2 output bytes and 2 off
     * maxlength (5 -> 3). 3 < 4, so expandp writes a terminating '$'
     * and returns FALSE without ever reaching the second source char. */
    ASSERT(expandp("\x01\x01", dest, 5) == FALSE);
    ASSERT(strcmp(dest, "^A$") == 0);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* forscan(): the non-interactive scan/match engine forwsearch() calls. */
/* ------------------------------------------------------------------- */

static unsigned char test_forscan_finds_match_leaving_point_at_end(void)
{
    BUFFER *bp = bfind("srchc", TRUE, 0);
    LINE *firstline;

    swbuffer(bp);
    insert_text("hello world");
    firstline = lforw(bp->b_linep);
    gotobob(FALSE, 0);

    ASSERT(forscan("world", PTEND) == TRUE);
    ASSERT(curwp->w_dotp == firstline);
    ASSERT(curwp->w_doto == 11);

    return TRUE;
}

static unsigned char test_forscan_finds_match_leaving_point_at_begin(void)
{
    BUFFER *bp = bfind("srchd", TRUE, 0);
    LINE *firstline;

    swbuffer(bp);
    insert_text("hello world");
    firstline = lforw(bp->b_linep);
    gotobob(FALSE, 0);

    ASSERT(forscan("world", PTBEG) == TRUE);
    ASSERT(curwp->w_dotp == firstline);
    ASSERT(curwp->w_doto == 6);

    return TRUE;
}

static unsigned char test_forscan_matches_across_line_boundary(void)
{
    BUFFER *bp = bfind("srche", TRUE, 0);
    LINE *firstline, *secondline;

    swbuffer(bp);
    insert_text("foo\nbar");
    firstline = lforw(bp->b_linep);
    secondline = lforw(firstline);
    gotobob(FALSE, 0);

    /* pattern "oo\nba" crosses the line boundary: matches "oo" at the
     * end of "foo", the (synthesized) EOL <NL>, then "ba" at the start
     * of "bar". */
    ASSERT(forscan("oo\nba", PTEND) == TRUE);
    ASSERT(curwp->w_dotp == secondline);
    ASSERT(curwp->w_doto == 2);

    return TRUE;
}

static unsigned char test_forscan_returns_false_when_not_found(void)
{
    BUFFER *bp = bfind("srchf", TRUE, 0);

    swbuffer(bp);
    insert_text("hello");
    gotobob(FALSE, 0);

    ASSERT(forscan("xyz", PTEND) == FALSE);
    ASSERT(curwp->w_doto == 0); /* dot untouched on failure */

    return TRUE;
}

static unsigned char test_forscan_case_insensitive_by_default(void)
{
    BUFFER *bp = bfind("srchg", TRUE, 0);

    swbuffer(bp);
    insert_text("HELLO");
    gotobob(FALSE, 0);
    curbp->b_mode &= ~MDEXACT;

    ASSERT(forscan("hello", PTEND) == TRUE);

    return TRUE;
}

/* ------------------------------------------------------------------- */
/* readpattern() / forwsearch(): interactive, keystroke-driven. All 7  */
/* share ONE keystroke segment (see file header + driver's             */
/* SEARCH_GATE_TEST) since none of them perform any disk I/O.          */
/* ------------------------------------------------------------------- */

static unsigned char test_readpattern_fails_when_blank_and_no_old_pattern(void)
{
    /* Must run first: relies on "pat" still being BSS-zero (see file
     * header). Bare ESC -> mlreplyt returns FALSE with an empty buffer,
     * and pat[0] == 0 means readpattern has no old pattern to fall
     * back on, so it also returns FALSE. */
    ASSERT(readpattern("Search") == FALSE);
    ASSERT(pat[0] == 0);

    return TRUE;
}

static unsigned char test_readpattern_stores_new_pattern(void)
{
    /* Types "abc" then ESC -> a real reply, stored into "pat". */
    ASSERT(readpattern("Search") == TRUE);
    ASSERT(strcmp(pat, "abc") == 0);

    return TRUE;
}

static unsigned char test_readpattern_keeps_old_pattern_on_blank_input(void)
{
    /* Bare ESC again, but this time "pat" already holds "abc" from the
     * previous test -- readpattern() treats the blank reply as "keep
     * the old pattern" and returns TRUE without changing it. */
    ASSERT(readpattern("Search") == TRUE);
    ASSERT(strcmp(pat, "abc") == 0);

    return TRUE;
}

static unsigned char test_forwsearch_moves_dot_after_match(void)
{
    BUFFER *bp = bfind("srchh", TRUE, 0);

    swbuffer(bp);
    insert_text("hello world");
    gotobob(FALSE, 0);

    /* Types "world" then ESC. */
    ASSERT(forwsearch(FALSE, 0) == TRUE);
    ASSERT(strcmp(pat, "world") == 0);
    ASSERT(curwp->w_doto == 11);

    return TRUE;
}

static unsigned char test_forwsearch_repeats_n_times(void)
{
    BUFFER *bp = bfind("srchi", TRUE, 0);

    swbuffer(bp);
    insert_text("lalala");
    gotobob(FALSE, 0);

    /* Types "la" then ESC; forwsearch(f, 3) repeats forscan() 3 times
     * over the SAME "la" pattern, landing at the very end of the
     * buffer after the 3rd occurrence. */
    ASSERT(forwsearch(FALSE, 3) == TRUE);
    ASSERT(strcmp(pat, "la") == 0);
    ASSERT(curwp->w_doto == 6);

    return TRUE;
}

static unsigned char test_forwsearch_reports_not_found(void)
{
    BUFFER *bp = bfind("srchj", TRUE, 0);

    swbuffer(bp);
    insert_text("hello");
    gotobob(FALSE, 0);

    /* Types "zzz" then ESC; forscan() fails, forwsearch() reports
     * "Not found" and leaves dot untouched. */
    ASSERT(forwsearch(FALSE, 0) == FALSE);
    ASSERT(curwp->w_doto == 0);

    return TRUE;
}

static unsigned char test_forwsearch_aborts_on_ctrl_g(void)
{
    BUFFER *bp = bfind("srchk", TRUE, 0);

    swbuffer(bp);
    insert_text("hello");
    gotobob(FALSE, 0);

    /* Bare C-G: readpattern()'s mlreplyt() returns ABORT, which
     * forwsearch() propagates immediately without ever calling
     * forscan() or mlwrite("Not found"). */
    ASSERT(forwsearch(FALSE, 0) == ABORT);
    ASSERT(curwp->w_doto == 0);

    return TRUE;
}

/* Pinned to the OVLENTRY segment (see test-overlay.cfg) so this table
 * always lands at the very first byte of the overlay window
 * (OVERLAY_BASE) -- the fixed address test_main_resident.c calls
 * through, regardless of what other RODATA this suite also contains. */
#pragma rodata-name (push, "OVLENTRY")
const test_fn search_tests[NUM_SEARCH_TESTS] = {
    test_eq_case_insensitive_matches_by_default,
    test_eq_exact_mode_distinguishes_case,
    test_expandp_passes_plain_text_unchanged,
    test_expandp_expands_newline_and_percent,
    test_expandp_expands_control_and_del_chars,
    test_expandp_truncates_when_maxlength_exceeded,
    test_forscan_finds_match_leaving_point_at_end,
    test_forscan_finds_match_leaving_point_at_begin,
    test_forscan_matches_across_line_boundary,
    test_forscan_returns_false_when_not_found,
    test_forscan_case_insensitive_by_default,
    test_readpattern_fails_when_blank_and_no_old_pattern,
    test_readpattern_stores_new_pattern,
    test_readpattern_keeps_old_pattern_on_blank_input,
    test_forwsearch_moves_dot_after_match,
    test_forwsearch_repeats_n_times,
    test_forwsearch_reports_not_found,
    test_forwsearch_aborts_on_ctrl_g,
};
#pragma rodata-name (pop)
