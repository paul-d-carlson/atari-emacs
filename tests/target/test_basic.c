/*
 * test_basic.c
 *
 * Unit tests for the REAL src/basic.c cursor-motion functions, linked
 * together with its real collaborators (see tests/README.md). No
 * source file under src/ is modified or duplicated here.
 *
 * basic.c's functions all operate on the current window's dot (and, for
 * a couple, its mark) via the real curwp/curbp globals -- there is no
 * mocking of LINE/BUFFER/WINDOW structures. A dedicated buffer
 * ("basicmove") is built once, using the real linsert()/lnewline() from
 * line.c (the same primitives self-insert/newline key bindings use), so
 * every test below exercises real cursor motion over a real multi-line
 * buffer.
 *
 * Buffer layout built by build_basic_test_buffer() (captured as
 * g_line1..g_line6 for tests to assert against by identity):
 *   line1: "0123456789"          (10 chars, no tabs/control chars)
 *   line2: "abcde"               (5 chars -- shorter than line1/line3)
 *   line3: "ABCDEFGHIJKLMNOPQRST" (20 chars -- longer than line1/line2)
 *   line4: ""                    (empty line)
 *   line5: "xyzxyzxy"             (8 chars; buffer ends mid-line here)
 *   line6: "ab\tcd"               (contains a tab, for getgoal() only)
 */

#include "resident_api.h"
#include "harness.h"
#include "test_basic.h"

static LINE *g_line1, *g_line2, *g_line3, *g_line4_empty, *g_line5, *g_line6;

static void insert_text(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n')
            lnewline();
        else
            linsert(1, (unsigned char)*s);
    }
}

/* Mimics main.c's execute(): clears thisflag before calling fn, then
 * copies thisflag into lastflag afterward -- the bookkeeping basic.c's
 * forwline()/backline() rely on (via lastflag&CFCPCN) to know whether
 * the *previous* command was also a line-to-line move, and so whether
 * to keep the existing goal column instead of recomputing it. Calling
 * forwline()/backline() directly, without going through main.c's real
 * command dispatcher, means this driver has to do that bookkeeping
 * itself to get realistic goal-column behavior across repeated calls. */
static unsigned char run_cmd(unsigned char (*fn)(unsigned char, int), unsigned char f, int n)
{
    unsigned char status;

    thisflag = 0;
    status = fn(f, n);
    lastflag = thisflag;
    return status;
}

void build_basic_test_buffer(void)
{
    BUFFER *bp = bfind("basicmove", TRUE, 0);

    swbuffer(bp);
    insert_text("0123456789\nabcde\nABCDEFGHIJKLMNOPQRST\n\nxyzxyzxy\nab\tcd");

    g_line1       = lforw(curbp->b_linep);
    g_line2       = lforw(g_line1);
    g_line3       = lforw(g_line2);
    g_line4_empty = lforw(g_line3);
    g_line5       = lforw(g_line4_empty);
    g_line6       = lforw(g_line5);

    lastflag = 0;
    gotobob(FALSE, 0);
}

/* ------------------------------------------------------------------- */

static unsigned char test_gotobol_moves_to_start_of_line(void)
{
    gotobob(FALSE, 0);
    forwchar(FALSE, 4);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 4);

    gotobol(FALSE, 0);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 0);

    return TRUE;
}

static unsigned char test_gotoeol_moves_to_end_of_line(void)
{
    gotobob(FALSE, 0);
    gotoeol(FALSE, 0);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 10); /* llength */

    return TRUE;
}

static unsigned char test_forwchar_moves_within_line(void)
{
    gotobob(FALSE, 0);
    ASSERT(forwchar(FALSE, 3) == TRUE);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 3);

    return TRUE;
}

static unsigned char test_forwchar_crosses_line_boundary(void)
{
    gotobob(FALSE, 0);
    /* line1 is 10 chars; the 11th forward step must land on line2. */
    ASSERT(forwchar(FALSE, 11) == TRUE);
    ASSERT(curwp->w_dotp == g_line2 && curwp->w_doto == 0);

    return TRUE;
}

