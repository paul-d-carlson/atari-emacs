/*
 * test_runner_resident.c
 *
 * Thin native host driver for the disk-overlay test architecture's
 * single resident image (see tests/target/test_main_resident.c). Test
 * dispatch order here MUST exactly match test_main_resident.c's own
 * suite order: test_buffer.c, test_atari.c, test_basic.c, test_bind.c,
 * test_display.c, test_file1.c, test_file2.c, test_file3.c, test_fileio.c,
 * test_line1.c, test_line2.c, test_random1.c, test_random2.c,
 * test_search.c, test_window.c, test_termio.c -- each suite's own tests
 * in the same order as its own OVLENTRY table (test_buffer.c's
 * buffer_tests[], etc).
 *
 * Line-coverage reporting (dbgcov_targets below) works even though every
 * overlay reuses the same OVERLAY_BASE address window: the code we
 * actually measure is the real application (src/buffer.c, atari.c,
 * basic.c, bind.c, display.c, file.c), which is linked ONCE into the resident
 * image at fixed, non-overlapping MAIN-region addresses. Its execution
 * accumulates in atari800's MONITOR_coverage across the whole run
 * (whichever overlay's tests happen to be driving it at the time), and
 * the resident .dbg maps those stable addresses back to source lines
 * unambiguously. Only the tiny per-suite *test* code lives in the shared
 * overlay window, and that is deliberately NOT among the coverage
 * targets. PASS/FAIL reporting does not depend on coverage at all.
 */
#include "harness_runner.h"

