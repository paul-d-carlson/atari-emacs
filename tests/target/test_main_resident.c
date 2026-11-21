/*
 * test_main_resident.c
 *
 * Entry point ("composition root") for the disk-overlay test
 * architecture's single resident 6502 test image. There is only ONE
 * resident image: it links the full, real
 * application code (buffer/display/file/fileio/line/window/atari/
 * termio/basic/random/bind/search.c) plus this test harness ONCE, and boots
 * ONCE. Each suite's actual test functions (test_buffer.c, test_atari.c,
 * test_basic.c, test_bind.c, test_display.c) are instead compiled as
 * separate, tiny "overlay" images (see test-overlay.cfg) that this file
 * loads one at a time, from real D: disk files, into a small shared
 * fixed-address window (see abi_table.h / overlay_loader.c), runs to
 * completion, then discards before loading the next.
 *
 * This is what actually fixes the heap-pressure problem the prior
 * split-binary architecture worked around by duplicating the full app
 * image per binary: the (much larger) application code and its heap
 * are resident and shared across every suite, while only a few KB
 * window is ever reserved for "whichever suite's test code is
 * currently running".
 *
 * Results are reported through the same fixed memory address as
 * before (tests/common/test_protocol.h), read directly out of emulated
 * RAM by the native driver (tests/driver/test_runner_resident.c).
 */

/* make global definitions not external, same trick src/main.c uses */
#define maindef

#include <string.h>

#include "estruct.h"
#include "efunc.h"
#include "edef.h"
#include "harness.h"
#include "harness_common.h"
#include "abi_table.h"
#include "overlay_loader.h"
#include "test_buffer.h"
#include "test_atari.h"
#include "test_basic.h"
#include "test_bind.h"
#include "test_display.h"
#include "test_display2.h"
#include "test_file1.h"
#include "test_file2.h"
#include "test_file3.h"
#include "test_fileio.h"
#include "test_line1.h"
#include "test_line2.h"
#include "test_random1.h"
#include "test_random2.h"
#include "test_random3.h"
#include "test_search.h"
#include "test_window.h"
#include "test_termio.h"

/* Runs every test in the suite currently sitting in the overlay window,
 * starting at OVERLAY_BASE + skip_bytes (skip_bytes is 2 for the one
 * suite -- basic.c -- whose overlay places a one-off setup function
 * pointer before its test_fn table; 0 for every other suite). */
static void run_overlay_tests(unsigned char count, unsigned char skip_bytes, unsigned char *idx)
{
    const test_fn *tests = (const test_fn *)(OVERLAY_BASE + skip_bytes);
    unsigned char i;

    for (i = 0; i < count; i++, (*idx)++) {
        test_begin(*idx);
        test_record(*idx, tests[i]());
    }
}

/* Switch back to the one permanent "main" buffer created by
 * test_edinit() at startup, then reclaim everything else.
 * free_other_buffers()/zotbuf() can never remove whatever buffer is
 * currently DISPLAYED, so a suite that creates its own named buffers
 * (via bfind()+swbuffer()) always leaves the LAST one it created as
 * curbp -- that lone buffer would otherwise ride along unreclaimed all
 * the way to the next suite that creates a buffer of its own, which is
 * exactly what corrupted the driver's per-suite heap "baseline" samples
 * and caused the (false-positive) "POSSIBLE LEAK" drift warning once
 * src/line.c became the last suite in the run (see README.md). "main"
 * has an empty b_fname, so swbuffer()'s clean-named-buffer eviction
 * path (which would call bclear()) never triggers for it, and it's
 * always still around to switch back to. Calling this even when
 * "main" is already curbp (a suite that created no buffers of its own)
 * is safe: swbuffer(bp) with bp==curbp just decrements+re-increments
 * b_nwnd back to the same value with no side effects. */
static void reset_buffers(void)
{
    BUFFER *anchor = bfind("main", TRUE, 0);
    if (anchor != NULL)
        swbuffer(anchor);
    free_other_buffers();
}

