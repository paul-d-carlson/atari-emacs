/*
 * scenario_content_editing.c
 *
 * First integration scenario for the real MicroEMACS Atari product
 * (boots the actual AUTORUN.SYS built from src/main.c + the full set of
 * src/ application C files, on a real, freshly-staged DOS 2.5 disk --
 * see mk/test-integration.mk).
 *
 * Covers (per data/emacs.hlp, cross-checked against src/ebind.h):
 *   - typing real content (Robert Frost's "Stopping by Woods on a Snowy
 *     Evening", ASCII-sanitized: plain apostrophe instead of the
 *     Unicode curly quote, title/attribution as plain text)
 *   - movement: C-F C-B C-N C-P C-A C-E C-V C-Z M-< M->
 *   - M-G (gotoline), via the numeric-argument prefix (M-5 M-G jumps to
 *     line 5): verified not just as a no-crash probe but by leaving a
 *     marker string in place at the landed-on line and checking the
 *     final saved file shows it exactly there -- proves gotoline()
 *     really moved dot to the requested line, not just that it ran.
 *   - C-L (refresh) and C-X = (showcpos): both display-only (refresh()
 *     only sets sgarbf; showcpos() only mlwrite()s the message line --
 *     confirmed by reading src/window.c/src/random.c), so these are
 *     verified the same way as the 6 unbound-command probes below: by
 *     the final saved content being unaffected by their presence.
 *   - mark: M-SPACE (setmark)
 *   - editing, each exercised as a content-preserving round trip so the
 *     buffer's FINAL text is provably identical to what was typed:
 *       C-D  (forwdel) immediately retyping the deleted char
 *       DEL  (backdel) immediately retyping the deleted char
 *       C-K + C-Y (killtext then yank -- exact inverse by design)
 *       C-O + C-D (openline splits the line; forwdel's ldelnewline
 *                  merge path immediately undoes the split)
 *   - search: C-S "woods" <ESC> (forward incremental search, readpattern
 *     terminates on ESC per src/search.c, not RETURN)
 *   - the 6 DOCUMENTED-BUT-NOT-IMPLEMENTED commands discovered while
 *     cross-referencing data/emacs.hlp against src/ebind.h -- C-T
 *     (transpose), C-W (kill region), C-R (search backward), M-F/M-B
 *     (word motion), M-W (copy region): none of these exist in
 *     src/ebind.h's keytab, none are in the self-insert ranges either,
 *     so main.c's execute() can only take the "Unbound" fallback for
 *     them -- pressing them is asserted (by construction: none of them
 *     can mutate the buffer) to be a true no-op, i.e. this test
 *     documents the CURRENT REAL BEHAVIOR rather than skipping or
 *     "fixing" the doc/impl mismatch (explicit user decision).
 *   - save (C-X C-W to a new file "POEM.TXT") + quit (C-X C-C)
 *
 * Verification is via atr_dos25_read_file() reading POEM.TXT back out
 * of the resulting .atr on the host side (no reboot needed) and
 * diffing against an expected buffer built from the SAME poem_lines[]
 * array the keystroke script itself is generated from (so there is no
 * separate hand-transcription of the expected text to drift out of
 * sync with what's actually typed).
 *
 * CONFIRMED BY ACTUAL RUN (corrects an earlier, wrong assumption): even
 * though src/fileio.c's ffputline() source calls `fputc('\n', ffp)`,
 * cc65's Atari "D:" CIO text-mode file stream translates that '\n'
 * (0x0A) into ATASCII $9B on write -- Atari's native EOL convention,
 * the same byte a real RETURN keypress produces. Confirmed empirically
 * by hex-dumping an actual saved file from a real run: line breaks are
 * $9B, not raw LF. (The pre-staged EMACS.HLP being LF->$9B `tr`'d
 * during disk staging is consistent with this: it's converting FROM
 * host LF TO the same $9B convention every D: text file uses on this
 * target -- see mk/test-integration.mk.) The expected-content buffer below
 * therefore joins lines with ATASCII $9B, not '\n'.
 *
 * KEYSTROKE INJECTION ENCODING (confirmed by reading
 * atari800's src/libatari800/input.c's
 * PLATFORM_Keyboard()): the `input.keychar` byte fed to libatari800 is
 * a HOST-side key identifier, not the raw ATASCII byte the 6502 program
 * ultimately receives. Plain control bytes 1-26 map directly to
 * AKEY_CTRL_a..z (so ^A-^Z scripting as raw bytes 0x01-0x1A is
 * correct), and 27 maps to AKEY_ESCAPE (so ESC as raw 0x1B is also
 * correct) -- but RETURN and DELETE are special-cased on host-ASCII
 * conventions, NOT the real Atari ATASCII codes: '\n' (0x0A, not $9B)
 * triggers AKEY_RETURN.
 *
 * DELETE IS SUBTLER THAN IT LOOKS (a second, initially-missed bug,
 * root-caused via a standalone isolation harness after the obvious
 * fixes above still left a mystery start-of-buffer corruption): host
 * byte 127 does NOT trigger the real Atari DELETE key. It maps to
 * AKEY_DELETE_CHAR, which akey.h defines as `AKEY_CTRL|AKEY_BACKSPACE`
 * (0xB4) -- the hardware scancode for the CTRL+BACKSPACE combo, a
 * DIFFERENT real key than plain DELETE. libatari800's core
 * (input.c: `POKEY_KBCODE = (UBYTE)INPUT_key_code`) feeds that
 * scancode straight to the OS ROM's real keyboard-to-ATASCII table,
 * which resolves CTRL+BACKSPACE to some other ATASCII byte entirely
 * (NOT $7E) -- so sending 127 silently self-inserts a garbage
 * character instead of deleting anything. The correct host byte for
 * DELETE is `'\b'` (8, ASCII backspace): PLATFORM_Keyboard() maps it to
 * plain AKEY_BACKSPACE (0x34, no ctrl bit set), which the OS ROM's
 * table resolves to real ATASCII $7E -- exactly the byte src/ebind.h
 * binds to `backdel` (`{0x7E, backdel}`, and equivalently `{CTRL|'H',
 * backdel}`, both call the same function). CONFIRMED empirically: a
 * throwaway isolation test (single line + just this one keystroke, no
 * poem, no warm-up, no other edits) reproduced the exact corruption
 * signature with 127 and produced byte-exact correct output once
 * switched to `'\b'`.
 *
 * Any unmatched byte value (e.g. raw $9B) falls through the switch's
 * default and resolves to AKEY_NONE -- a silent no-op, not an error,
 * which is why getting RETURN's encoding wrong doesn't crash anything,
 * it just drops the keystroke; getting DELETE's encoding wrong is
 * worse because 127 lands on a REAL, different, bound key rather than
 * an unbound/no-op one, so it corrupts content instead of just being a
 * no-op.
 *
 * KEY-INJECTION TIMING: with the DEL fix above alone, `key_idle_frames`
 * (frames of idle time the harness lets the emulator run between each
 * injected keystroke) still needed to be raised from an initial value
 * of 6 to 20 -- at 6, individual keystrokes were still occasionally
 * dropped by the OS's keyboard read loop, most visibly in the middle
 * of the multi-step editing round trips below (confirmed via the same
 * isolation technique: an isolated single-line round-trip chain
 * dropped a character at idle=6 and was byte-exact at idle=20). This
 * is a real, separate, orthogonal timing sensitivity from the DEL
 * encoding bug -- it affects any keystroke, not just DEL -- so 20 is
 * kept as the default here rather than the smaller value used by
 * simpler harness call sites.
 *
 * NOT covered here (left to a future scenario, deliberately, to keep
 * this first one focused): C-X C-F/C-X C-R (open/read another file),
 * C-X B/C-X X/C-X K (buffer switch/cycle/kill), C-X D (dired), M-?
 * (help) -- all of these are real, working commands, just not
 * exercised by this scenario yet (see scenario_file_operations.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atr_dos25.h"
#include "int_harness.h"

static const char *poem_lines[] = {
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
#define NUM_POEM_LINES ((int)(sizeof(poem_lines) / sizeof(poem_lines[0])))

/* Builds the "type the poem" keystroke phase: each line followed by a
 * RETURN (host-ASCII '\n', which libatari800 recognizes as
 * AKEY_RETURN -- see the file header comment), except the very last
 * line (so the buffer ends up with exactly NUM_POEM_LINES real LINE
 * objects, no extra trailing empty one). Returned buffer is malloc'd;
 * caller frees it. */
