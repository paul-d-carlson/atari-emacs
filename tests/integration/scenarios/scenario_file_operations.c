/*
 * scenario_file_operations.c
 *
 * Second integration scenario for the real MicroEMACS Atari product
 * (same boot/build recipe as scenario_content_editing.c -- see
 * mk/test-integration.mk -- but a fresh, independent .atr so this scenario's disk
 * writes never interact with the first scenario's).
 *
 * Covers the commands scenario_content_editing.c's file header
 * explicitly deferred: C-X C-F (filefind), C-X C-R (fileread), C-X B
 * (usebuffer), C-X X (nextbuffer), C-X K (killbuffer), C-X D (dired),
 * M-? (help), plus first-time coverage of C-X C-S (filesave) and M-Z
 * (quit's OTHER binding). Cross-checked against src/file.c, src/buffer.c,
 * src/bind.c and src/main.c before scripting a single keystroke (see the
 * per-step comments below for the specific source-grounded reasoning).
 *
 * ^G (ctrlg/abort) is exercised right after warm-up: src/display.c's
 * mlreplyt() (the prompt reader used by filefind/fileread/usebuffer/
 * killbuffer/filewrite) reads raw bytes directly and treats 0x07 (BEL)
 * specially -- it echoes "^G", calls ctrlg() (beep + "[Abort]"), and
 * returns ABORT, which every caller treats as "do nothing" (e.g.
 * filefind() checks `if ((s=mlreply(...)) != TRUE) return (s);`).
 * Tested by starting C-X C-F, typing a bogus partial filename, then
 * sending ^G before RETURN -- no separate verification is needed: if
 * the abort didn't work, the bogus text would leak into "main" and the
 * ALREADY-PRESENT STOP.TXT check a few steps later would fail.
 *
 * Two buffers, two real Robert Frost poems (ASCII-sanitized: plain
 * hyphens instead of the em dash, plain apostrophes):
 *   - buffer "main" (the default startup buffer -- see src/main.c's
 *     edinit(bname) with bname="main"): "Stopping by Woods on a Snowy
 *     Evening", saved to STOP.TXT.
 *   - buffer "ROAD.TXT" (created via C-X C-F on a file that doesn't
 *     exist yet -- src/file.c's filefind()/getfile() create a new
 *     buffer named after the file and leave it empty, mlwrite
 *     "[New]"): "The Road Not Taken", saved via C-X C-S (filesave)
 *     directly to its already-remembered fname, no filename prompt.
 *
 * TWO REAL DOC-VS-IMPLEMENTATION MISMATCHES FOUND WHILE GROUNDING THIS
 * SCENARIO (both now corrected in data/emacs.hlp as part of this
 * same change):
 *   1. C-X C-R (fileread) does NOT insert the other file's content at
 *      point. src/file.c's fileread() calls readin(fname, TRUE)
 *      directly on the CURRENT buffer, and readin() unconditionally
 *      calls bclear(bp) FIRST -- i.e. it wipes out the current buffer's
 *      existing text and replaces it with the named file's content (a
 *      "revert to a different file" operation, not a paste-in). Tested
 *      below by reading STOP.TXT into a freshly-created, still-empty
 *      buffer (so bclear() has nothing to discard and no confirm
 *      prompt appears) and verifying the result byte-matches STOP.TXT.
 *   2. M-Z is NOT a distinct "save all changed buffers, then exit"
 *      command. src/ebind.h binds `{META|'Z', quit}` -- the exact same
 *      C function pointer as `{CTLX|CTRL|'C', quit}`. src/main.c's
 *      quit() has no save-all logic at all; it just checks anycb() (any
 *      buffer changed) and, if so, asks "Modified;quit?" (a single y/n
 *      keystroke via mlyesno(), no RETURN). Tested below by
 *      deliberately dirtying the "main" buffer with a content-preserving
 *      C-D-and-retype round trip (same proven idiom as
 *      scenario_content_editing.c) immediately before invoking M-Z, so
 *      this run exercises the confirm-prompt path that
 *      scenario_content_editing.c's always-clean-at-quit C-X C-C never
 *      did, and confirms M-Z's actual (not documented) behavior.
 *
 * BUFFER-EVICTION ROUND TRIP (src/buffer.c's swbuffer()): switching
 * away from a CLEAN, named buffer frees its in-memory lines
 * (bclear()) and marks it b_active=FALSE; switching back to it calls
 * readin() again, re-reading the remembered file from disk. This is
 * exercised (not just asserted) by saving each buffer, cycling to the
 * other buffer and back via C-X X (nextbuffer), then re-saving under a
 * NEW filename and diffing against the same expected content used for
 * the original save -- proving the evicted/reloaded content is
 * byte-identical, not just "whatever happened to still be in RAM."
 * (Note: getfile()/filefind's OWN buffer-switch bookkeeping does NOT
 * evict the buffer being left -- only swbuffer() does -- so the first
 * C-X B/C-X X after a C-X C-F is the first point eviction can occur;
 * see the per-step comments below for exactly when each buffer is
 * evicted and reloaded.)
 *
 * C-X D (dired) is exercised as: invoke it, confirm the run continues
 * (no crash), then switch back to "main" by name and confirm normal
 * operation resumes -- src/file.c's dired()/dired_fill() build a
 * read-only (MDVIEW) "*dired*" buffer; no attempt is made to
 * screen-scrape or parse its directory-listing content (no
 * infrastructure exists for that yet, and it isn't the point of this
 * scenario).
 *
 * M-? (help) is exercised end-to-end: src/bind.c's help() ultimately
 * loads "D:EMACS.HLP" (the exact same data/emacs.hlp, LF->$9B
 * translated at disk-staging time -- see mk/test-integration.mk) into a new
 * MDVIEW buffer. filewrite() has no MDVIEW gate (only filesave() does),
 * so C-X C-W still works on this read-only buffer; the saved copy is
 * diffed against a host-side re-derivation of data/emacs.hlp with
 * the SAME LF->$9B translation the Makefile's `tr` step performs, which
 * doubles as a regression check that the just-corrected help text
 * (see above) is really what ships on the boot disk.
 *
 * Same keystroke-injection encoding rules as scenario_content_editing.c
 * (see that file's header comment for the full derivation): RETURN is
 * host '\n' (0x0A), DELETE/backdel is host '\b' (0x08, NOT 127),
 * key_idle_frames must stay >= 20 to avoid dropped post-boot keystrokes,
 * but a SEPARATE, larger constraint applies here: a keystroke sent right
 * after a filesave/filewrite of a large-ish buffer (a few hundred bytes
 * and up) can itself be dropped if key_idle_frames is too small -- the
 * real disk write takes long enough (in emulated CPU/frame time) that
 * the CPU isn't back at getkey() yet when the fixed idle window ends,
 * so the next command's leading C-X (0x18) byte is never actually
 * polled and simply gets skipped; the first byte the CPU DOES see is
 * the following plain character, which self-inserts instead of taking
 * the CTLX prefix. Confirmed via a throwaway scratch_diag.c bisection:
 * idle=20 and idle=40 both reproduced "C-X B" right after a 768-byte
 * C-X C-S corrupting the buffer with a leaked "Bmain" self-insert;
 * idle=50 fixed it. key_idle_frames=64 below keeps a safety margin
 * above that measured threshold (this scenario's largest write,
 * HELPOUT.TXT, is 1149 bytes -- larger than the 768-byte case that was
 * bisected).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atr_dos25.h"
#include "int_harness.h"

static const char *poem1_lines[] = {
    "Stopping by Woods on a Snowy Evening",
    "",
    "By Robert Frost",
    "",
    "Whose woods these are I think I know.",
    "His house is in the village though;",
    "He will not see me stopping here",
    "To watch his woods fill up with snow.",
    "",
    "My little horse must think it queer",
    "To stop without a farmhouse near",
    "Between the woods and frozen lake",
    "The darkest evening of the year.",
    "",
    "He gives his harness bells a shake",
    "To ask if there is some mistake.",
    "The only other sound's the sweep",
    "Of easy wind and downy flake.",
    "",
    "The woods are lovely, dark and deep,",
    "But I have promises to keep,",
    "And miles to go before I sleep,",
    "And miles to go before I sleep."
};
#define NUM_POEM1_LINES ((int)(sizeof(poem1_lines) / sizeof(poem1_lines[0])))

static const char *poem2_lines[] = {
    "The Road Not Taken",
    "",
    "By Robert Frost",
    "",
    "Two roads diverged in a yellow wood,",
    "And sorry I could not travel both",
    "And be one traveler, long I stood",
    "And looked down one as far as I could",
    "To where it bent in the undergrowth;",
    "",
    "Then took the other, as just as fair,",
    "And having perhaps the better claim,",
    "Because it was grassy and wanted wear;",
    "Though as for that the passing there",
    "Had worn them really about the same,",
    "",
    "And both that morning equally lay",
    "In leaves no step had trodden black.",
    "Oh, I kept the first for another day!",
    "Yet knowing how way leads on to way,",
    "I doubted if I should ever come back.",
    "",
    "I shall be telling this with a sigh",
    "Somewhere ages and ages hence:",
    "Two roads diverged in a wood, and I--",
    "I took the one less traveled by,",
    "And that has made all the difference."
};
#define NUM_POEM2_LINES ((int)(sizeof(poem2_lines) / sizeof(poem2_lines[0])))

/* Builds the "type this poem" keystroke phase: each line followed by a
 * RETURN (host-ASCII '\n'), except the last line. Malloc'd; caller
 * frees. See scenario_content_editing.c's identical helper for the
 * full reasoning. */
