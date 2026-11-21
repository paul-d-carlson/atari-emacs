/*
 * stress_test.c
 *
 * Long-running (operator-terminated), CONTINUOUS-SESSION soak test for
 * heap fragmentation, leaks, and stack pressure -- one single boot, run
 * indefinitely, stopped by typing 'Q'/'q'+RETURN at the controlling
 * terminal, a SIGINT/SIGTERM, or by creating STRESS_OUT_DIR/STOP_STRESS. On stop
 * (or a crash), the in-flight cycle finishes, one final checkpoint+probe
 * pass runs, and an end-of-run report is printed before exiting.
 *
 * Design: two content buffers ("main" + "WORK.TXT") are populated
 * once with small, fixed baseline text and repeatedly put through
 * CONTENT-PRESERVING round trips (net-zero edits, save+reload,
 * evict+reload buffer switches, help/dired visits, aborted prompts) --
 * so verification is always a simple byte-exact compare against each
 * buffer's ORIGINAL expected content, no matter how many cycles have
 * run. Every CHECKPOINT_INTERVAL cycles, both buffers are re-verified
 * and the cc65 heap (via heapcheck.h, reused unmodified from
 * tests/driver/), the cc65 C-stack pointer, and DOS free-sector count
 * are sampled and appended to a CSV log -- the trend across that CSV is
 * what answers "did fragmentation/leaks/stack pressure appear over
 * time," since no single before/after snapshot can show a trend.
 *
 * Deliberately NOT implemented in this first version (noted so it isn't
 * mistaken for an oversight): deliberately driving the DOS directory to
 * its 64-file ceiling as a scripted edge case. The rotating 2-file pool
 * used here stays far under that limit by design; exercising the
 * ceiling itself would need many more scripted file creations and was
 * left out to keep this version's scope tractable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "int_harness.h"
#include "stress_harness.h"
#include "atr_dos25.h"
#include "heapcheck.h"

/* Host paths; the build passes absolute ones. */
#ifndef STRESS_MAP_PATH
#define STRESS_MAP_PATH "build/me.map"
#endif
#ifndef STRESS_OUT_DIR
#define STRESS_OUT_DIR "build/integration"
#endif

/* ---- stop mechanism: SIGINT/SIGTERM, stdin 'Q', or a sentinel file ---- */

static volatile sig_atomic_t g_stop_requested = 0;

static void on_stop_signal(int signum)
{
    (void)signum;
    g_stop_requested = 1;
}

/* The controlling terminal's stdin file description can be SHARED with
 * other processes (e.g. a pty also used by the invoking shell/pipeline),
 * so O_NONBLOCK must be restored before exit -- otherwise it leaks into
 * whatever runs next on that same terminal. atexit() covers both a
 * normal return from main() and any exit() call made after this runs. */
static int g_stdin_orig_flags = -1;

static void stdin_restore_blocking(void)
{
    if (g_stdin_orig_flags != -1)
        fcntl(STDIN_FILENO, F_SETFL, g_stdin_orig_flags);
}

static void stdin_set_nonblocking(void)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags == -1)
        return;
    g_stdin_orig_flags = flags;
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    atexit(stdin_restore_blocking);
}

static void poll_stop_key(void)
{
    char buf[64];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        ssize_t i;
        for (i = 0; i < n; i++) {
            if (buf[i] == 'Q' || buf[i] == 'q')
                g_stop_requested = 1;
        }
    }
    (void)errno; /* EAGAIN/EWOULDBLOCK expected when nothing is waiting */
}

