/*
 * harness_common.h
 *
 * Shared setup/teardown helpers used by every test_main*.c entry point
 * (one per split test binary -- see tests/driver/harness_runner.h for
 * the host-side half of this split). Kept separate from any one
 * test_main*.c so multiple binaries can share this scaffolding instead
 * of duplicating it.
 */
#ifndef HARNESS_COMMON_H
#define HARNESS_COMMON_H

/* Verbatim copy of src/main.c's edinit(), which src/main.c calls as
 * "vtinit(); edinit(bname);" before entering its command loop. Creates
 * the initial buffer/window pair that every suite runs against. */
void test_edinit(char bname[]);

/*
 * Heap reclamation between (or within) suites.
 *
 * Every test suite creates its own throwaway buffers via bfind(), most
 * with no real b_fname -- so swbuffer()'s own "evict clean named
 * buffers" optimization (which only applies to file-backed buffers,
 * since it relies on readin() being able to reload them later) never
 * reclaims their lines. Left unchecked, buffers from every earlier test
 * pile up on the real (and, on this 8-bit target, very small) heap for
 * the whole run, starving later tests of memory they need for their own
 * real linsert()/ffputline() allocations. None of these test buffers are
 * ever revisited once their own test finishes, so it's safe to free them
 * all (except whichever buffer is currently curbp).
 *
 * zotbuf() only requires b_nwnd==0 (already true for every buffer that
 * isn't the current one, since this harness only ever has a single
 * window) -- but it calls bclear(), which would interactively prompt
 * ("Discard?") for any buffer left with unsaved changes (BFCHG set).
 * That prompt path is already covered by test_buffer.c's dedicated
 * bclear tests; silently clearing BFCHG here first (rather than
 * consuming a real keystroke from the shared script) keeps this cleanup
 * itself non-interactive.
 */
void free_other_buffers(void);

#endif /* HARNESS_COMMON_H */