static unsigned char test_forwchar_fails_at_end_of_buffer(void)
{
    gotoeob(FALSE, 0);
    ASSERT(forwchar(FALSE, 1) == FALSE);

    return TRUE;
}

static unsigned char test_forwchar_negative_delegates_to_backchar(void)
{
    gotobob(FALSE, 0);
    forwchar(FALSE, 5);
    ASSERT(forwchar(FALSE, -5) == TRUE);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 0);

    return TRUE;
}

static unsigned char test_backchar_moves_within_line(void)
{
    gotobob(FALSE, 0);
    forwchar(FALSE, 5);
    ASSERT(backchar(FALSE, 2) == TRUE);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 3);

    return TRUE;
}

static unsigned char test_backchar_crosses_line_boundary(void)
{
    gotobob(FALSE, 0);
    forwchar(FALSE, 11);                    /* start of line2 */
    ASSERT(backchar(FALSE, 1) == TRUE);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 10); /* end of line1 */

    return TRUE;
}

static unsigned char test_backchar_fails_at_start_of_buffer(void)
{
    gotobob(FALSE, 0);
    ASSERT(backchar(FALSE, 1) == FALSE);

    return TRUE;
}

static unsigned char test_gotobob_moves_to_beginning_of_buffer(void)
{
    gotoeob(FALSE, 0);
    gotobob(FALSE, 0);
    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 0);

    return TRUE;
}

static unsigned char test_gotoeob_moves_to_end_of_buffer(void)
{
    gotobob(FALSE, 0);
    gotoeob(FALSE, 0);
    ASSERT(curwp->w_dotp == curbp->b_linep && curwp->w_doto == 0);

    return TRUE;
}

static unsigned char test_gotoline_jumps_to_requested_line(void)
{
    lastflag = 0; /* force curgoal to be recomputed from column 0 */
    ASSERT(gotoline(FALSE, 3) == TRUE);
    ASSERT(curwp->w_dotp == g_line3 && curwp->w_doto == 0);

    return TRUE;
}

static unsigned char test_gotoline_rejects_bogus_argument(void)
{
    ASSERT(gotoline(FALSE, 0) == FALSE);
    ASSERT(gotoline(FALSE, -5) == FALSE);

    return TRUE;
}

static unsigned char test_getgoal_expands_tabs(void)
{
    /* line6 == "ab\tcd": col 0='a'->1, 1='b'->2, 2='\t'->(2|7)+1==8.
     * Aiming for column 8 should land right after the tab (offset 3). */
    curgoal = 8;
    ASSERT(getgoal(g_line6) == 3);

    return TRUE;
}

static unsigned char test_forwline_backline_preserve_goal_column(void)
{
    gotobob(FALSE, 0);
    forwchar(FALSE, 7);              /* line1, column 7 */
    lastflag = 0;                    /* force curgoal recompute this time */

    ASSERT(run_cmd(forwline, FALSE, 1) == TRUE);
    /* line2 ("abcde") is only 5 chars -- goal column 7 is unreachable,
     * so dot lands at end-of-line instead. */
    ASSERT(curwp->w_dotp == g_line2 && curwp->w_doto == 5);

    /* Goal column (7) must be *preserved*, not recomputed from line2's
     * short offset, because lastflag now carries CFCPCN from the call
     * above (run_cmd() mimics main.c's execute() bookkeeping). */
    ASSERT(run_cmd(forwline, FALSE, 1) == TRUE);
    ASSERT(curwp->w_dotp == g_line3 && curwp->w_doto == 7);

    ASSERT(run_cmd(backline, FALSE, 1) == TRUE);
    ASSERT(curwp->w_dotp == g_line2 && curwp->w_doto == 5);

    return TRUE;
}

static unsigned char test_forwline_negative_delegates_to_backline(void)
{
    gotobob(FALSE, 0);
    lastflag = 0;
    run_cmd(forwline, FALSE, 2);             /* -> line3, column 0 */
    ASSERT(curwp->w_dotp == g_line3);

    ASSERT(run_cmd(forwline, FALSE, -2) == TRUE);
    ASSERT(curwp->w_dotp == g_line1);

    return TRUE;
}