static const test_info_t test_info[] = {
    /* test_buffer.c -> src/buffer.c */
    { "bfind_creates_and_finds",          "src/buffer.c", "bfind" },
    { "bfind_sorted_insert",              "src/buffer.c", "bfind" },
    { "bfind_temp_buffer_rejected",       "src/buffer.c", "bfind" },
    { "anycb_reports_changes",            "src/buffer.c", "anycb" },
    { "swbuffer_switches_and_activates",  "src/buffer.c", "swbuffer" },
    { "nextbuffer_cycles_skipping_temp",  "src/buffer.c", "nextbuffer" },
    { "bclear_no_confirmation_needed",    "src/buffer.c", "bclear" },
    { "bclear_confirms_discard_yes",      "src/buffer.c", "bclear" },
    { "bclear_declines_discard_no",       "src/buffer.c", "bclear" },
    { "zotbuf_removes_buffer",            "src/buffer.c", "zotbuf" },
    { "zotbuf_refuses_if_displayed",      "src/buffer.c", "zotbuf" },
    { "killbuffer_reports_missing_buffer","src/buffer.c", "killbuffer" },
    { "killbuffer_kills_existing_buffer", "src/buffer.c", "killbuffer" },
    { "usebuffer_switches_buffer",        "src/buffer.c", "usebuffer" },
    /* test_atari.c -> src/atari.c */
    { "term_dimensions",            "src/atari.c", "term (t_nrow/t_ncol/t_margin/t_scrsiz)" },
    { "move_positions_cursor",       "src/atari.c", "atari_move (t_move)" },
    { "eeol_clears_to_end_of_line",  "src/atari.c", "atari_eeol (t_eeol)" },
    { "eeop_clears_rest_of_screen",  "src/atari.c", "atari_eeop (t_eeop)" },
    { "rev_toggles_reverse_video",   "src/atari.c", "atari_rev (t_rev)" },
    { "beep_does_not_crash",         "src/atari.c", "atari_beep (t_beep)" },
    /* test_basic.c -> src/basic.c */
    { "gotobol_moves_to_start_of_line",            "src/basic.c", "gotobol" },
    { "gotoeol_moves_to_end_of_line",               "src/basic.c", "gotoeol" },
    { "forwchar_moves_within_line",                 "src/basic.c", "forwchar" },
    { "forwchar_crosses_line_boundary",             "src/basic.c", "forwchar" },
    { "forwchar_fails_at_end_of_buffer",            "src/basic.c", "forwchar" },
    { "forwchar_negative_delegates_to_backchar",    "src/basic.c", "forwchar" },
    { "backchar_moves_within_line",                 "src/basic.c", "backchar" },
    { "backchar_crosses_line_boundary",             "src/basic.c", "backchar" },
    { "backchar_fails_at_start_of_buffer",          "src/basic.c", "backchar" },
    { "gotobob_moves_to_beginning_of_buffer",       "src/basic.c", "gotobob" },
    { "gotoeob_moves_to_end_of_buffer",              "src/basic.c", "gotoeob" },
    { "gotoline_jumps_to_requested_line",           "src/basic.c", "gotoline" },
    { "gotoline_rejects_bogus_argument",            "src/basic.c", "gotoline" },
    { "getgoal_expands_tabs",                       "src/basic.c", "getgoal" },
    { "forwline_backline_preserve_goal_column",     "src/basic.c", "forwline / backline" },
    { "forwline_negative_delegates_to_backline",    "src/basic.c", "forwline" },
    { "forwpage_backpage_scroll_to_buffer_bounds",  "src/basic.c", "forwpage / backpage" },
    { "setmark_records_dot",                        "src/basic.c", "setmark" },
    { "swapmark_exchanges_dot_and_mark",            "src/basic.c", "swapmark" },
    { "swapmark_fails_with_no_mark",                "src/basic.c", "swapmark" },
    /* test_bind.c -> src/bind.c */
    { "help_reports_not_online",                    "src/bind.c", "help" },
    { "help_reads_real_help_file",                   "src/bind.c", "help" },
    /* test_display.c -> src/display.c */
    { "update_renders_buffer_line",                 "src/display.c", "update" },
    { "update_erases_shrunk_line_tail",              "src/display.c", "update / updateline" },
    { "modeline_shows_bufname_no_star_when_clean",  "src/display.c", "modeline" },
    { "modeline_shows_star_when_buffer_changed",    "src/display.c", "modeline" },
    { "mlwrite_renders_message_with_mlputi",        "src/display.c", "mlwrite / mlputi" },
    { "mlputs_appends_without_erasing",             "src/display.c", "mlputs" },
    { "movecursor_positions_hardware_cursor",       "src/display.c", "movecursor" },
    { "mlerase_clears_message_line",                "src/display.c", "mlerase" },
    { "vtputc_handles_tab_ctrlchar_and_overflow",   "src/display.c", "vtputc" },
    { "update_extends_line_at_boundary",            "src/display.c", "update / updext" },
    { "update_extends_line_with_far_scroll_and_ctrlchars", "src/display.c", "update / updext / vtpute" },
    { "update_scans_forward_to_find_dot_in_view",   "src/display.c", "update" },
    { "update_reframes_with_various_force_values",  "src/display.c", "update" },
    /* test_display2.c -> src/display.c (split out of test_display.c once
     * it became the largest overlay -- see test_display2.c's header) */
    { "update_single_line_edit_fast_path",          "src/display.c", "update" },
    { "modeline_shows_filename_when_set",           "src/display.c", "modeline" },
    { "mlputi_handles_negative_numbers",            "src/display.c", "mlputi" },
    { "vttidy_cleans_up_message_line",               "src/display.c", "vttidy" },
    { "mlyesno_reprompts_on_invalid_key",           "src/display.c", "mlyesno" },
    { "mlreplyt_erase_and_kill_edit_buffer",        "src/display.c", "mlreplyt" },
    { "mlreplyt_bell_aborts",                       "src/display.c", "mlreplyt" },
    { "mlreplyt_echoes_ctrlchar_and_custom_eol",    "src/display.c", "mlreplyt" },
    /* test_file1.c -> src/file.c */
    { "makename_strips_directory_prefix",              "src/file.c", "makename" },
    { "makename_stops_at_semicolon_version_suffix",    "src/file.c", "makename" },
    { "makename_truncates_long_names",                 "src/file.c", "makename" },
    { "getfile_creates_new_buffer_and_reads_file",     "src/file.c", "getfile / readin" },
    { "getfile_reactivates_evicted_buffer_with_fresh_content", "src/file.c", "getfile / swbuffer" },
    { "readin_reports_new_for_missing_file",           "src/file.c", "readin" },
    { "readin_replaces_buffer_content",                "src/file.c", "readin" },
    { "writeout_writes_buffer_content_to_disk",        "src/file.c", "writeout" },
    /* test_file2.c -> src/file.c */
    { "filesave_skips_when_no_changes",                "src/file.c", "filesave" },
    { "filesave_reports_no_fname",                     "src/file.c", "filesave" },
    { "filesave_blocked_in_readonly_mode",             "src/file.c", "filesave" },
    { "filesave_writes_when_changed",                  "src/file.c", "filesave" },
    { "dired_command_fills_and_switches",              "src/file.c", "dired" },
    { "dired_fill_lists_real_directory",               "src/file.c", "dired_fill" },
    { "readin_wraps_overlong_line",                     "src/file.c", "readin" },
    { "writeout_reports_error_on_bad_device",          "src/file.c", "writeout" },
    /* test_file3.c -> src/file.c (interactive mlreply() wrappers) */
    { "fileread_reads_named_file",                      "src/file.c", "fileread" },
    { "filefind_finds_named_file",                      "src/file.c", "filefind" },
    { "filewrite_writes_named_file",                    "src/file.c", "filewrite" },
    { "filename_sets_buffer_name",                      "src/file.c", "filename" },
    { "filename_blank_clears_name",                     "src/file.c", "filename" },
    /* test_fileio.c -> src/fileio.c (direct, not through file.c's wrappers) */
    { "ffwopen_prepends_device_prefix_when_missing",          "src/fileio.c", "ffwopen / atari_qualify" },
    { "ffwopen_and_ffropen_uppercase_qualified_filename",     "src/fileio.c", "ffwopen / ffropen / atari_qualify" },
    { "atari_qualify_truncates_overlong_filename",            "src/fileio.c", "atari_qualify" },
    { "ffropen_reports_fiofnf_for_missing_file",              "src/fileio.c", "ffropen" },
    { "ffwopen_reports_error_for_bad_device",                 "src/fileio.c", "ffwopen" },
    { "ffclose_without_open_is_safe",                         "src/fileio.c", "ffclose" },
    { "ffputline_ffgetline_roundtrip_multiple_lines",         "src/fileio.c", "ffputline / ffgetline" },
    { "ffgetline_reports_fiolng_for_long_line",               "src/fileio.c", "ffgetline" },
    { "ffputline_and_ffgetline_handle_lines_longer_than_255_bytes", "src/fileio.c", "ffputline / ffgetline" },
    /* test_line1.c -> src/line.c (linsert / lnewline) */
    { "linsert_at_buffer_end_allocates_first_line",            "src/line.c", "linsert" },
    { "linsert_at_buffer_end_with_nonzero_doto_fails",         "src/line.c", "linsert" },
    { "linsert_extends_existing_line_in_place",                "src/line.c", "linsert" },
    { "linsert_mid_line_shifts_tail_in_place",                 "src/line.c", "linsert" },
    { "linsert_mid_line_hard_realloc_shifts_tail",             "src/line.c", "linsert" },
    { "lnewline_splits_line_and_relocates_linep_and_mark",     "src/line.c", "lnewline" },
    { "lnewline_at_line_start_creates_empty_first_half",       "src/line.c", "lnewline / lalloc" },
    { "linsert_and_lnewline_blocked_in_readonly_mode",         "src/line.c", "linsert / lnewline / rdonly" },
    /* test_line2.c -> src/line.c (ldelete / ldelnewline / kill buffer / insspace) */
    { "ldelete_same_line_without_kill",                        "src/line.c", "ldelete" },
    { "ldelete_same_line_with_kill_deposits_chars",            "src/line.c", "ldelete / kinsert" },
    { "ldelete_crosses_newline_easy_merge",                    "src/line.c", "ldelete / ldelnewline" },
    { "ldelete_crosses_newline_hard_merge_reallocates",        "src/line.c", "ldelete / ldelnewline" },
    { "ldelete_at_buffer_end_frees_blank_last_line",           "src/line.c", "ldelete / ldelnewline / lfree" },
    { "ldelete_at_buffer_end_keeps_nonblank_last_line",        "src/line.c", "ldelete / ldelnewline" },
    { "ldelete_blocked_in_readonly_mode",                      "src/line.c", "ldelete / rdonly" },
    { "kinsert_kremove_roundtrip_and_reset",                   "src/line.c", "kinsert / kremove / kdelete" },
    { "kinsert_grows_across_kblock_boundary",                  "src/line.c", "kinsert" },
    { "insspace_inserts_spaces_and_moves_dot_back",            "src/line.c", "insspace" },
    /* test_random1.c -> src/random.c (showcpos / getccol / quote / tab / openline / newline) */
    { "showcpos_reports_position",                             "src/random.c", "showcpos" },
    { "getccol_counts_mixed_content",                          "src/random.c", "getccol" },
    { "getccol_bflg_stops_at_first_nonblank",                  "src/random.c", "getccol" },
    { "quote_inserts_literal_char",                             "src/random.c", "quote" },
    { "quote_inserts_newline",                                  "src/random.c", "quote" },
    { "quote_zero_count_reads_char_no_insert",                 "src/random.c", "quote" },
    { "quote_negative_count_returns_false",                    "src/random.c", "quote" },
    { "quote_blocked_in_readonly_mode",                        "src/random.c", "quote / rdonly" },
    { "tab_inserts_real_tab_when_tabsize_zero",                "src/random.c", "tab" },
    { "tab_sets_tabsize_with_arg",                             "src/random.c", "tab" },
    { "tab_inserts_spaces_to_next_stop",                       "src/random.c", "tab" },
    { "tab_negative_returns_false",                            "src/random.c", "tab" },
    { "openline_inserts_blank_lines_and_returns",              "src/random.c", "openline" },
    { "openline_zero_is_noop",                                 "src/random.c", "openline" },
    { "openline_negative_returns_false",                       "src/random.c", "openline" },
    { "openline_blocked_in_readonly_mode",                     "src/random.c", "openline / rdonly" },
    { "newline_inserts_multiple_lines",                        "src/random.c", "newline" },
    { "newline_negative_returns_false",                        "src/random.c", "newline" },
    { "newline_blocked_in_readonly_mode",                      "src/random.c", "newline / rdonly" },
    /* test_random2.c -> src/random.c (indent / forwdel / backdel / killtext / yank) */
    { "indent_duplicates_space_indentation",                   "src/random.c", "indent" },
    { "indent_duplicates_tab_and_space_indentation",           "src/random.c", "indent" },
    { "indent_negative_returns_false",                         "src/random.c", "indent" },
    { "indent_blocked_in_readonly_mode",                       "src/random.c", "indent / rdonly" },
    { "forwdel_deletes_forward_simple",                        "src/random.c", "forwdel" },
    { "forwdel_kill_mode_deposits_to_kill_buffer",             "src/random.c", "forwdel / kdelete" },
    { "forwdel_negative_delegates_to_backdel",                 "src/random.c", "forwdel" },
    { "forwdel_blocked_in_readonly_mode",                      "src/random.c", "forwdel / rdonly" },
    { "backdel_deletes_backward_simple",                       "src/random.c", "backdel" },
    { "backdel_kill_mode_deposits_to_kill_buffer",             "src/random.c", "backdel / kdelete" },
    { "backdel_fails_at_buffer_start",                         "src/random.c", "backdel" },
    { "backdel_negative_delegates_to_forwdel",                 "src/random.c", "backdel" },
    { "backdel_blocked_in_readonly_mode",                      "src/random.c", "backdel / rdonly" },
    /* test_random3.c -> src/random.c (killtext / yank; split out of
     * test_random2.c once it became the largest overlay -- see
     * test_random3.c's header) */
    { "killtext_kills_to_end_of_line",                         "src/random.c", "killtext" },
    { "killtext_kills_newline_when_at_end_of_line",            "src/random.c", "killtext" },
    { "killtext_kills_from_bol_to_dot_when_n_zero",            "src/random.c", "killtext" },
    { "killtext_kills_n_lines_forward",                        "src/random.c", "killtext" },
    { "killtext_returns_false_when_n_exceeds_remaining_lines", "src/random.c", "killtext" },
    { "killtext_negative_n_reports_neg_message",               "src/random.c", "killtext" },
    { "yank_inserts_previously_killed_text",                   "src/random.c", "yank" },
    { "yank_handles_embedded_newline",                         "src/random.c", "yank" },
    { "yank_negative_returns_false",                           "src/random.c", "yank" },
    { "yank_blocked_in_readonly_mode",                         "src/random.c", "yank / rdonly" },    /* test_search.c -> src/search.c (eq / expandp / forscan / readpattern / forwsearch) */
    { "eq_case_insensitive_matches_by_default",                 "src/search.c", "eq" },
    { "eq_exact_mode_distinguishes_case",                       "src/search.c", "eq" },
    { "expandp_passes_plain_text_unchanged",                    "src/search.c", "expandp" },
    { "expandp_expands_newline_and_percent",                    "src/search.c", "expandp" },
    { "expandp_expands_control_and_del_chars",                  "src/search.c", "expandp" },
    { "expandp_truncates_when_maxlength_exceeded",              "src/search.c", "expandp" },
    { "forscan_finds_match_leaving_point_at_end",               "src/search.c", "forscan" },
    { "forscan_finds_match_leaving_point_at_begin",             "src/search.c", "forscan" },
    { "forscan_matches_across_line_boundary",                   "src/search.c", "forscan" },
    { "forscan_returns_false_when_not_found",                   "src/search.c", "forscan" },
    { "forscan_case_insensitive_by_default",                    "src/search.c", "forscan" },
    { "readpattern_fails_when_blank_and_no_old_pattern",        "src/search.c", "readpattern" },
    { "readpattern_stores_new_pattern",                         "src/search.c", "readpattern" },
    { "readpattern_keeps_old_pattern_on_blank_input",           "src/search.c", "readpattern" },
    { "forwsearch_moves_dot_after_match",                       "src/search.c", "forwsearch" },
    { "forwsearch_repeats_n_times",                             "src/search.c", "forwsearch" },
    { "forwsearch_reports_not_found",                           "src/search.c", "forwsearch" },
    { "forwsearch_aborts_on_ctrl_g",                            "src/search.c", "forwsearch" },
    /* test_window.c -> src/window.c (reposition / refresh) */
    { "reposition_with_f_false_forces_center",                 "src/window.c", "reposition" },
    { "reposition_with_positive_n_sets_force",                 "src/window.c", "reposition" },
    { "reposition_with_negative_n_sets_force",                 "src/window.c", "reposition" },
    { "refresh_with_f_false_sets_sgarbf",                      "src/window.c", "refresh" },
    { "refresh_with_f_true_centers_dot",                       "src/window.c", "refresh" },
    /* test_termio.c -> src/termio.c (ttopen/ttclose/ttflush/ttputc directly,
     * ttgetc/ttgetc_noshow/typahead via scripted keystrokes) */
    { "typahead_reports_false_when_no_key_pending",            "src/termio.c", "typahead" },
    { "ttopen_ttclose_ttflush_are_noops",                      "src/termio.c", "ttopen / ttclose / ttflush" },
    { "ttputc_writes_character_at_cursor_position",            "src/termio.c", "ttputc" },
    { "ttgetc_returns_injected_keycode",                       "src/termio.c", "ttgetc / typahead" },
    { "ttgetc_noshow_returns_injected_keycode",                "src/termio.c", "ttgetc_noshow / typahead" },
};
#define NUM_TEST_NAMES ((int)(sizeof(test_info) / sizeof(test_info[0])))