int main(void)
{
    unsigned char idx;
    char bname[NBUFN];

    /* Zero the result block and mark it live. */
    memset(results, 0, sizeof(test_results_t));
    results->magic = RESULTS_MAGIC;

    /* Same minimal init sequence as src/main.c: vtinit() then edinit(). */
    strcpy(bname, "main");
    vtinit();
    test_edinit(bname);

    idx = 0;

    if (overlay_load("D:OVBUF.BIN")) {
        run_overlay_tests(NUM_BUFFER_TESTS, 0, &idx);
    } else {
        idx += NUM_BUFFER_TESTS; /* leave these TEST_NOT_RUN/SKIP */
    }
    reset_buffers();

    if (overlay_load("D:OVATA.BIN")) {
        run_overlay_tests(NUM_ATARI_TESTS, 0, &idx);
    } else {
        idx += NUM_ATARI_TESTS;
    }
    reset_buffers();

    if (overlay_load("D:OVBAS.BIN")) {
        /* basic.c's overlay places a `void (*)(void)` setup-function
         * pointer (build_basic_test_buffer) in front of its test_fn
         * table -- see test_basic.c. Call it once, then skip its 2
         * bytes when dispatching the real tests. */
        ((void (*)(void))(*(void **)OVERLAY_BASE))();
        run_overlay_tests(NUM_BASIC_TESTS, 2, &idx);
    } else {
        idx += NUM_BASIC_TESTS;
    }
    reset_buffers();

    if (overlay_load("D:OVBND.BIN")) {
        run_overlay_tests(NUM_BIND_TESTS, 0, &idx);
    } else {
        idx += NUM_BIND_TESTS;
    }
    reset_buffers();

    /* src/display.c suite, split across two overlays (same rationale as
     * the file.c/line.c/random.c splits -- see test_display2.c's
     * header): the reset_buffers() between halves resets the heap
     * high-water mark right before test_display2.c's first test
     * (update_single_line_edit_fast_path), the test a heap-corruption
     * regression previously hung/crashed inside. */
    if (overlay_load("D:OVDSP.BIN")) {
        run_overlay_tests(NUM_DISPLAY_TESTS, 0, &idx);
    } else {
        idx += NUM_DISPLAY_TESTS;
    }
    reset_buffers();

    if (overlay_load("D:OVDS2.BIN")) {
        run_overlay_tests(NUM_DISPLAY2_TESTS, 0, &idx);
    } else {
        idx += NUM_DISPLAY2_TESTS;
    }
    reset_buffers();

    /* The file suite is split across TWO overlays (see test_file1.c's
     * header): the free_other_buffers() between them resets the heap
     * high-water mark halfway through, keeping the file tests' deep-call
     * software-stack overrun off the live top-of-heap buffer. */
    if (overlay_load("D:OVFIL1.BIN")) {
        run_overlay_tests(NUM_FILE1_TESTS, 0, &idx);
    } else {
        idx += NUM_FILE1_TESTS;
    }
    reset_buffers();

    if (overlay_load("D:OVFIL2.BIN")) {
        run_overlay_tests(NUM_FILE2_TESTS, 0, &idx);
    } else {
        idx += NUM_FILE2_TESTS;
    }
    reset_buffers();

    /* Third file overlay: the interactive mlreply()-prompted wrappers
     * (fileread/filefind/filewrite/filename). These do real disk I/O
     * after reading their filename, so each one's keystrokes are gated
     * on its own test index (see tests/driver/test_runner_resident.c);
     * streaming them ungated once looked like a keyboard hang after
     * disk I/O. */
    if (overlay_load("D:OVFIL3.BIN")) {
        run_overlay_tests(NUM_FILE3_TESTS, 0, &idx);
    } else {
        idx += NUM_FILE3_TESTS;
    }

    /* src/fileio.c suite: exercises ffropen/ffwopen/ffputline/ffgetline/
     * ffclose directly, not just indirectly through file.c's wrappers. */
    if (overlay_load("D:OVFIO.BIN")) {
        run_overlay_tests(NUM_FILEIO_TESTS, 0, &idx);
    } else {
        idx += NUM_FILEIO_TESTS;
    }
    reset_buffers();

    /* src/line.c suite, split across two overlays (same rationale as
     * test_file1.c/test_file2.c -- see that header): each test builds
     * its own fresh buffer, so the free_other_buffers() between halves
     * resets the heap high-water mark halfway through. */
    if (overlay_load("D:OVLIN1.BIN")) {
        run_overlay_tests(NUM_LINE1_TESTS, 0, &idx);
    } else {
        idx += NUM_LINE1_TESTS;
    }
    reset_buffers();

    if (overlay_load("D:OVLIN2.BIN")) {
        run_overlay_tests(NUM_LINE2_TESTS, 0, &idx);
    } else {
        idx += NUM_LINE2_TESTS;
    }
    reset_buffers();

    /* src/random.c suite, split across THREE overlays (same rationale
     * as the line.c/file.c splits above; random2 was further split into
     * random2/random3 -- see test_random3.c's header -- once it became
     * the single largest overlay in the whole suite). */
    if (overlay_load("D:OVRAN1.BIN")) {
        run_overlay_tests(NUM_RANDOM1_TESTS, 0, &idx);
    } else {
        idx += NUM_RANDOM1_TESTS;
    }
    reset_buffers();

    if (overlay_load("D:OVRAN2.BIN")) {
        run_overlay_tests(NUM_RANDOM2_TESTS, 0, &idx);
    } else {
        idx += NUM_RANDOM2_TESTS;
    }
    reset_buffers();

    if (overlay_load("D:OVRAN3.BIN")) {
        run_overlay_tests(NUM_RANDOM3_TESTS, 0, &idx);
    } else {
        idx += NUM_RANDOM3_TESTS;
    }
    reset_buffers();

    /* src/search.c suite: eq()/expandp()/forscan() directly, plus
     * readpattern()/forwsearch() via scripted keystrokes. */
    if (overlay_load("D:OVSCH.BIN")) {
        run_overlay_tests(NUM_SEARCH_TESTS, 0, &idx);
    } else {
        idx += NUM_SEARCH_TESTS;
    }
    reset_buffers();

    /* src/window.c suite: reposition()/refresh(), plain curwp/sgarbf
     * flag-setters, no buffers of its own. */
    if (overlay_load("D:OVWIN.BIN")) {
        run_overlay_tests(NUM_WINDOW_TESTS, 0, &idx);
    } else {
        idx += NUM_WINDOW_TESTS;
    }
    reset_buffers();

    /* src/termio.c suite: ttopen/ttclose/ttflush/ttputc directly, plus
     * ttgetc()/ttgetc_noshow()/typahead() via scripted keystrokes. */
    if (overlay_load("D:OVTIO.BIN")) {
        run_overlay_tests(NUM_TERMIO_TESTS, 0, &idx);
    } else {
        idx += NUM_TERMIO_TESTS;
    }
    reset_buffers();

    results->done = 1;

    for (;;)
        ;                              /* halt; driver detects done==1 */
    return 0;                          /* unreachable; silences cc65's
                                         * "control reaches end of
                                         * non-void function" warning */
}
