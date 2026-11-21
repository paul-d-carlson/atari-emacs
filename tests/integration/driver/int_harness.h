/*
 * int_harness.h
 *
 * A NEW, separate libatari800 boot/frame-step/keystroke-injection engine
 * for driving the REAL MicroEMACS product (main.c + the full set of
 * src/ application C files, built from an AUTORUN.SYS on a real DOS 2.5
 * boot disk) -- as opposed to
 * tests/driver/harness_runner.c, which is deliberately left untouched
 * and cannot be reused unmodified here because it is tightly coupled to
 * common/test_protocol.h's test_results_t (used for boot-detection,
 * per-segment gating, and completion-detection), a protocol the real
 * product never writes.
 *
 * The real product's main loop (src/main.c) is a simple, synchronous,
 * blocking loop -- update(); c = getkey(); ...; execute(c,f,n); goto
 * loop; -- with no typeahead/race condition to gate against. So this
 * engine uses plain frame-count-based timing instead of a results
 * struct: wait boot_settle_frames, then feed the keystroke script with
 * fixed hold/idle frame timing, then wait post_script_settle_frames
 * (to let a final disk write/read finish) before stopping.
 *
 * Reuses the same libatari800_init()/libatari800_next_frame()/
 * libatari800_get_main_memory_ptr() calling convention proven by
 * harness_runner.c (see its comments for why argv[0] is omitted, and
 * why LIBATARI800_CPU_CRASH is the only fatal libatari800_error_code).
 */
#ifndef INT_HARNESS_H
#define INT_HARNESS_H

/* One logical phase of a keystroke script. chars is a NUL-terminated
 * string of literal bytes to feed to input.keychar, one per key-slot
 * (control chars are single bytes 0x01-0x1A, a META prefix is a raw
 * METACH/ESC byte (0x1B) followed by the literal next char, C-X prefix
 * is a raw 0x18 followed by the literal next char). RETURN and DELETE
 * use HOST-ASCII conventions, not the real Atari ATASCII codes --
 * confirmed by reading libatari800/input.c's PLATFORM_Keyboard(): RETURN
 * is '\n' (0x0A, NOT $9B) and DELETE is 127 (0x7F, NOT $7E); any other
 * unmatched byte value (including raw $9B/$7E) silently resolves to
 * AKEY_NONE (a dropped keystroke, not an error). The OS still delivers
 * real ATASCII $9B/$7E to the running 6502 program once these are
 * recognized -- this is purely a host-injection-layer encoding detail.
 * A NULL-chars entry terminates the segment list.
 */
typedef struct {
    const char *chars;
    const char *label; /* for diagnostics only */
} int_key_segment_t;

typedef struct {
    const char *atr_path;
    const int_key_segment_t *segments;   /* NULL-chars-terminated array */
    int boot_settle_frames;              /* frames to wait before 1st key */
    int key_hold_frames;                 /* frames a key stays "pressed" */
    int key_idle_frames;                 /* frames of no key between keys */
    int post_script_settle_frames;       /* extra frames after last key */
    int max_frames;                      /* hard safety cap */
} int_harness_config_t;

typedef struct {
    unsigned char *mem;   /* emulated 64K RAM; valid until int_harness_shutdown() */
    int frames_used;
    int crashed;
    int crash_frame;
} int_harness_result_t;

/* Boots cfg->atr_path under libatari800, runs the full keystroke script,
 * and fills *out. Returns 0 if the run completed (whether or not the
 * emulated program crashed -- check out->crashed), -1 if libatari800
 * itself failed to initialize. Caller must call int_harness_shutdown()
 * exactly once afterward (whether this returns 0 or -1... only call it
 * if this returned 0; on -1 libatari800 was never successfully inited).
 */
int int_harness_run(const int_harness_config_t *cfg, int_harness_result_t *out);

void int_harness_shutdown(void);

#endif /* INT_HARNESS_H */
