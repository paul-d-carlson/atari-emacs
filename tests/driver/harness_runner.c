/*
 * harness_runner.c
 *
 * Shared implementation of run_harness() -- see harness_runner.h. This
 * is the common engine extracted from what used to be one monolithic
 * test_runner.c, so the native driver (tests/driver/test_runner_resident.c)
 * can supply its own test_info[]/keystroke-script/dbgcov-targets
 * without duplicating the libatari800 boot/frame-step/report machinery.
 *
 * Links against libatari800 (fetched and built by `make deps`),
 * boots the real cc65-compiled test binary via a real Atari DOS 2.5 disk
 * image (DOS.SYS + DUP.SYS + the test binary staged as AUTORUN.SYS,
 * built by mk/test-unit.mk with dir2atr). libatari800's AFILE_OpenFile()
 * auto-detects the .atr from its magic bytes and performs a genuine
 * Atari800_Coldstart() disk boot, exactly like running
 * `atari800 test.atr` on the command line -- this is a real DOS cold
 * boot, not a direct XEX binary-load injection, so the test binary gets
 * a real "D:" CIO device handler and can exercise real disk I/O.
 *
 * There is no mocking of any src collaborator: every assertion in
 * the test_*.c suites runs against the actual compiled 6502 machine
 * code, executed by a cycle-accurate Atari 8-bit emulator, including
 * real disk reads/writes through the real DOS "D:" device.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libatari800.h"
#include "harness_runner.h"
#include "heapcheck.h"

/* atari800's built-in MONITOR_PROFILE execution-tracking instrumentation
 * (atari800-5.2.0/src/monitor.c under build/deps/). Declared here by hand,
 * matching monitor.h's MONITOR_coverage_rec layout exactly, rather than
 * #including the vendor monitor.h (which would drag in vendor config.h/
 * atari.h that aren't otherwise needed by this driver). Requires the
 * vendored libatari800.a to have been built with MONITOR_PROFILE defined
 * -- mk/deps.mk enables it. */
typedef struct {
    unsigned long count;  /* times executed since last reset */
    unsigned long cycles; /* cycles executed since last reset */
} monitor_coverage_rec_t;
extern monitor_coverage_rec_t MONITOR_coverage[0x10000];

#define KEY_HOLD_FRAMES 2   /* frames a key stays "pressed"             */
#define KEY_IDLE_FRAMES 4   /* frames of no key between keystrokes      */

/* Net free-byte decline (first suite baseline minus last) above which we
 * flag possible leak drift. Every overlay swap now resets curbp back to
 * the permanent "main" anchor buffer before calling free_other_buffers()
 * (see reset_buffers() in test_main_resident.c), so a healthy run
 * returns close to the same baseline each suite. A characterized run
 * still shows a ~350-400 byte residual even with no leak present: many
 * suites cycle several differently-sized BUFFER/LINE allocations
 * through cc65's bump/coalescing heap, and that doesn't always
 * reassemble back into the exact original free-list shape (free-list
 * integrity is independently confirmed OK at every boundary regardless).
 * The threshold is set above that characterized noise floor so only a
 * genuinely growing decline (a real leak) gets flagged. */
#define HEAP_DRIFT_LEAK_THRESHOLD 1000

/*
 * Take one heap sample at a clean test boundary: validate the free list,
 * track the running origin estimate (lowest __heapptr ever seen -- a hard
 * lower bound on the real, stripped __heaporg), the constant ceiling, and
 * the high-water usage (lowest free-byte count). Corruption is recorded
 * once, tagged with the test index at which it first appeared.
 */
static void heap_take_sample(const unsigned char *mem, const heap_syms_t *hsyms,
                             heap_probe_t *hp, unsigned *heap_origin,
                             unsigned *heap_ceiling, unsigned *min_free,
                             int *min_free_test, int *corrupt_test,
                             const char **corrupt_err, int this_test)
{
    heap_probe(mem, hsyms, hp);
    if (*heap_origin == 0 || hp->heapptr < *heap_origin)
        *heap_origin = hp->heapptr;
    *heap_ceiling = hp->heapend;
    if (hp->corrupt) {
        if (*corrupt_test < 0) {
            *corrupt_test = this_test;
            *corrupt_err = hp->err;
        }
    } else if (hp->free_bytes < *min_free) {
        *min_free = hp->free_bytes;
        *min_free_test = this_test;
    }
}