/* Global (combined-array) index of mlyesno_reprompts_on_invalid_key --
 * test_buffer.c(14) + test_atari.c(6) + test_basic.c(20) + test_bind.c(2)
 * = 42, plus its own local index 17 within test_display.c's own table
 * = 59. Must stay in sync with test_info[] above and every suite's own
 * NUM_*_TESTS. */
#define INTERACTIVE_GATE_TEST 59

/* Global index of the FIRST interactive buffer test
 * (bclear_confirms_discard_yes: buffer.c local index 7, and no earlier
 * suite precedes buffer.c, so global index 7). The first keystroke
 * segment MUST be gated on this rather than streamed from boot (-1):
 * unlike the old split-binary arch, the resident image performs a
 * variable-length D:OVBUF.BIN overlay DISK LOAD between boot and test 0,
 * so streaming from boot let 'y'/'n' land in (and overwrite each other
 * in) the single-byte OS key register while the program was still busy
 * with disk I/O and not yet reading -- desyncing the open-loop feed so
 * bclear_confirms read the wrong key. Gating on test 7 defers the whole
 * segment until the program has actually reached (and blocked in)
 * mlyesno(), immune to how long the overlay load takes. */
#define BCLEAR_GATE_TEST 7

/* Global indices of the five test_file3.c interactive wrapper tests
 * (see test_main_resident.c). Each gets its
 * OWN keystroke segment gated on its own index (see test_file3.c's
 * header): the wrappers do real disk I/O AFTER consuming their
 * filename, during which the program isn't reading the keyboard, so
 * the next test's keys must not be streamed until that test actually
 * starts and blocks in mlreply(). */
