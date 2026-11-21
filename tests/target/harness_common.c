/*
 * harness_common.c
 *
 * Shared setup/teardown helpers used by every test_main*.c entry point.
 * See harness_common.h for the rationale behind each piece; this file
 * used to be inlined directly into test_main.c before the test suite
 * was split into multiple independent binaries (see
 * tests/driver/harness_runner.h).
 */
#include <stdlib.h>

#include "estruct.h"
#include "efunc.h"
#include "edef.h"
#include "harness_common.h"

void test_edinit(char bname[])
{
    register BUFFER *bp;
    register WINDOW *wp;

    bp = bfind(bname, TRUE, 0);
    if (bp == NULL)
        exit(1);
    if ((wp = (WINDOW *)malloc(sizeof(WINDOW))) == NULL)
        exit(1);
    curbp  = bp;
    curwp  = wp;
    wp->w_bufp  = bp;
    bp->b_nwnd  = 1;
    wp->w_linep = bp->b_linep;
    wp->w_dotp  = bp->b_linep;
    wp->w_doto  = 0;
    wp->w_markp = NULL;
    wp->w_marko = 0;
    wp->w_force = 0;
    wp->w_flag  = WFMODE|WFHARD;
}

/*
 * Verbatim copy of src/main.c's ctrlg(). Linked in because display.c's
 * mlreplyt() calls it on the BELL/abort path (never exercised by our
 * scripted keystrokes, but the symbol must still resolve at link time).
 * Cannot be linked from main.c itself, because main.c defines its own
 * main() (which would collide with each test_main*.c's own main()) and
 * includes ebind.h's key-binding table (which would drag in the entire
 * command set). Copying this small, self-contained function is the one
 * place this test harness departs from "link the real .o file" -- but
 * the logic itself is real, unmodified, and unmocked, straight from
 * src/main.c.
 */
unsigned char ctrlg(unsigned char f, int n)
{
    (void)f; (void)n;
    (*term.t_beep)();
    mlwrite("[Abort]");
    return (ABORT);
}

/* Verbatim copy of src/main.c's rdonly(). Linked in because line.c/file.c/
 * random.c call it on their MDVIEW read-only checks (never exercised by
 * our tests, since we never set MDVIEW, but the symbol must still
 * resolve at link time). */
unsigned char rdonly(void)
{
    (*term.t_beep)();
    mlwrite("[Read only]");
    return (FALSE);
}

void free_other_buffers(void)
{
    BUFFER *bp = bheadp;
    BUFFER *next;

    while (bp != NULL) {
        next = bp->b_bufp;
        if (bp != curbp && bp->b_nwnd == 0) {
            bp->b_flag &= ~BFCHG;
            zotbuf(bp);
        }
        bp = next;
    }
}
