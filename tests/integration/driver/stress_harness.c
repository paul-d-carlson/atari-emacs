/*
 * stress_harness.c -- see stress_harness.h.
 *
 * Boot/feed logic deliberately duplicated (not shared) from
 * int_harness.c's proven frame-step loop, rather than refactored to a
 * common helper -- see stress_harness.h's header comment for why this
 * file must not risk perturbing the already-bisected scenario timings.
 */
#include <stdio.h>
#include <string.h>

#include "libatari800.h"
#include "stress_harness.h"

static input_template_t g_input;
static int g_booted = 0;

int stress_boot(const char *atr_path, int boot_settle_frames, stress_result_t *out)
{
    char *init_args[4];
    int frame;

    memset(out, 0, sizeof(*out));

    init_args[0] = "-config";
    init_args[1] = "/dev/null";
    init_args[2] = (char *)atr_path;
    init_args[3] = NULL;

    if (!libatari800_init(-1, init_args)) {
        fprintf(stderr, "libatari800_init failed: %s\n", libatari800_error_message());
        return -1;
    }

    out->mem = libatari800_get_main_memory_ptr();
    libatari800_clear_input_array(&g_input);
    g_booted = 1;

    for (frame = 0; frame < boot_settle_frames; frame++) {
        int rc = libatari800_next_frame(&g_input);
        if (rc == 0 && libatari800_error_code == LIBATARI800_CPU_CRASH) {
            out->crashed = 1;
            out->crash_frame = frame;
            out->total_frames = frame + 1;
            fprintf(stderr, "CPU CRASH during boot at frame %d: %s\n", frame,
                    libatari800_error_message());
            return 0;
        }
    }
    out->total_frames = boot_settle_frames;
    return 0;
}

int stress_feed(const int_key_segment_t *segments, int key_hold_frames,
                 int key_idle_frames, int post_script_settle_frames,
                 int max_extra_frames, stress_result_t *io)
{
    int seg = 0;
    size_t pos = 0;
    int hold = 0;
    int idle = key_idle_frames; /* allow the first key immediately */
    int script_done_frame = -1;
    int frame;

    if (!g_booted)
        return -1;
    io->crashed = 0;
    io->crash_frame = 0;

    for (frame = 0; frame < max_extra_frames; frame++) {
        int rc;

        if (hold > 0) {
            hold--;
            if (hold == 0)
                g_input.keychar = 0;
        } else if (idle < key_idle_frames) {
            idle++;
            g_input.keychar = 0;
        } else {
            while (segments[seg].chars != NULL && segments[seg].chars[pos] == '\0') {
                seg++;
                pos = 0;
            }
            if (segments[seg].chars != NULL) {
                g_input.keychar = (unsigned char)segments[seg].chars[pos++];
                hold = key_hold_frames;
                idle = 0;
            } else {
                g_input.keychar = 0;
                if (script_done_frame < 0)
                    script_done_frame = frame;
            }
        }

        rc = libatari800_next_frame(&g_input);
        io->total_frames++;
        if (rc == 0 && libatari800_error_code == LIBATARI800_CPU_CRASH) {
            io->crashed = 1;
            io->crash_frame = io->total_frames;
            fprintf(stderr, "CPU CRASH at cumulative frame %d: %s\n",
                    io->total_frames, libatari800_error_message());
            return 0;
        }

        if (script_done_frame >= 0 && frame - script_done_frame >= post_script_settle_frames)
            break;
    }

    if (script_done_frame < 0)
        fprintf(stderr, "stress_harness: batch never finished (hit max_extra_frames=%d)\n",
                max_extra_frames);
    return 0;
}

void stress_shutdown(void)
{
    if (g_booted) {
        libatari800_exit();
        g_booted = 0;
    }
}