static unsigned char test_forwpage_backpage_scroll_to_buffer_bounds(void)
{
    /* The test buffer only has 6 real lines -- far fewer than a
     * default page (term.t_nrow - 3), so forwpage() must run off the
     * end, and backpage() from there must run all the way back to the
     * first real line. Exercises both the f==FALSE default-scroll
     * branch and the f!=FALSE explicit-page-count branch. */
    gotobob(FALSE, 0);
    forwpage(FALSE, 0);
    ASSERT(curwp->w_linep == curbp->b_linep);
    ASSERT(curwp->w_dotp  == curbp->b_linep);

    backpage(FALSE, 0);
    ASSERT(curwp->w_linep == g_line1);
    ASSERT(curwp->w_dotp  == g_line1);

    forwpage(TRUE, 1);
    ASSERT(curwp->w_linep == curbp->b_linep);

    ASSERT(forwpage(TRUE, -1) == TRUE);      /* delegates to backpage() */
    ASSERT(curwp->w_linep == g_line1);

    return TRUE;
}

static unsigned char test_setmark_records_dot(void)
{
    gotobob(FALSE, 0);
    forwchar(FALSE, 3);
    ASSERT(setmark(FALSE, 0) == TRUE);
    ASSERT(curwp->w_markp == g_line1 && curwp->w_marko == 3);

    return TRUE;
}

static unsigned char test_swapmark_exchanges_dot_and_mark(void)
{
    gotobob(FALSE, 0);
    forwchar(FALSE, 2);
    setmark(FALSE, 0);               /* mark <- (line1, 2) */

    forwchar(FALSE, 9);              /* line1 has 8 chars left (10-2),
                                       * +1 more crosses to (line2, 0)  */
    ASSERT(swapmark(FALSE, 0) == TRUE);

    ASSERT(curwp->w_dotp == g_line1 && curwp->w_doto == 2);
    ASSERT(curwp->w_markp == g_line2 && curwp->w_marko == 0);

    return TRUE;
}

static unsigned char test_swapmark_fails_with_no_mark(void)
{
    curwp->w_markp = NULL;
    ASSERT(swapmark(FALSE, 0) == FALSE);

    return TRUE;
}

/* ------------------------------------------------------------------- */

/* Pinned to the OVLENTRY segment (see test-overlay.cfg), same as every
 * other suite's dispatch table, but with ONE extra function pointer
 * (build_basic_test_buffer) placed immediately before it in the SAME
 * segment -- test_main_resident.c calls slot 0 once as a `void (*)(void)`
 * setup routine, then dispatches basic_tests[] starting 2 bytes later.
 * ld65 lays out multiple declarations pushed into the same segment in
 * the order they're declared, so basic_setup_fn (declared first) is
 * guaranteed to land at OVERLAY_BASE, with basic_tests[] immediately
 * after it. */
#pragma rodata-name (push, "OVLENTRY")
void (* const basic_setup_fn)(void) = build_basic_test_buffer;
const test_fn basic_tests[NUM_BASIC_TESTS] = {
    test_gotobol_moves_to_start_of_line,
    test_gotoeol_moves_to_end_of_line,
    test_forwchar_moves_within_line,
    test_forwchar_crosses_line_boundary,
    test_forwchar_fails_at_end_of_buffer,
    test_forwchar_negative_delegates_to_backchar,
    test_backchar_moves_within_line,
    test_backchar_crosses_line_boundary,
    test_backchar_fails_at_start_of_buffer,
    test_gotobob_moves_to_beginning_of_buffer,
    test_gotoeob_moves_to_end_of_buffer,
    test_gotoline_jumps_to_requested_line,
    test_gotoline_rejects_bogus_argument,
    test_getgoal_expands_tabs,
    test_forwline_backline_preserve_goal_column,
    test_forwline_negative_delegates_to_backline,
    test_forwpage_backpage_scroll_to_buffer_bounds,
    test_setmark_records_dot,
    test_swapmark_exchanges_dot_and_mark,
    test_swapmark_fails_with_no_mark,
};
#pragma rodata-name (pop)