#define FILE3_FILEREAD_GATE_TEST   79
#define FILE3_FILEFIND_GATE_TEST   80
#define FILE3_FILEWRITE_GATE_TEST  81
#define FILE3_FILENAME_GATE_TEST   82
#define FILE3_FILEBLANK_GATE_TEST  83

/* Global index of test_quote_inserts_literal_char, the first of
 * test_random1.c's 4 consecutive keystroke-consuming quote() tests.
 * random1 begins at global index 111 (buffer 14 + atari 6 + basic 20 +
 * bind 2 + display 21 + file1 8 + file2 8 + file3 5 + fileio 9 + line1 8
 * + line2 10 = 111); quote_inserts_literal_char is random1's 4th test
 * (0-based local index 3), so 111 + 3 = 114. Must stay in sync with
 * test_info[] above and test_random1.c's own dispatch order. */
#define RANDOM1_QUOTE_GATE_TEST 114

/* Global index of the FIRST of test_search.c's 7 consecutive
 * keystroke-consuming readpattern()/forwsearch() tests
 * (test_readpattern_fails_when_blank_and_no_old_pattern). search.c
 * begins at global index 153 (buffer 14 + atari 6 + basic 20 + bind 2 +
 * display 21 + file1 8 + file2 8 + file3 5 + fileio 9 + line1 8 +
 * line2 10 + random1 19 + random2 23 = 153); that test is search.c's
 * 12th test (0-based local index 11), so 153 + 11 = 164. Unlike
 * test_file3.c's disabled per-test segments, search.c performs no disk
 * I/O between prompts, so all 7 tests safely share this ONE segment
 * (same rationale as INTERACTIVE_GATE_TEST/RANDOM1_QUOTE_GATE_TEST
 * above). Must stay in sync with test_info[] above and
 * test_search.c's own dispatch order. */