static char *build_typed_poem(void)
{
    size_t cap = 1, len = 0;
    int i;
    char *buf;

    for (i = 0; i < NUM_POEM_LINES; i++)
        cap += strlen(poem_lines[i]) + 1;
    buf = malloc(cap);
    if (buf == NULL) {
        fprintf(stderr, "out of memory building typed poem\n");
        exit(1);
    }
    for (i = 0; i < NUM_POEM_LINES; i++) {
        size_t n = strlen(poem_lines[i]);
        memcpy(buf + len, poem_lines[i], n);
        len += n;
        if (i != NUM_POEM_LINES - 1)
            buf[len++] = '\n';
    }
    buf[len] = '\0';
    return buf;
}

/* Builds the expected on-disk file content: each line followed by
 * ATASCII $9B (Atari's native EOL, confirmed by actual run -- see the
 * file header comment), INCLUDING the last line -- ffputline() is
 * called once per LINE object unconditionally. */
static unsigned char *build_expected_file(size_t *out_len)
{
    size_t cap = 1, len = 0;
    int i;
    unsigned char *buf;

    for (i = 0; i < NUM_POEM_LINES; i++)
        cap += strlen(poem_lines[i]) + 1;
    buf = malloc(cap);
    if (buf == NULL) {
        fprintf(stderr, "out of memory building expected file content\n");
        exit(1);
    }
    for (i = 0; i < NUM_POEM_LINES; i++) {
        size_t n = strlen(poem_lines[i]);
        memcpy(buf + len, poem_lines[i], n);
        len += n;
        buf[len++] = '\x9b';
    }
    *out_len = len;
    return buf;
}