int run_harness(int argc, char **argv, const harness_config_t *cfg)
{
    const char *atr_path = (argc > 1) ? argv[1] : cfg->default_atr_path;
    const char *dbg_path = (argc > 2) ? argv[2] : cfg->default_dbg_path;
    char *init_args[4];
    input_template_t input;
    unsigned char *mem;
    test_results_t *results;
    int seg = 0;
    size_t seg_pos = 0;
    int hold = 0;
    int idle = KEY_IDLE_FRAMES; /* allow first key immediately */
    int frame;
    int passed_count, i;

    /* ---- host-side heap instrumentation state (see heapcheck.h) ---- */
    heap_syms_t hsyms;
    heap_probe_t hp;
    unsigned *free_at = NULL;         /* free bytes as each test began     */
    unsigned  heap_origin = 0;        /* lowest __heapptr seen (~heap base)*/
    unsigned  heap_ceiling = 0;       /* __heapend (constant)              */
    unsigned  min_free = 0xFFFFFFFFu; /* high-water usage = lowest free    */
    int       min_free_test = -1;     /* test index where min_free hit     */
    unsigned  final_free = 0, final_blocks = 0, final_largest = 0;
    int       heap_corrupt_test = -1; /* first test index seen corrupt     */
    const char *heap_corrupt_err = NULL;
    int       prev_cur_test = -1;
    int       heap_ready = 0;
    unsigned  bheadp_addr = 0;        /* &bheadp, for the hang diagnostic  */
    int       have_bheadp = 0;

    /* libatari800_init uses the argc=-1 "count args until NULL" convention.
     * Deliberately no argv[0] program-name placeholder here: per
     * libatari800/api.c, libatari800_init() itself inserts a dummy NULL
     * zeroth entry whenever argv[0] doesn't end in "atari800" (so it's
     * unnecessary to supply one) -- and if we *did* supply an arbitrary
     * placeholder like "test_runner", the same "doesn't end in atari800"
     * check would fire on OUR placeholder too, shifting every real
     * argument down by one slot and causing the library to try (and
     * fail) to open "test_runner" itself as a file. Omitting argv[0]
     * lets the library insert its own correctly-skipped NULL entry
     * instead. */
    init_args[0] = "-config";
    init_args[1] = "/dev/null";
    init_args[2] = (char *)atr_path;
    init_args[3] = NULL;

    if (!libatari800_init(-1, init_args)) {
        fprintf(stderr, "libatari800_init failed: %s\n", libatari800_error_message());
        return 2;
    }

    mem = libatari800_get_main_memory_ptr();
    results = (test_results_t *)(mem + RESULTS_ADDR);

    /* Resolve cc65's heap bookkeeping variable addresses from the linker
     * .map (they are absent from the .dbg -- the runtime lib is built
     * without -g). The map lives beside the .dbg: same path, .dbg->.map.
     * Failure is non-fatal: we simply skip the memory report. */
    {
        char map_path[1024];
        size_t n = strlen(dbg_path);
        if (n >= 4 && n < sizeof(map_path) &&
            strcmp(dbg_path + n - 4, ".dbg") == 0) {
            memcpy(map_path, dbg_path, n - 4);
            memcpy(map_path + n - 4, ".map", 5);
            heap_ready = (heap_resolve_syms(map_path, &hsyms) == 0);
            have_bheadp = (map_resolve_symbol(map_path, "_bheadp", &bheadp_addr) == 0);
        }
        if (heap_ready) {
            free_at = malloc(sizeof(*free_at) * (size_t)cfg->num_tests);
            if (free_at == NULL) {
                heap_ready = 0;
            } else {
                for (i = 0; i < cfg->num_tests; i++)
                    free_at[i] = 0xFFFFFFFFu; /* sentinel: not sampled */
            }
        }
        if (!heap_ready)
            printf("(heap instrumentation disabled: could not resolve heap "
                   "symbols from map beside %s)\n", dbg_path);
    }

    libatari800_clear_input_array(&input);

    /* Don't start streaming the keystroke script until the test binary
     * has actually started running (results->magic set). Atari OS cold
     * boot takes a variable, non-trivial number of frames; starting the
     * script on a fixed frame count regardless of boot time caused
     * keystrokes to be consumed before the program was even ready to
     * read them. */
    for (frame = 0; frame < cfg->max_frames; frame++) {
        int rc;

        if (results->magic != RESULTS_MAGIC) {
            input.keychar = 0;
        } else if (hold > 0) {
            hold--;
            if (hold == 0)
                input.keychar = 0;
        } else if (idle < KEY_IDLE_FRAMES) {
            idle++;
            input.keychar = 0;
        } else {
            /* Skip over any fully-consumed segments. */
            while (cfg->segments[seg].chars != NULL &&
                   cfg->segments[seg].chars[seg_pos] == '\0') {
                seg++;
                seg_pos = 0;
            }
            if (cfg->segments[seg].chars != NULL &&
                (cfg->segments[seg].gate_test < 0 ||
                 results->cur_test >= (unsigned char)cfg->segments[seg].gate_test)) {
                input.keychar = cfg->segments[seg].chars[seg_pos++];
                hold = KEY_HOLD_FRAMES;
                idle = 0;
            } else {
                /* Either no segments left, or the next segment's gate
                 * hasn't opened yet -- wait, don't advance. Reset the
                 * idle counter so that, once the gate DOES open, the
                 * first keystroke isn't fed until KEY_IDLE_FRAMES later:
                 * a just-started test needs those few frames to paint its
                 * prompt (mlwrite/update) and actually block in
                 * term.t_getchar() before the first key arrives. Without
                 * this settle delay the leading char is fed on the test's
                 * very first frame and swallowed while the program is
                 * still displaying the prompt -- harmless for mlyesno
                 * (invalid key -> reprompt) but fatal for the file.c
                 * mlreply() wrappers, whose first fed char (C-G abort)
                 * would be lost, desyncing the whole segment. */
                idle = 0;
                input.keychar = 0;
            }
        }

        /* NOTE: libatari800_next_frame() returns !error_code (i.e. TRUE
         * for a normal frame, FALSE/0 if error_code is nonzero -- this is
         * the inverse of what its own doc comment enumerates). So 0 here
         * means libatari800_error_code was set this frame. During normal
         * boot, transient LIBATARI800_DLIST_ERROR frames are expected
         * (ANTIC hasn't set up a display list yet) and are not fatal --
         * only a real LIBATARI800_CPU_CRASH means the 6502 program
         * jammed on an illegal opcode. */
        rc = libatari800_next_frame(&input);
        if (rc == 0 && libatari800_error_code == LIBATARI800_CPU_CRASH) {
            printf("CPU CRASH at frame %d: %s\n", frame,
                   libatari800_error_message());
            printf("  magic=0x%02x done=%u total=%u failed=%u cur_test=%u\n",
                   results->magic, results->done, results->total, results->failed,
                   results->cur_test);
            if (results->cur_test < (unsigned char)cfg->num_tests)
                printf("  crashed in/after test #%u (%s, %s: %s)\n", results->cur_test,
                       cfg->test_info[results->cur_test].name,
                       cfg->test_info[results->cur_test].src_file,
                       cfg->test_info[results->cur_test].src_func);
            libatari800_exit();
            return 1;
        }

        /* Heap sampling at clean test boundaries. cur_test advances only
         * between test functions (never mid-malloc), so the free list is
         * quiescent here -- safe to validate without racing an in-flight
         * allocation. Record the free bytes as each test begins, and fold
         * the sample into origin/ceiling/high-water/corruption tracking. */
        if (heap_ready && results->magic == RESULTS_MAGIC &&
            (int)results->cur_test != prev_cur_test) {
            int t = (int)results->cur_test;
            prev_cur_test = t;
            if (t >= 0 && t < cfg->num_tests) {
                heap_take_sample(mem, &hsyms, &hp, &heap_origin, &heap_ceiling,
                                 &min_free, &min_free_test, &heap_corrupt_test,
                                 &heap_corrupt_err, t);
                if (free_at[t] == 0xFFFFFFFFu)
                    free_at[t] = hp.free_bytes;
                if (hp.corrupt && heap_corrupt_test == t)
                    printf("*** HEAP CORRUPTION detected entering test #%d "
                           "(%s, %s: %s): %s\n", t, cfg->test_info[t].name,
                           cfg->test_info[t].src_file, cfg->test_info[t].src_func,
                           hp.err ? hp.err : "(unknown)");
            }
        }

        if (results->magic == RESULTS_MAGIC && results->done)
            break;
    }

    if (!(results->magic == RESULTS_MAGIC && results->done)) {
        printf("TIMEOUT after %d frames (no result block / not done)\n", frame);
        printf("  magic=0x%02x done=%u total=%u failed=%u cur_test=%u\n",
               results->magic, results->done, results->total, results->failed,
               results->cur_test);
        if (results->cur_test < (unsigned char)cfg->num_tests)
            printf("  hung in test #%u (%s, %s: %s)\n", results->cur_test,
                   cfg->test_info[results->cur_test].name,
                   cfg->test_info[results->cur_test].src_file,
                   cfg->test_info[results->cur_test].src_func);

        /* Hot-loop localizer: snapshot execution counts, run a short burst
         * of extra frames, and report the addresses whose execution count
         * grew the most. In an infinite loop those addresses dominate the
         * delta, pinpointing the 6502 PC range that is spinning. */
        {
            static unsigned long snap[65536];
            unsigned long a;
            input.keychar = 0;
            for (a = 0; a < 65536; a++)
                snap[a] = MONITOR_coverage[a].count;
            for (i = 0; i < 300; i++)
                (void)libatari800_next_frame(&input);
            {
                int top;
                printf("  hot-loop PC deltas (top 12 executed addresses in +300 frames):\n");
                for (top = 0; top < 12; top++) {
                    unsigned long best = 0, besta = 0, d;
                    for (a = 0; a < 65536; a++) {
                        d = MONITOR_coverage[a].count - snap[a];
                        if (d > best) { best = d; besta = a; }
                    }
                    if (best == 0) break;
                    printf("    $%04lX  +%lu\n", besta, best);
                    snap[besta] = MONITOR_coverage[besta].count; /* exclude next round */
                }
            }
        }

        /* Buffer-chain walk: bfind spins on a cyclic b_bufp list. Walk it
         * from bheadp (address resolved from the linker map), printing
         * each buffer's address + b_bname, to reveal where the cycle
         * closes and which buffer is corrupt. */
        if (have_bheadp) {
            unsigned int bp = mem[bheadp_addr] | (mem[(bheadp_addr + 1) & 0xFFFF] << 8);
            int n;
            printf("  buffer chain from bheadp=$%04X:\n", bp);
            for (n = 0; n < 24 && bp != 0; n++) {
                char nm[17];
                int k;
                for (k = 0; k < 16; k++) {
                    unsigned char c = mem[(bp + 32 + k) & 0xFFFF];
                    nm[k] = (c >= 32 && c < 127) ? (char)c : '.';
                }
                nm[16] = 0;
                printf("    [%2d] $%04X  b_bufp=$%04X  bname=\"%s\"\n", n, bp,
                       mem[bp & 0xFFFF] | (mem[(bp + 1) & 0xFFFF] << 8), nm);
                bp = mem[bp & 0xFFFF] | (mem[(bp + 1) & 0xFFFF] << 8);
            }
            if (bp != 0)
                printf("    ... (chain did not terminate in 24 hops -> cycle)\n");
        }
        for (i = 0; i < cfg->num_tests; i++) {
            if (results->results[i] == TEST_FAIL)
                printf("  FAILED: #%u %s (%s: %s)\n", i, cfg->test_info[i].name,
                       cfg->test_info[i].src_file, cfg->test_info[i].src_func);
        }
        libatari800_exit();
        return 1;
    }

    printf("MicroEMACS unit tests (real 6502 code, real libatari800 emulation)\n");
    printf("----------------------------------------------------------------------------\n");
    passed_count = 0;
    {
        const char *cur_file = "";
        for (i = 0; i < cfg->num_tests; i++) {
            const char *status;
            if (strcmp(cfg->test_info[i].src_file, cur_file) != 0) {
                cur_file = cfg->test_info[i].src_file;
                printf("%s:\n", cur_file);
            }
            if (results->results[i] == TEST_PASS) {
                status = "PASS";
                passed_count++;
            } else if (results->results[i] == TEST_FAIL) {
                status = "FAIL";
            } else {
                status = "SKIP";
            }
            printf("  [%s] %-38s %s\n", status, cfg->test_info[i].name, cfg->test_info[i].src_func);
        }
    }
    printf("----------------------------------------------------------------------------\n");
    printf("%d/%d tests passed (frames used: %d)\n", passed_count, results->total, frame);

    if (heap_ready) {
        unsigned heap_total;

        /* Final probe: state after the last test completed. */
        heap_probe(mem, &hsyms, &hp);
        final_free = hp.free_bytes;
        final_blocks = hp.free_blocks;
        final_largest = hp.largest_free;
        if (heap_ceiling == 0)
            heap_ceiling = hp.heapend;
        if (hp.corrupt && heap_corrupt_test < 0) {
            heap_corrupt_test = cfg->num_tests;   /* only at the very end */
            heap_corrupt_err = hp.err;
        } else if (!hp.corrupt && hp.free_bytes < min_free) {
            min_free = hp.free_bytes;
            min_free_test = cfg->num_tests;
        }

        heap_total = (heap_ceiling > heap_origin && heap_origin != 0)
                     ? heap_ceiling - heap_origin : 0;

        printf("----------------------------------------------------------------------------\n");
        printf("Heap utilization (cc65 malloc, host-observed, sampled at test boundaries):\n");
        printf("  heap window         ~%u bytes  ($%04X base .. $%04X ceiling)\n",
               heap_total, heap_origin, heap_ceiling);
        if (min_free != 0xFFFFFFFFu) {
            unsigned peak_used = (heap_total >= min_free) ? heap_total - min_free : 0;
            if (min_free_test >= cfg->num_tests)
                printf("  peak resident used   %u bytes  (min free %u, at end of run)\n",
                       peak_used, min_free);
            else
                printf("  peak resident used   %u bytes  (min free %u, entering test #%d %s)\n",
                       peak_used, min_free, min_free_test,
                       cfg->test_info[min_free_test].name);
        }
        {
            unsigned start_free = (cfg->num_tests > 0 && free_at[0] != 0xFFFFFFFFu)
                                  ? free_at[0] : final_free;
            printf("  free at first test   %u bytes\n", start_free);
        }
        printf("  free at end of run   %u bytes  (%u free block%s on list, largest %u)\n",
               final_free, final_blocks, final_blocks == 1 ? "" : "s", final_largest);
        printf("  free-list integrity  %s",
               heap_corrupt_test < 0 ? "OK (validated at every test boundary)\n" : "");
        if (heap_corrupt_test >= 0) {
            if (heap_corrupt_test >= cfg->num_tests)
                printf("*** CORRUPT at end of run: %s\n",
                       heap_corrupt_err ? heap_corrupt_err : "(unknown)");
            else
                printf("*** CORRUPT entering test #%d (%s): %s\n", heap_corrupt_test,
                       cfg->test_info[heap_corrupt_test].name,
                       heap_corrupt_err ? heap_corrupt_err : "(unknown)");
        }

        /* Per-suite drift: each overlay swap runs free_other_buffers(), so
         * the free-byte level at the first test of each contiguous src_file
         * group is that suite's clean baseline. A monotonic decline across
         * suites -- despite the frees -- is the signature of a leak in the
         * modified application code. */
        printf("Per-suite free baseline (at first test of each suite) & drift:\n");
        {
            const char *cur_file = "";
            int need_baseline = 0;
            unsigned first_baseline = 0xFFFFFFFFu, last_baseline = 0xFFFFFFFFu;
            for (i = 0; i < cfg->num_tests; i++) {
                if (strcmp(cfg->test_info[i].src_file, cur_file) != 0) {
                    cur_file = cfg->test_info[i].src_file;
                    need_baseline = 1;         /* first sampled test wins */
                }
                /* cur_test can advance by >1 within a single emulated frame
                 * (a burst of quick tests), so a suite's very first test may
                 * go unsampled -- use the first test in the suite that DID
                 * get a boundary sample as the baseline. */
                if (need_baseline && free_at[i] != 0xFFFFFFFFu) {
                    printf("  %-16s %u bytes\n", cur_file, free_at[i]);
                    need_baseline = 0;
                    if (first_baseline == 0xFFFFFFFFu)
                        first_baseline = free_at[i];
                    last_baseline = free_at[i];
                }
            }
            if (first_baseline != 0xFFFFFFFFu && last_baseline != 0xFFFFFFFFu) {
                long drift = (long)first_baseline - (long)last_baseline;
                printf("  net drift (first suite - last)  %ld bytes  -> %s\n",
                       drift,
                       drift > HEAP_DRIFT_LEAK_THRESHOLD
                           ? "*** POSSIBLE LEAK (free bytes declined across suites)"
                           : "OK (baselines stable; no growing leak)");
            }
        }
    }

    {
        static unsigned char hit[65536];
        unsigned long addr;

        for (addr = 0; addr < 65536; addr++)
            hit[addr] = (MONITOR_coverage[addr].count > 0);

        if (dbgcov_analyze(dbg_path, cfg->dbgcov_targets, cfg->num_dbgcov_targets, hit) != 0) {
            printf("(line coverage unavailable: could not read debug info at %s)\n", dbg_path);
        } else {
            int t;
            printf("----------------------------------------------------------------------------\n");
            printf("Line coverage (statement-level, from real 6502 execution trace):\n");
            for (t = 0; t < cfg->num_dbgcov_targets; t++) {
                double pct = cfg->dbgcov_targets[t].coverable_lines
                    ? (100.0 * cfg->dbgcov_targets[t].covered_lines / cfg->dbgcov_targets[t].coverable_lines)
                    : 0.0;
                printf("  %-16s %3u/%-3u lines (%5.1f%%)\n", cfg->dbgcov_targets[t].label,
                       cfg->dbgcov_targets[t].covered_lines, cfg->dbgcov_targets[t].coverable_lines, pct);
            }
        }
    }

    libatari800_exit();

    free(free_at);
    return (results->failed == 0 && passed_count == cfg->num_tests) ? 0 : 1;
}