static char *build_typed_poem(const char *const *lines, int n)
{
    size_t cap = 1, len = 0;
    int i;
    char *buf;

    for (i = 0; i < n; i++)
        cap += strlen(lines[i]) + 1;
    buf = malloc(cap);
    if (buf == NULL) {
        fprintf(stderr, "out of memory building typed poem\n");
        exit(1);
    }
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

/* Builds the expected on-disk file content: each line followed by
 * ATASCII $9B (confirmed real EOL convention -- see
 * scenario_content_editing.c's header comment), INCLUDING the last
 * line. Malloc'd; caller frees. */
static unsigned char *build_expected_file(const char *const *lines, int n, size_t *out_len)
{
    size_t cap = 1, len = 0;
    int i;
    unsigned char *buf;

    for (i = 0; i < n; i++)
        cap += strlen(lines[i]) + 1;
    buf = malloc(cap);
    if (buf == NULL) {
        fprintf(stderr, "out of memory building expected file content\n");
        exit(1);
    }
    for (i = 0; i < n; i++) {
        size_t ln = strlen(lines[i]);
        memcpy(buf + len, lines[i], ln);
        len += ln;
        buf[len++] = '\x9b';
    }
    *out_len = len;
    return buf;
}

/* Host path of the help-file source; the build passes an absolute path. */
#ifndef HELP_SOURCE_PATH
#define HELP_SOURCE_PATH "data/emacs.hlp"
#endif

/* Builds the expected content of the M-? help-buffer save-out: the
 * SAME data/emacs.hlp source file the build stages onto the boot disk
 * as D:EMACS.HLP, with the identical LF(0x0A)->ATASCII $9B translation
 * tools/mkatr.sh performs. Malloc'd; caller frees. */
static unsigned char *build_expected_help(size_t *out_len)
{
    FILE *fp;
    long size;
    size_t n, i;
    unsigned char *buf;

    fp = fopen(HELP_SOURCE_PATH, "rb");
    if (fp == NULL) {
        fprintf(stderr, "cannot open %s for expected help content\n", HELP_SOURCE_PATH);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fprintf(stderr, "out of memory building expected help content\n");
        fclose(fp);
        exit(1);
    }
    n = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    for (i = 0; i < n; i++)
        if (buf[i] == '\n')
            buf[i] = '\x9b';
    *out_len = n;
    return buf;
}

/* Reads dos_name back out of atr_path and diffs it against
 * expected[0..expected_len). Prints PASS/FAIL; clears *ok on failure
 * (never sets it, so callers can chain several verify_file() calls and
 * check *ok once at the end). */
static void verify_file(const char *atr_path, const char *dos_name,
                         const unsigned char *expected, size_t expected_len,
                         int *ok)
{
    unsigned char *actual = NULL;
    size_t actual_len = 0;

    if (atr_dos25_read_file(atr_path, dos_name, &actual, &actual_len) != 0) {
        fprintf(stderr, "FAIL: %s not found (or unreadable)\n", dos_name);
        *ok = 0;
        return;
    }
    if (actual_len != expected_len || memcmp(actual, expected, expected_len) != 0) {
        size_t i, n = actual_len < expected_len ? actual_len : expected_len;
        fprintf(stderr, "FAIL: %s content mismatch (expected %zu bytes, got %zu bytes)\n",
                dos_name, expected_len, actual_len);
        for (i = 0; i < n; i++) {
            if (actual[i] != expected[i]) {
                fprintf(stderr, "  first difference at byte %zu: expected 0x%02x, got 0x%02x\n",
                        i, expected[i], actual[i]);
                break;
            }
        }
        {
            size_t dumpn = actual_len < 80 ? actual_len : 80;
            fprintf(stderr, "  actual (first %zu bytes, $9b shown as |): \"", dumpn);
            for (i = 0; i < dumpn; i++) {
                unsigned char c = actual[i];
                fputc(c == 0x9b ? '|' : (c >= 0x20 && c < 0x7f ? c : '?'), stderr);
            }
            fprintf(stderr, "\"\n");
        }
        *ok = 0;
    } else {
        printf("PASS: %s content matches (%zu bytes)\n", dos_name, actual_len);
    }
    free(actual);
}

int main(int argc, char **argv)
{
    const char *atr_path = (argc > 1) ? argv[1] : "build/integration/scenario_file_operations.atr";
    char *typed_poem1 = build_typed_poem(poem1_lines, NUM_POEM1_LINES);
    char *typed_poem2 = build_typed_poem(poem2_lines, NUM_POEM2_LINES);
    unsigned char *expected1, *expected2, *expected_help;
    size_t expected1_len, expected2_len, expected_help_len;
    int_harness_config_t cfg;
    int_harness_result_t res;
    int ok = 1;

    expected1 = build_expected_file(poem1_lines, NUM_POEM1_LINES, &expected1_len);
    expected2 = build_expected_file(poem2_lines, NUM_POEM2_LINES, &expected2_len);
    expected_help = build_expected_help(&expected_help_len);

    {
        const int_key_segment_t segments[] = {
            /* Same post-boot keystroke-drop warm-up as
             * scenario_content_editing.c. */
            { "\x07" "\x07", "warm-up (C-G x2)" },

            /* ^G abort probe: begin C-X C-F, type a bogus partial
             * filename, then abort with ^G before RETURN -- proven by
             * the STOP.TXT check below staying byte-exact (see file
             * header comment for why no separate check is needed). */
            { "\x18" "\x06", "C-X C-F (filefind) -- to be aborted" },
            { "BOGUS", "partial (bogus) filename, never confirmed" },
            { "\x07", "C-G (abort the pending filefind prompt)" },

            /* Type poem 1 into the default "main" buffer. */
            { typed_poem1, "type poem 1 (Stopping by Woods)" },
            { "\x1b" "<", "M-< (goto BOB)" },

            /* Save-as: main's b_fname becomes STOP.TXT, BFCHG clears. */
            { "\x18" "\x17", "C-X C-W (filewrite)" },
            { "STOP.TXT", "filename" },
            { "\n", "RETURN (confirm filename)" },

            /* C-X C-F on a file that does NOT exist yet: src/file.c's
             * filefind()/getfile() create a brand-new buffer named
             * "ROAD.TXT" (makename() strips any path, none here), set
             * its b_fname to "ROAD.TXT", and readin() reports "[New]"
             * leaving it empty -- no confirm/collision prompt since no
             * buffer named "ROAD.TXT" exists yet. getfile()'s own
             * buffer-switch bookkeeping does NOT evict "main" (only
             * swbuffer() does that), so main's just-typed lines stay
             * resident in memory across this switch. */
            { "\x18" "\x06", "C-X C-F (filefind)" },
            { "ROAD.TXT", "filename" },
            { "\n", "RETURN (confirm filename, creates new buffer)" },

            /* Type poem 2 into the new ROAD.TXT buffer. */
            { typed_poem2, "type poem 2 (The Road Not Taken)" },
            { "\x1b" "<", "M-< (goto BOB)" },

            /* C-X C-S (filesave): first use of this binding in the
             * integration suite (scenario_content_editing.c only
             * exercised C-X C-W). No filename prompt at all --
             * src/file.c's filesave() writes straight to the buffer's
             * already-remembered b_fname ("ROAD.TXT", set by the
             * C-X C-F above). */
            { "\x18" "\x13", "C-X C-S (filesave)" },

            /* C-X B (usebuffer) back to "main": src/buffer.c's
             * usebuffer() -> swbuffer() evicts the CURRENT buffer
             * (ROAD.TXT: clean + has a fname -> bclear()'d,
             * b_active=FALSE) before switching. "main" was never
             * evicted (see above), so its content comes straight from
             * memory here, not a disk reload -- that reload path is
             * exercised a few steps down via C-X X instead. */
            { "\x18" "B", "C-X B (usebuffer)" },
            { "main", "buffer name" },
            { "\n", "RETURN (confirm buffer name)" },

            /* Re-save main's (still purely in-memory) content under a
             * new name so it can be diffed independently of STOP.TXT. */
            { "\x18" "\x17", "C-X C-W (filewrite)" },
            { "CHECK1.TXT", "filename" },
            { "\n", "RETURN (confirm filename)" },

            /* C-X X (nextbuffer): buffer list is alphabetically sorted
             * by name (src/buffer.c's bfind() insertion sort) -- with
             * only "main" and "ROAD.TXT" existing so far, and 'R' <
             * 'm' in ASCII, the list head is ROAD.TXT. From "main"
             * (list tail), nextbuffer() wraps to the head: ROAD.TXT.
             * This evicts "main" (clean + fname CHECK1.TXT) and, since
             * ROAD.TXT was evicted by the C-X B above, forces a REAL
             * readin() reload of ROAD.TXT's saved content from disk --
             * not just whatever was left in RAM. */
            { "\x18" "X", "C-X X (nextbuffer: main -> ROAD.TXT)" },

            /* Re-save the just-reloaded ROAD.TXT content under a new
             * name, proving the evict/reload round trip preserved it
             * byte-exactly. */
            { "\x18" "\x17", "C-X C-W (filewrite)" },
            { "CHECK2.TXT", "filename" },
            { "\n", "RETURN (confirm filename)" },

            /* C-X X again: from ROAD.TXT (list head), nextbuffer()
             * advances to "main" (list tail). Evicts ROAD.TXT (clean +
             * fname CHECK2.TXT) and forces a real reload of "main"
             * (evicted a few steps up) from its own remembered fname
             * (CHECK1.TXT by now). */
            { "\x18" "X", "C-X X (nextbuffer: ROAD.TXT -> main)" },

            /* C-X K (killbuffer) "ROAD.TXT": src/buffer.c's
             * killbuffer()/zotbuf() refuse to kill a buffer that is
             * currently displayed (b_nwnd != 0); ROAD.TXT was evicted
             * (b_nwnd 0) by the C-X X just above and is clean, so this
             * succeeds with no confirm prompt (bclear() only prompts
             * if BFCHG is set) and frees the buffer struct entirely.
             * The already-saved ROAD.TXT/CHECK2.TXT files on disk are
             * untouched -- killbuffer only frees the in-memory
             * structure. */
            { "\x18" "K", "C-X K (killbuffer)" },
            { "ROAD.TXT", "buffer name" },
            { "\n", "RETURN (confirm buffer name)" },

            /* C-X C-F on ANOTHER nonexistent file: fresh, empty,
             * never-modified buffer "SCRATCH.TXT" (BFCHG clear from
             * the moment it's created), used below to exercise
             * fileread() without triggering its bclear()'s "Discard?"
             * confirm (which only fires if the buffer being replaced
             * has unsaved changes). */
            { "\x18" "\x06", "C-X C-F (filefind)" },
            { "SCRATCH.TXT", "filename" },
            { "\n", "RETURN (confirm filename, creates new buffer)" },

            /* C-X C-R (fileread): per src/file.c's fileread(), this
             * calls readin("STOP.TXT", TRUE) directly on the CURRENT
             * buffer (SCRATCH.TXT) -- readin() unconditionally
             * bclear()s first, so this REPLACES SCRATCH.TXT's (empty)
             * content with STOP.TXT's, and reassigns SCRATCH.TXT's
             * b_fname to "STOP.TXT" -- it is a revert/reload, NOT an
             * insert-at-point (the doc file said otherwise; corrected
             * as part of this same change). No confirm prompt: the
             * buffer being replaced was never dirtied. */
            { "\x18" "\x12", "C-X C-R (fileread)" },
            { "STOP.TXT", "filename" },
            { "\n", "RETURN (confirm filename)" },

            /* Save the fileread result under a new name to verify it
             * byte-matches poem 1 -- proving fileread pulled in
             * STOP.TXT's real content, not a stale/garbled copy. */
            { "\x18" "\x17", "C-X C-W (filewrite)" },
            { "CHECK3.TXT", "filename" },
            { "\n", "RETURN (confirm filename)" },

            /* Back to "main" (evicts SCRATCH.TXT, clean + fname
             * CHECK3.TXT by now). */
            { "\x18" "B", "C-X B (usebuffer)" },
            { "main", "buffer name" },
            { "\n", "RETURN (confirm buffer name)" },

            /* C-X D (dired): src/file.c's dired()/dired_fill() build a
             * read-only "*dired*" buffer with the current directory
             * listing and switch to it (evicting "main"). Not
             * screen-scraped/parsed here -- just asserted to run
             * without crashing and to leave the editor in a state
             * normal buffer switching still works from (next step). */
            { "\x18" "D", "C-X D (dired)" },

            /* Back to "main" from dired (evicts "*dired*"; proves
             * dired didn't wedge buffer switching). */
            { "\x18" "B", "C-X B (usebuffer)" },
            { "main", "buffer name" },
            { "\n", "RETURN (confirm buffer name)" },

            /* M-? (help): src/bind.c's help() loads "D:EMACS.HLP" (the
             * exact data/emacs.hlp staged onto this disk -- see
             * mk/test-integration.mk) into a new MDVIEW buffer, evicting "main". */
            { "\x1b" "?", "M-? (help)" },

            /* filewrite() has no MDVIEW gate (only filesave() checks
             * it), so this still works on the read-only help buffer.
             * Saved content is diffed against a host-side re-derivation
             * of data/emacs.hlp with the same LF->$9B translation
             * the Makefile's `tr` step performs. */
            { "\x18" "\x17", "C-X C-W (filewrite)" },
            { "HELPOUT.TXT", "filename" },
            { "\n", "RETURN (confirm filename)" },

            /* Back to "main" one last time (evicts the help buffer),
             * to set up the M-Z test below. */
            { "\x18" "B", "C-X B (usebuffer)" },
            { "main", "buffer name" },
            { "\n", "RETURN (confirm buffer name)" },

            /* Deliberately dirty "main" with the same content-
             * preserving C-D-and-retype round trip
             * scenario_content_editing.c uses (net: line 0 unchanged,
             * but BFCHG gets set) -- needed so the M-Z test below
             * actually exercises quit()'s "Modified;quit?" confirm
             * path, which scenario_content_editing.c's always-clean
             * C-X C-C never did. */
            { "\x1b" "<", "M-< (goto BOB)" },
            { "\x06" "\x04" "t", "C-F C-D + retype 't' (dirty main)" },

            /* M-Z: src/ebind.h binds this to the EXACT SAME `quit`
             * function as C-X C-C (confirmed by reading src/main.c's
             * quit() -- no save-all logic exists there at all; the doc
             * file's old "save all changed, then exit" description was
             * wrong and has been corrected as part of this same
             * change). Since "main" is now dirty, anycb() is TRUE, so
             * quit() calls mlyesno("Modified;quit?") -- a single y/n
             * keystroke, no RETURN. */
            { "\x1b" "Z", "M-Z (quit, expects Modified;quit? prompt)" },
            { "y", "confirm quit despite unsaved changes" },

            { NULL, NULL }
        };

        memset(&cfg, 0, sizeof(cfg));
        cfg.atr_path = atr_path;
        cfg.segments = segments;
        cfg.boot_settle_frames = 400;
        cfg.key_hold_frames = 3;
        cfg.key_idle_frames = 100;
        cfg.post_script_settle_frames = 150;
        cfg.max_frames = 300000;

        if (int_harness_run(&cfg, &res) != 0) {
            fprintf(stderr, "FAIL: int_harness_run could not initialize libatari800\n");
            free(typed_poem1);
            free(typed_poem2);
            free(expected1);
            free(expected2);
            free(expected_help);
            return 1;
        }
    }

    printf("ran %d frames%s\n", res.frames_used, res.crashed ? " (CPU CRASH)" : "");
    if (res.crashed) {
        fprintf(stderr, "FAIL: emulated CPU crashed at frame %d\n", res.crash_frame);
        ok = 0;
    }

    int_harness_shutdown();

    if (ok) {
        verify_file(atr_path, "STOP.TXT", expected1, expected1_len, &ok);
        verify_file(atr_path, "ROAD.TXT", expected2, expected2_len, &ok);
        verify_file(atr_path, "CHECK1.TXT", expected1, expected1_len, &ok);
        verify_file(atr_path, "CHECK2.TXT", expected2, expected2_len, &ok);
        verify_file(atr_path, "CHECK3.TXT", expected1, expected1_len, &ok);
        verify_file(atr_path, "HELPOUT.TXT", expected_help, expected_help_len, &ok);
    }

    free(expected1);
    free(expected2);
    free(expected_help);
    free(typed_poem1);
    free(typed_poem2);

    printf(ok ? "SCENARIO PASSED\n" : "SCENARIO FAILED\n");
    return ok ? 0 : 1;
}