int main(int argc, char **argv)
{
    const char *atr_path = (argc > 1) ? argv[1] : "build/integration/scenario_content_editing.atr";
    char *typed_poem = build_typed_poem();
    unsigned char *expected;
    size_t expected_len;
    unsigned char *actual = NULL;
    size_t actual_len = 0;
    int_harness_config_t cfg;
    int_harness_result_t res;
    int ok = 1;

    expected = build_expected_file(&expected_len);

    /* Splice the M-G/gotoline marker in at the byte offset just past
     * lines 0-3 (each including its $9b terminator) -- i.e. right
     * before line 5 (poem_lines[4]), matching where the script below
     * lands via "M-5 M-G". */
    {
        static const char marker[] = "HERE:";
        const size_t marker_len = sizeof(marker) - 1;
        size_t offset = 0;
        int i;
        unsigned char *spliced;

        for (i = 0; i < 4; i++)
            offset += strlen(poem_lines[i]) + 1;
        spliced = malloc(expected_len + marker_len);
        if (spliced == NULL) {
            fprintf(stderr, "out of memory splicing gotoline marker\n");
            exit(1);
        }
        memcpy(spliced, expected, offset);
        memcpy(spliced + offset, marker, marker_len);
        memcpy(spliced + offset + marker_len, expected + offset, expected_len - offset);
        free(expected);
        expected = spliced;
        expected_len += marker_len;
    }

    /* Segment list: order matters, gating is purely sequential (no
     * results-struct gate exists in the real product -- see
     * int_harness.h). Every editing op is either a proven no-op on
     * buffer content, or paired with an exact undo -- see file header
     * comment for the reasoning behind each pairing. */
    {
        const int_key_segment_t segments[] = {
            /* Real Atari OS keyboard emulation drops the first couple
             * of injected keystrokes right after boot (same quirk the
             * unit-test harness works around) -- C-G (ctrlg/abort) is
             * used here instead of the unit tests' "ZZ" trick, since
             * there is no prompt in the real command loop to silently
             * swallow stray printable chars; C-G just beeps and shows
             * "[Abort]" with no buffer mutation, so it's safe even if
             * it lands for real instead of being dropped. */
            { "\x07" "\x07", "warm-up (C-G x2)" },

            /* Type the whole poem. */
            { typed_poem, "type poem" },

            /* Reset to a known anchor before the movement tour. */
            { "\x1b" "<", "M-< (goto BOB)" },

            /* Non-mutating movement tour. */
            { "\x06" "\x02" "\x0e" "\x10" "\x01" "\x05" "\x16" "\x1a",
              "C-F C-B C-N C-P C-A C-E C-V C-Z" },
            { "\x1b" ">", "M-> (goto EOB)" },
            { "\x1b" "<", "M-< (goto BOB, re-anchor for edits)" },

            /* The 6 documented-but-unbound commands (see file header):
             * none of these can mutate the buffer (they're neither in
             * keytab nor in execute()'s self-insert ranges), so
             * including them here is itself the "assert unbound / no
             * mutation" check, without needing to decode the message
             * line's screen bytes. */
            { "\x14", "C-T (transpose -- documented, unbound)" },
            { "\x17", "C-W (kill region -- documented, unbound)" },
            { "\x12", "C-R (search backward -- documented, unbound)" },
            { "\x1b" "F", "M-F (word forward -- documented, unbound)" },
            { "\x1b" "B", "M-B (word backward -- documented, unbound)" },
            { "\x1b" "W", "M-W (copy region -- documented, unbound)" },

            /* Mark (no consequence without a working kill-region to
             * pair it with -- M-W is unbound, see above). */
            { "\x1b" " ", "M-SPACE (setmark)" },

            /* Forward incremental search; dot is at BOB so this always
             * finds the first "woods" (line 5, case-folded). Pattern
             * entry ends on ESC, not RETURN -- see src/search.c's
             * readpattern(). */
            { "\x13", "C-S (start search)" },
            { "woods", "search pattern" },
            { "\x1b", "ESC (end search pattern)" },

            /* Re-anchor to BOB so the following edits are at a known,
             * deterministic position regardless of where the search
             * landed. */
            { "\x1b" "<", "M-< (re-anchor for edits)" },

            /* C-D round trip: advance one char (dot=1, between the
             * poem's 'S' and 't'), delete the char forward of dot
             * ('t'), retype it. Net: line 0 unchanged, dot ends at 2. */
            { "\x06" "\x04" "t", "C-F C-D + retype 't'" },

            /* DEL round trip: advance one more char (dot=3, between
             * 'o' and 'p'), delete the char BEHIND dot ('o' via
             * backdel), retype it. Net: line 0 unchanged, dot ends at 3.
             * DEL/backdel is host-ASCII 8 ('\b', AKEY_BACKSPACE), NOT
             * 127 -- see the file header comment. */
            { "\x06" "\x08" "o", "C-F BACKSPACE + retype 'o'" },

            /* C-K / C-Y round trip: kill from dot (mid-line-0, not at
             * EOL) to end of line 0 (no newline included in the kill,
             * per src/random.c's killtext()), then yank it straight
             * back at the same dot -- exact inverse, line 0 fully
             * restored. */
            { "\x0b" "\x19", "C-K C-Y (kill-to-EOL + yank)" },

            /* C-O / C-D round trip: dot now sits at the end of line 0
             * (right where C-Y's yank left it). openline() splits a
             * new blank line in at dot then backs up over it, leaving
             * dot right before the just-inserted line break; C-D
             * (forwdel) deletes forward across that break, taking
             * ldelnewline()'s merge path and undoing the split exactly. */
            { "\x0f" "\x04", "C-O C-D (openline + merge back)" },

            /* M-G (gotoline): re-anchor, then jump to line 5 ("Whose
             * woods these are I think I know.") via a numeric-argument
             * prefix -- M-5 sets n=5 in main.c's META-# digit loop,
             * then a SECOND M- prefix supplies the actual command
             * (getkey() folds each ESC+next-byte into one META|c
             * token itself, so the digit loop's own getkey() call
             * sees METACH again for "G", not a bare unprefixed 'G').
             * The marker text is left in place (not undone) so the
             * final saved file proves dot really landed on line 5,
             * not just that gotoline() ran without crashing. */
            { "\x1b" "<", "M-< (re-anchor before gotoline)" },
            { "\x1b" "5" "\x1b" "G", "M-5 M-G (gotoline: line 5)" },
            { "HERE:", "marker text (proves gotoline landed on line 5)" },

            /* C-L (refresh) and C-X = (showcpos): both confirmed
             * display-only (refresh() only sets sgarbf; showcpos()
             * only mlwrite()s the message line -- see src/window.c /
             * src/random.c), so no separate verification is needed
             * beyond the final saved content still matching exactly. */
            { "\x0c", "C-L (refresh, display-only)" },
            { "\x18" "=", "C-X = (showcpos, display-only)" },

            /* Save to a new file, then quit. Buffer is clean right
             * after the write (writeout() succeeded), so C-X C-C's
             * quit() sees anycb()==FALSE and exits without a confirm
             * prompt. */
            { "\x18" "\x17", "C-X C-W (filewrite)" },
            { "POEM.TXT", "filename" },
            { "\n", "RETURN (confirm filename)" },
            { "\x18" "\x03", "C-X C-C (quit)" },

            { NULL, NULL }
        };

        memset(&cfg, 0, sizeof(cfg));
        cfg.atr_path = atr_path;
        cfg.segments = segments;
        cfg.boot_settle_frames = 400;
        cfg.key_hold_frames = 3;
        cfg.key_idle_frames = 20;
        cfg.post_script_settle_frames = 150;
        cfg.max_frames = 40000;

        if (int_harness_run(&cfg, &res) != 0) {
            fprintf(stderr, "FAIL: int_harness_run could not initialize libatari800\n");
            free(typed_poem);
            free(expected);
            return 1;
        }
    }

    printf("ran %d frames%s\n", res.frames_used, res.crashed ? " (CPU CRASH)" : "");
    if (res.crashed) {
        fprintf(stderr, "FAIL: emulated CPU crashed at frame %d\n", res.crash_frame);
        ok = 0;
    }
    /* --- TEMP DEBUG: dump the message line + a couple of buffer rows --- */
    if (getenv("SCENARIO_DEBUG") != NULL) {
        unsigned savmsc = res.mem[0x58] | (res.mem[0x59] << 8);
        int row;
        fprintf(stderr, "SAVMSC=0x%04x\n", savmsc);
        for (row = 0; row < 24; row++) {
            unsigned off = savmsc + (unsigned)row * 40;
            int col;
            fprintf(stderr, "row%2d:", row);
            for (col = 0; col < 40; col++)
                fprintf(stderr, " %02x", res.mem[off + col]);
            fprintf(stderr, "\n");
        }
    }

    int_harness_shutdown();

    if (ok) {
        if (atr_dos25_read_file(atr_path, "POEM.TXT", &actual, &actual_len) != 0) {
            fprintf(stderr, "FAIL: POEM.TXT not found (or unreadable) on %s\n", atr_path);
            ok = 0;
        } else if (actual_len != expected_len ||
                   memcmp(actual, expected, expected_len) != 0) {
            size_t i, n = actual_len < expected_len ? actual_len : expected_len;
            fprintf(stderr, "FAIL: POEM.TXT content mismatch (expected %zu bytes, got %zu bytes)\n",
                    expected_len, actual_len);
            fprintf(stderr, "  expected[0..60): %.60s\n", (char *)expected);
            fprintf(stderr, "  actual[0..60]  : %.60s\n", (char *)actual);
            if (getenv("SCENARIO_DEBUG") != NULL) {
                size_t k;
                fprintf(stderr, "  expected hex (%zu bytes):\n", expected_len);
                for (k = 0; k < expected_len; k++)
                    fprintf(stderr, "%02x ", expected[k]);
                fprintf(stderr, "\n  actual hex (%zu bytes):\n", actual_len);
                for (k = 0; k < actual_len; k++)
                    fprintf(stderr, "%02x ", actual[k]);
                fprintf(stderr, "\n");
            }
            for (i = 0; i < n; i++) {
                if (actual[i] != expected[i]) {
                    fprintf(stderr, "  first difference at byte %zu: expected 0x%02x, got 0x%02x\n",
                            i, expected[i], actual[i]);
                    break;
                }
            }
            ok = 0;
        } else {
            printf("PASS: POEM.TXT content matches (%zu bytes)\n", actual_len);
        }
    }

    free(actual);
    free(expected);
    free(typed_poem);

    printf(ok ? "SCENARIO PASSED\n" : "SCENARIO FAILED\n");
    return ok ? 0 : 1;
}