static int stop_file_present(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

/* ---- generic small map-symbol resolver (for cc65's zero-page "sp") ---- */

static int resolve_symbol(const char *map_path, const char *name, unsigned *out_addr)
{
    FILE *f = fopen(map_path, "r");
    char line[512];
    if (f == NULL)
        return -1;
    while (fgets(line, sizeof(line), f) != NULL) {
        char *tok = strtok(line, " \t\r\n");
        while (tok != NULL) {
            if (strcmp(tok, name) == 0) {
                char *addr = strtok(NULL, " \t\r\n");
                if (addr != NULL) {
                    *out_addr = (unsigned)(strtoul(addr, NULL, 16) & 0xFFFF);
                    fclose(f);
                    return 0;
                }
            }
            tok = strtok(NULL, " \t\r\n");
        }
    }
    fclose(f);
    return -1;
}

/* cc65 zero-page runtime "C stack" (parameter/eval stack) pointer, grows
 * DOWN. Ceiling/floor derived from src/atari.cfg's arithmetic: MAIN
 * spans [$1E00, $BC20 - $0200) = [$1E00, $BA20), and the reserved
 * __STACKSIZE__ ($0200) region sits directly above it, [$BA20, $BC20) --
 * so sp starts near $BC20 and must never fall below $BA20. */
#define STACK_CEILING 0xBC20u
#define STACK_FLOOR   0xBA20u

static unsigned rd16(const unsigned char *mem, unsigned addr)
{
    return (unsigned)mem[addr & 0xFFFF] | ((unsigned)mem[(addr + 1) & 0xFFFF] << 8);
}

/* ---- baseline buffer content ---- */

static const char *buf0_lines[] = {
    "main: stress-test baseline content.",
    "",
    "This text never changes across the whole run --",
    "every scripted edit below is a net-zero round trip",
    "(insert then delete, kill then yank, save then reload).",
    "",
    "If a checkpoint ever fails to match this exact text,",
    "that is the signal something is actually broken."
};
#define NUM_BUF0_LINES ((int)(sizeof(buf0_lines) / sizeof(buf0_lines[0])))

static const char *buf1_lines[] = {
    "WORK.TXT: second stress-test baseline buffer.",
    "",
    "Kept deliberately separate from \"main\" so buffer",
    "create/switch/evict/reload churn (C-X B, C-X X) has",
    "a second real buffer to move between, not just one.",
    "",
    "Also saved/reloaded/killed and recreated on its own",
    "schedule, independent of main's cycle."
};
#define NUM_BUF1_LINES ((int)(sizeof(buf1_lines) / sizeof(buf1_lines[0])))

typedef struct {
    const char *bufname;   /* buffer name, for C-X B / C-X C-F */
    const char *filename;  /* DOS filename used for save/reload */
    const char *const *lines;
    int nlines;
    unsigned char *expected;
    size_t expected_len;
} stress_buf_t;

#define NUM_BUFS 2
static stress_buf_t g_bufs[NUM_BUFS];

static char *build_typed(const char *const *lines, int n)
{
    size_t cap = 1, len = 0;
    int i;
    char *buf;
    for (i = 0; i < n; i++)
        cap += strlen(lines[i]) + 1;
    buf = malloc(cap);
    if (buf == NULL) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (i = 0; i < n; i++) {
        size_t ln = strlen(lines[i]);
        memcpy(buf + len, lines[i], ln);
        len += ln;
        if (i != n - 1)
            buf[len++] = '\n';
    }
    buf[len] = '\0';
    return buf;
}

static unsigned char *build_expected(const char *const *lines, int n, size_t *out_len)
{
    size_t cap = 1, len = 0;
    int i;
    unsigned char *buf;
    for (i = 0; i < n; i++)
        cap += strlen(lines[i]) + 1;
    buf = malloc(cap);
    if (buf == NULL) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (i = 0; i < n; i++) {
        size_t ln = strlen(lines[i]);
        memcpy(buf + len, lines[i], ln);
        len += ln;
        buf[len++] = '\x9b';
    }
    *out_len = len;
    return buf;
}

/* ---- harness plumbing ---- */

static stress_result_t g_res;
static const char *g_atr_path;
static int g_current_buf = -1;
static long g_cycle = 0;
static long g_checkpoints_ok = 0;
static long g_checkpoints_failed = 0;
static int g_any_failure = 0;

/* Generous per-batch safety cap: largest single operation here (a
 * whole-buffer save+reload) is far smaller than
 * scenario_file_operations.c's 1149-byte HELPOUT.TXT case, which needed
 * 300000; reuse that same proven margin rather than re-deriving a
 * tighter one. */
#define BATCH_MAX_FRAMES  300000
#define KEY_HOLD_FRAMES   3
#define KEY_IDLE_FRAMES   100
#define POST_SETTLE_FRAMES 150
#define BOOT_SETTLE_FRAMES 400

static int feed(const int_key_segment_t *segments, const char *label)
{
    if (g_res.crashed)
        return 0;
    stress_feed(segments, KEY_HOLD_FRAMES, KEY_IDLE_FRAMES, POST_SETTLE_FRAMES,
                BATCH_MAX_FRAMES, &g_res);
    if (g_res.crashed) {
        fprintf(stderr, "STOP: crash during \"%s\" at cumulative frame %d\n",
                label, g_res.crash_frame);
        g_any_failure = 1;
        return 0;
    }
    return 1;
}

static void ensure_current(int target)
{
    int_key_segment_t seg[3];
    if (g_current_buf == target)
        return;
    seg[0].chars = "\x18" "B";      seg[0].label = "C-X B (switch)";
    seg[1].chars = g_bufs[target].bufname; seg[1].label = "buffer name";
    seg[2].chars = "\n";            seg[2].label = "RETURN";
    {
        int_key_segment_t full[4];
        full[0] = seg[0]; full[1] = seg[1]; full[2] = seg[2];
        full[3].chars = NULL; full[3].label = NULL;
        if (feed(full, "C-X B (ensure_current)"))
            g_current_buf = target;
    }
}

/* ---- one-time setup: create + populate + save both buffers ---- */

static void setup(void)
{
    char *typed0 = build_typed(buf0_lines, NUM_BUF0_LINES);
    char *typed1 = build_typed(buf1_lines, NUM_BUF1_LINES);
    int_key_segment_t segs[16];
    int i = 0;

    g_bufs[0].bufname = "main";
    g_bufs[0].filename = "CKMAIN.TXT";
    g_bufs[0].lines = buf0_lines;
    g_bufs[0].nlines = NUM_BUF0_LINES;
    g_bufs[0].expected = build_expected(buf0_lines, NUM_BUF0_LINES, &g_bufs[0].expected_len);

    g_bufs[1].bufname = "WORK.TXT";
    g_bufs[1].filename = "WORK.TXT";
    g_bufs[1].lines = buf1_lines;
    g_bufs[1].nlines = NUM_BUF1_LINES;
    g_bufs[1].expected = build_expected(buf1_lines, NUM_BUF1_LINES, &g_bufs[1].expected_len);

    segs[i].chars = "\x07" "\x07";       segs[i++].label = "warm-up (C-G x2)";
    segs[i].chars = typed0;              segs[i++].label = "type main baseline";
    segs[i].chars = "\x1b" "<";          segs[i++].label = "M-< (re-anchor)";
    segs[i].chars = "\x18" "\x17";       segs[i++].label = "C-X C-W (filewrite)";
    segs[i].chars = "CKMAIN.TXT";        segs[i++].label = "filename";
    segs[i].chars = "\n";                segs[i++].label = "RETURN";
    segs[i].chars = "\x18" "\x06";       segs[i++].label = "C-X C-F (filefind: create WORK.TXT)";
    segs[i].chars = "WORK.TXT";          segs[i++].label = "filename";
    segs[i].chars = "\n";                segs[i++].label = "RETURN";
    segs[i].chars = typed1;              segs[i++].label = "type WORK.TXT baseline";
    segs[i].chars = "\x1b" "<";          segs[i++].label = "M-< (re-anchor)";
    segs[i].chars = "\x18" "\x13";       segs[i++].label = "C-X C-S (filesave)";
    segs[i].chars = NULL;                segs[i++].label = NULL;

    if (stress_boot(g_atr_path, BOOT_SETTLE_FRAMES, &g_res) != 0) {
        fprintf(stderr, "stress_boot failed\n");
        exit(1);
    }
    feed(segs, "setup");
    g_current_buf = 1; /* WORK.TXT is current after setup's C-X C-F + C-X C-S */

    free(typed0);
    free(typed1);
}

/* ---- checkpoint: re-verify both buffers + sample heap/stack/disk ---- */

static heap_syms_t g_heap_syms;
static FILE *g_csv = NULL;
static long g_probe_count = 0;
static unsigned g_first_free_bytes = 0, g_last_free_bytes = 0, g_min_free_bytes = 0, g_max_free_bytes = 0;
static unsigned g_max_stack_depth = 0;
static unsigned g_first_disk_free = 0, g_last_disk_free = 0;
static int g_heap_ever_corrupt = 0;

static void verify_one(int idx)
{
    unsigned char *actual = NULL;
    size_t actual_len = 0;
    int_key_segment_t segs[2];

    ensure_current(idx);
    segs[0].chars = "\x18" "\x13"; segs[0].label = "C-X C-S (checkpoint save)";
    segs[1].chars = NULL;          segs[1].label = NULL;
    if (!feed(segs, "checkpoint save"))
        return;

    if (atr_dos25_read_file(g_atr_path, g_bufs[idx].filename, &actual, &actual_len) != 0) {
        fprintf(stderr, "CHECKPOINT FAIL: %s not found on disk (cycle %ld)\n",
                g_bufs[idx].filename, g_cycle);
        g_checkpoints_failed++;
        g_any_failure = 1;
        return;
    }
    if (actual_len != g_bufs[idx].expected_len ||
        memcmp(actual, g_bufs[idx].expected, g_bufs[idx].expected_len) != 0) {
        fprintf(stderr, "CHECKPOINT FAIL: %s content mismatch at cycle %ld "
                "(expected %zu bytes, got %zu bytes)\n",
                g_bufs[idx].filename, g_cycle, g_bufs[idx].expected_len, actual_len);
        g_checkpoints_failed++;
        g_any_failure = 1;
    } else {
        g_checkpoints_ok++;
    }
    free(actual);
}

static void probe_and_log(void)
{
    heap_probe_t hp;
    unsigned sp_val = 0, stack_depth = 0, disk_free = 0;
    int have_sp = 0, have_disk = 0;

    heap_probe(g_res.mem, &g_heap_syms, &hp);
    if (hp.corrupt) {
        fprintf(stderr, "HEAP CORRUPT at cycle %ld: %s\n", g_cycle, hp.err);
        g_heap_ever_corrupt = 1;
        g_any_failure = 1;
    }

    {
        unsigned sp_addr;
        static unsigned sp_addr_cached = 0xFFFFFFFFu;
        static int sp_resolved = -1;
        if (sp_resolved < 0) {
            sp_resolved = (resolve_symbol(STRESS_MAP_PATH, "sp", &sp_addr_cached) == 0);
        }
        if (sp_resolved) {
            sp_addr = sp_addr_cached;
            sp_val = rd16(g_res.mem, sp_addr);
            stack_depth = (sp_val <= STACK_CEILING) ? (STACK_CEILING - sp_val) : 0;
            have_sp = 1;
        }
    }

    have_disk = (atr_dos25_free_sectors(g_atr_path, &disk_free) == 0);

    if (g_probe_count == 0) {
        g_first_free_bytes = g_min_free_bytes = g_max_free_bytes = hp.free_bytes;
        if (have_disk) g_first_disk_free = disk_free;
    } else {
        if (hp.free_bytes < g_min_free_bytes) g_min_free_bytes = hp.free_bytes;
        if (hp.free_bytes > g_max_free_bytes) g_max_free_bytes = hp.free_bytes;
    }
    g_last_free_bytes = hp.free_bytes;
    if (have_disk) g_last_disk_free = disk_free;
    if (stack_depth > g_max_stack_depth) g_max_stack_depth = stack_depth;
    g_probe_count++;

    if (g_csv != NULL) {
        fprintf(g_csv, "%ld,%d,%u,%u,%u,%d,%u,%u,%u\n",
                g_cycle, g_res.total_frames, hp.free_bytes, hp.largest_free,
                hp.free_blocks, hp.corrupt, have_sp ? sp_val : 0, stack_depth,
                have_disk ? disk_free : 0);
        fflush(g_csv);
    }

    printf("[cycle %6ld] frames=%d heap_free=%u largest_free=%u stack_depth=%u "
           "disk_free=%u ok=%ld fail=%ld\n",
           g_cycle, g_res.total_frames, hp.free_bytes, hp.largest_free, stack_depth,
           have_disk ? disk_free : 0, g_checkpoints_ok, g_checkpoints_failed);
}

static void checkpoint(void)
{
    int i;
    for (i = 0; i < NUM_BUFS; i++)
        verify_one(i);
    probe_and_log();
}

/* ---- one randomized cycle of content-preserving churn ---- */

static void op_boundary_edit(void)
{
    static const int lens[] = { 1, 3, 7, 15, 16, 17, 31, 32, 33 };
    int target = rand() % NUM_BUFS;
    int line = 2 + (rand() % (g_bufs[target].nlines >= 7 ? 5 : 1));
    int len = lens[rand() % (int)(sizeof(lens) / sizeof(lens[0]))];
    char gotoline_buf[8];
    char insert_buf[40];
    char delete_buf[40];
    int_key_segment_t segs[6];

    snprintf(gotoline_buf, sizeof(gotoline_buf), "\x1b%d\x1bG", line);
    memset(insert_buf, 'Z', (size_t)len); insert_buf[len] = '\0';
    memset(delete_buf, '\b', (size_t)len); delete_buf[len] = '\0';

    ensure_current(target);
    segs[0].chars = "\x1b" "<";      segs[0].label = "M-< (re-anchor)";
    segs[1].chars = gotoline_buf;    segs[1].label = "M-<n> M-G (gotoline)";
    segs[2].chars = insert_buf;      segs[2].label = "insert N chars";
    segs[3].chars = delete_buf;      segs[3].label = "delete N chars (backdel)";
    segs[4].chars = NULL;            segs[4].label = NULL;
    feed(segs, "boundary edit round trip");
}

static void op_kill_yank(void)
{
    int target = rand() % NUM_BUFS;
    int line = 2 + (rand() % (g_bufs[target].nlines >= 7 ? 5 : 1));
    char gotoline_buf[8];
    int_key_segment_t segs[5];

    snprintf(gotoline_buf, sizeof(gotoline_buf), "\x1b%d\x1bG", line);

    ensure_current(target);
    segs[0].chars = "\x1b" "<";   segs[0].label = "M-< (re-anchor)";
    segs[1].chars = gotoline_buf; segs[1].label = "M-<n> M-G (gotoline)";
    segs[2].chars = "\x0b";       segs[2].label = "C-K (kill to EOL)";
    segs[3].chars = "\x19";       segs[3].label = "C-Y (yank)";
    segs[4].chars = NULL;         segs[4].label = NULL;
    feed(segs, "kill/yank round trip");
}

static void op_save_reload(void)
{
    int target = rand() % NUM_BUFS;
    int_key_segment_t segs[5];

    ensure_current(target);
    segs[0].chars = "\x18" "\x13";              segs[0].label = "C-X C-S (filesave)";
    segs[1].chars = "\x18" "\x12";              segs[1].label = "C-X C-R (fileread)";
    segs[2].chars = g_bufs[target].filename;    segs[2].label = "filename";
    segs[3].chars = "\n";                       segs[3].label = "RETURN";
    segs[4].chars = NULL;                       segs[4].label = NULL;
    feed(segs, "whole-buffer save+reload round trip");
}

static void op_switch_cycle(void)
{
    int other = 1 - g_current_buf;
    int_key_segment_t segs[2];

    /* Ensure current is clean before leaving it, so swbuffer() actually
     * evicts (bclear()s) it rather than just detaching -- see
     * scenario_file_operations.c's header comment on eviction. */
    segs[0].chars = "\x18" "\x13"; segs[0].label = "C-X C-S (clean before switch)";
    segs[1].chars = NULL;          segs[1].label = NULL;
    feed(segs, "pre-switch save");
    ensure_current(other);
}

static void op_help_and_back(void)
{
    int back_to = g_current_buf;
    int_key_segment_t segs[2];
    segs[0].chars = "\x1b" "?"; segs[0].label = "M-? (help)";
    segs[1].chars = NULL;       segs[1].label = NULL;
    feed(segs, "M-? (help)");
    g_current_buf = -1; /* help() switched us to the help buffer */
    ensure_current(back_to);
}

static void op_dired_and_back(void)
{
    int back_to = g_current_buf;
    int_key_segment_t segs[2];
    segs[0].chars = "\x18" "D"; segs[0].label = "C-X D (dired)";
    segs[1].chars = NULL;       segs[1].label = NULL;
    feed(segs, "C-X D (dired)");
    g_current_buf = -1; /* dired() switched us to *dired* */
    ensure_current(back_to);
}

static void op_aborted_filefind(void)
{
    int_key_segment_t segs[4];
    segs[0].chars = "\x18" "\x06"; segs[0].label = "C-X C-F (to be aborted)";
    segs[1].chars = "BOGUS";       segs[1].label = "partial bogus filename";
    segs[2].chars = "\x07";        segs[2].label = "C-G (abort)";
    segs[3].chars = NULL;          segs[3].label = NULL;
    feed(segs, "aborted C-X C-F");
}

static void run_one_cycle(void)
{
    int r = rand() % 100;
    if (r < 40) op_boundary_edit();
    else if (r < 60) op_kill_yank();
    else if (r < 75) op_save_reload();
    else if (r < 85) op_switch_cycle();
    else if (r < 92) op_help_and_back();
    else if (r < 98) op_dired_and_back();
    else op_aborted_filefind();
}

/* ---- final report ---- */

static void print_report(void)
{
    printf("\n=== stress test end-of-run report ===\n");
    printf("cycles run:            %ld\n", g_cycle);
    printf("cumulative frames:      %d\n", g_res.total_frames);
    printf("crashed:                %s%s\n", g_res.crashed ? "YES" : "no",
           g_res.crashed ? " (see stderr for frame/location)" : "");
    printf("checkpoints passed:     %ld\n", g_checkpoints_ok);
    printf("checkpoints failed:     %ld\n", g_checkpoints_failed);
    printf("heap free bytes:        first=%u  min=%u  max=%u  last=%u\n",
           g_first_free_bytes, g_min_free_bytes, g_max_free_bytes, g_last_free_bytes);
    printf("heap ever corrupt:      %s\n", g_heap_ever_corrupt ? "YES" : "no");
    printf("max stack depth used:   %u bytes (ceiling=%u floor=%u -> %u bytes margin at worst)\n",
           g_max_stack_depth, STACK_CEILING, STACK_FLOOR,
           (STACK_CEILING - STACK_FLOOR) - g_max_stack_depth);
    printf("disk free sectors:      first=%u  last=%u\n", g_first_disk_free, g_last_disk_free);

    printf("\nVERDICT: ");
    if (g_res.crashed || g_checkpoints_failed > 0 || g_heap_ever_corrupt) {
        printf("FAIL\n");
    } else if (g_last_free_bytes + 64 < g_first_free_bytes) {
        /* Small slack (64 bytes) for one-time/rare allocations; a real
         * per-cycle leak will keep growing well past this on a longer
         * run -- flag as needing a longer look, not an outright fail. */
        printf("INCONCLUSIVE (heap free bytes declined by %u since first probe; "
               "run longer to confirm a real leak vs. one-time settling)\n",
               g_first_free_bytes - g_last_free_bytes);
    } else {
        printf("PASS\n");
    }
}

/* ---- main ---- */

int main(int argc, char **argv)
{
    const char *csv_path = STRESS_OUT_DIR "/stress_log.csv";
    const char *stop_file = STRESS_OUT_DIR "/STOP_STRESS";
    long max_cycles = 0; /* 0 = unlimited */
    unsigned seed = (unsigned)time(NULL);

    g_atr_path = (argc > 1) ? argv[1] : STRESS_OUT_DIR "/stress.atr";
    if (argc > 2) max_cycles = atol(argv[2]);
    if (argc > 3) seed = (unsigned)strtoul(argv[3], NULL, 10);

    printf("stress_test: atr=%s max_cycles=%ld seed=%u\n", g_atr_path, max_cycles, seed);
    printf("stress_test: type 'Q' + RETURN, send SIGINT/SIGTERM, or create %s to stop gracefully\n",
           stop_file);
    srand(seed);

    signal(SIGINT, on_stop_signal);
    signal(SIGTERM, on_stop_signal);
    stdin_set_nonblocking();
    remove(stop_file);

    if (heap_resolve_syms(STRESS_MAP_PATH, &g_heap_syms) != 0) {
        fprintf(stderr, "warning: could not resolve heap symbols from %s "
                "-- heap probing disabled\n", STRESS_MAP_PATH);
    }

    g_csv = fopen(csv_path, "w");
    if (g_csv != NULL)
        fprintf(g_csv, "cycle,total_frames,heap_free_bytes,heap_largest_free,"
                "heap_free_blocks,heap_corrupt,sp_value,stack_depth,disk_free_sectors\n");

    setup();
    checkpoint(); /* baseline probe, cycle 0 */

    while (max_cycles == 0 || g_cycle < max_cycles) {
        if (g_res.crashed)
            break;
        poll_stop_key();
        if (stop_file_present(stop_file))
            g_stop_requested = 1;
        if (g_stop_requested) {
            printf("stop requested -- finishing current state, running final checkpoint\n");
            break;
        }

        run_one_cycle();
        g_cycle++;

        if (g_res.crashed)
            break;
        if (g_cycle % 20 == 0)
            checkpoint();
    }

    if (!g_res.crashed)
        checkpoint(); /* final pass */
    else
        probe_and_log(); /* still read whatever the emulator's RAM/disk show at crash time */

    print_report();

    if (g_csv != NULL)
        fclose(g_csv);
    stress_shutdown();
    remove(stop_file);

    return g_any_failure ? 1 : 0;
}
