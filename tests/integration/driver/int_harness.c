/*
 * int_harness.c -- see int_harness.h.
 */
#include <stdio.h>
#include <string.h>

#include "libatari800.h"
#include "int_harness.h"

int int_harness_run(const int_harness_config_t *cfg, int_harness_result_t *out)
{
    char *init_args[4];
    input_template_t input;
    int seg = 0;
    size_t pos = 0;
    int hold = 0;
    int idle;
    int frame;
    int script_done_frame = -1;

    memset(out, 0, sizeof(*out));

    /* argv[0] deliberately omitted -- see harness_runner.c's comment on
     * why libatari800_init()'s own inserted placeholder must be used
     * instead of supplying one ourselves. */
    init_args[0] = "-config";
    init_args[1] = "/dev/null";
    init_args[2] = (char *)cfg->atr_path;
    init_args[3] = NULL;

    if (!libatari800_init(-1, init_args)) {
        fprintf(stderr, "libatari800_init failed: %s\n", libatari800_error_message());
        return -1;
    }

    out->mem = libatari800_get_main_memory_ptr();
    libatari800_clear_input_array(&input);

    idle = cfg->key_idle_frames; /* allow the first key once boot settles */

    for (frame = 0; frame < cfg->max_frames; frame++) {
        int rc;

        if (frame < cfg->boot_settle_frames) {
            input.keychar = 0;
        } else if (hold > 0) {
            hold--;
            if (hold == 0)
                input.keychar = 0;
        } else if (idle < cfg->key_idle_frames) {
            idle++;
            input.keychar = 0;
        } else {
            /* Skip over fully-consumed segments. */
            while (cfg->segments[seg].chars != NULL &&
                   cfg->segments[seg].chars[pos] == '\0') {
                seg++;
                pos = 0;
            }
            if (cfg->segments[seg].chars != NULL) {
                input.keychar = (unsigned char)cfg->segments[seg].chars[pos++];
                hold = cfg->key_hold_frames;
                idle = 0;
            } else {
                input.keychar = 0;
                if (script_done_frame < 0)
                    script_done_frame = frame;
            }
        }

        /* See harness_runner.c's note: libatari800_next_frame() returns
         * FALSE (0) whenever libatari800_error_code was set this frame;
         * transient DLIST errors during boot are normal, only a CPU
         * crash is fatal. */
        rc = libatari800_next_frame(&input);
        if (rc == 0 && libatari800_error_code == LIBATARI800_CPU_CRASH) {
            out->crashed = 1;
            out->crash_frame = frame;
            out->frames_used = frame + 1;
            fprintf(stderr, "CPU CRASH at frame %d: %s\n", frame,
                    libatari800_error_message());
            return 0;
        }

        if (script_done_frame >= 0 &&
            frame - script_done_frame >= cfg->post_script_settle_frames)
            break;
    }

    out->frames_used = frame;
    if (script_done_frame < 0)
        fprintf(stderr, "int_harness: keystroke script never finished "
                "(hit max_frames=%d)\n", cfg->max_frames);
    return 0;
}

void int_harness_shutdown(void)
{
    libatari800_exit();
}