#define SEARCH_GATE_TEST 164

/* Global index of test_ttgetc_returns_injected_keycode, the first of
 * test_termio.c's 2 consecutive keystroke-consuming tests
 * (ttgetc/ttgetc_noshow). window.c begins at global index 171 (153 +
 * search.c's 18 = 171); window.c has 5 tests, so termio.c begins at
 * 176; ttgetc_returns_injected_keycode is termio.c's 4th test (0-based
 * local index 3), so 176 + 3 = 179. Must stay in sync with test_info[]
 * above and test_termio.c's own dispatch order. */
#define TERMIO_GATE_TEST 179

/*
 * Line-coverage targets: the real application source files under test,
 * all resident at fixed addresses (see the file comment above for why
 * the shared overlay window doesn't create address ambiguity here).
 * dbgcov_analyze() fills in the coverable/covered counts; suffix uses a
 * leading slash so "/bind.c" can't accidentally match test_bind.c etc.
 */
static dbgcov_target_t dbgcov_targets[] = {
    { "/buffer.c",  "src/buffer.c",  0, 0, 0 },
    { "/atari.c",   "src/atari.c",   0, 0, 0 },
    { "/basic.c",   "src/basic.c",   0, 0, 0 },
    { "/bind.c",    "src/bind.c",    0, 0, 0 },
    { "/display.c", "src/display.c", 0, 0, 0 },
    { "/file.c",    "src/file.c",    0, 0, 0 },
    { "/fileio.c",  "src/fileio.c",  0, 0, 0 },
    { "/line.c",    "src/line.c",    0, 0, 0 },
    { "/random.c",  "src/random.c",  0, 0, 0 },
    { "/search.c",  "src/search.c",  0, 0, 0 },
    { "/window.c",  "src/window.c",  0, 0, 0 },
    { "/termio.c",  "src/termio.c",  0, 0, 0 },
};
#define NUM_DBGCOV_TARGETS ((int)(sizeof(dbgcov_targets) / sizeof(dbgcov_targets[0])))

static const script_segment_t segments[] = {
    { "ZZ"                /* warm-up: absorbs any leading key jitter; mlyesno
                           * ignores these as invalid answers and re-prompts */
      "y"                 /* test_bclear_confirms_discard_yes: "Discard?"  */
      "n"                 /* test_bclear_declines_discard_no:  "Discard?"  */
      "doesnotexist\n"    /* test_killbuffer_reports_missing_buffer        */
      "killme\n"          /* test_killbuffer_kills_existing_buffer         */
      "usetarget\n",      /* test_usebuffer_switches_buffer                */
      BCLEAR_GATE_TEST },
    { "xy"                /* mlyesno_reprompts_on_invalid_key: 'x' ignored,
                           * then 'y' accepted                            */
      "ab\bc\x15" "de\n"  /* mlreplyt_erase_and_kill_edit_buffer:
                           * "ab", backspace, "c", C-U kill, "de", Enter   */
      "\x07"              /* mlreplyt_bell_aborts: C-G                    */
      "\x02\n\x1b",       /* mlreplyt_echoes_ctrlchar_and_custom_eol:
                           * C-B, Enter (echoes <NL>, doesn't terminate),
                           * ESC (the custom eolchar, terminates)         */
      INTERACTIVE_GATE_TEST },
    { "D:TESTFS.TXT\n",   /* fileread: no C-G probe (test_file3.c's current
                           * source calls fileread() only once)           */
      FILE3_FILEREAD_GATE_TEST },
    { "\x07"              /* filefind: C-G aborts the prompt             */
      "D:TESTFS.TXT\n",   /* filefind: getfile() the saved file          */
      FILE3_FILEFIND_GATE_TEST },
    { "\x07"              /* filewrite: C-G aborts the prompt            */
      "D:FW3.TXT\n",      /* filewrite: writeout() to a new file         */
      FILE3_FILEWRITE_GATE_TEST },
    { "\x07"              /* filename: C-G aborts the prompt             */
      "D:NEWNAME.TXT\n",  /* filename: rename the current buffer         */
      FILE3_FILENAME_GATE_TEST },
    { "\n",               /* filename (blank): bare Enter -> FALSE branch */
      FILE3_FILEBLANK_GATE_TEST },
    { "Q"                 /* quote_inserts_literal_char: literal 'Q'       */
      "\n"                /* quote_inserts_newline: '\n'                   */
      "Z"                 /* quote_zero_count_reads_char_no_insert: 'Z'    */
      "Z",                /* quote_negative_count_returns_false: 'Z'       */
      RANDOM1_QUOTE_GATE_TEST },
    { "\x1b"              /* readpattern_fails_when_blank_and_no_old_pattern:
                           * bare ESC, no old pattern -> FALSE              */
      "abc\x1b"           /* readpattern_stores_new_pattern: "abc", ESC     */
      "\x1b"              /* readpattern_keeps_old_pattern_on_blank_input:
                           * bare ESC, keeps "abc"                          */
      "world\x1b"         /* forwsearch_moves_dot_after_match: "world", ESC */
      "la\x1b"            /* forwsearch_repeats_n_times: "la", ESC          */
      "zzz\x1b"           /* forwsearch_reports_not_found: "zzz", ESC       */
      "\x07",             /* forwsearch_aborts_on_ctrl_g: bare C-G          */
      SEARCH_GATE_TEST },
    { "K"                 /* ttgetc_returns_injected_keycode: 'K'          */
      "j",                /* ttgetc_noshow_returns_injected_keycode: 'j'   */
      TERMIO_GATE_TEST },
    { NULL, 0 }
};

int main(int argc, char **argv)
{
    harness_config_t cfg;

    cfg.default_atr_path = "build/unit/test_resident.atr";
    cfg.default_dbg_path = "build/unit/test_resident.dbg";
    cfg.test_info = test_info;
    cfg.num_tests = NUM_TEST_NAMES;
    cfg.segments = segments;
    cfg.dbgcov_targets = dbgcov_targets;
    cfg.num_dbgcov_targets = NUM_DBGCOV_TARGETS;
    cfg.max_frames = 20000;

    return run_harness(argc, argv, &cfg);
}
