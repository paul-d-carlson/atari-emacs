/*
 * abi_table.h
 *
 * Fixed target addresses and slot numbering for the disk-overlay test
 * architecture's cross-image call table.
 *
 * The resident test image (app code + harness + loader, see
 * test_main_resident.c) is the ONLY image that ever links the real
 * application source files under src/. Each test suite (test_buffer.c,
 * test_atari.c, test_basic.c, test_bind.c, test_display.c) is instead compiled and
 * linked as its own tiny, separate, disk-loadable "overlay" image (see
 * test-overlay.cfg) that gets fread() into a shared, fixed-address
 * memory window (OVERLAY_BASE/OVERLAY_SIZE) one suite at a time by
 * overlay_loader.c, and invoked directly at that fixed address.
 *
 * Because an overlay is linked completely separately from the resident
 * image, it cannot call bfind()/mlwrite()/curbp/term/etc by their real
 * names -- nothing in the overlay's own link defines those symbols.
 * Instead, the resident image populates a small array of `void *`
 * (function addresses, or addresses-of-globals) at another fixed
 * address (ABI_TABLE_ADDR, see abi_export.c), and every overlay suite
 * reaches resident state ONLY through resident_api.h's macros, which
 * index into this table by slot number.
 *
 * CRITICAL: OVERLAY_BASE, OVERLAY_SIZE, and ABI_TABLE_ADDR here MUST
 * exactly match the corresponding address arithmetic in both
 * test-resident.cfg and test-overlay.cfg (__STACKSIZE__/__RESERVED_MEMORY__
 * derived). They are re-derived by hand below (not computed from the
 * linker) purely because this header is shared by ordinary cc65 C code,
 * which cannot read ld65 config symbols -- if either config's constants
 * change, update these to match, and vice versa.
 */
#ifndef ABI_TABLE_H
#define ABI_TABLE_H

/*
 * The overlay window + ABI table occupy the whole __RESERVED_MEMORY__
 * region ($14A2 bytes) that sits ABOVE cc65's C software stack. On the
 * Atari target the runtime sets the initial software stack pointer to
 * MEMTOP - __RESERVED_MEMORY__ and grows it DOWNWARD, so everything
 * from MEMTOP+1 - __RESERVED_MEMORY__ ($A77E) up to MEMTOP+1 ($BC20) is
 * safely above the live stack. OVERLAY_BASE must therefore be
 * $BC20 - __RESERVED_MEMORY__ ($A77E) -- NOT additionally offset by
 * __STACKSIZE__ (an earlier version did, which placed the bottom $100
 * of this window on top of the live stack, so overlay_load()'s memset()
 * clobbered its own stack frame and the following read() hung).
 */
#define OVERLAY_BASE     0xA77E   /* $BC20 - $14A2 (== MEMTOP+1 - __RESERVED_MEMORY__) */
#define OVERLAY_SIZE     0x13EC   /* 5100 bytes -- sized to the largest suite (test_display.c,
                                   * 4890 bytes), built WITHOUT -Or/--register-space since
                                   * overlay builds can't safely share resident's own
                                   * register-space liveness analysis, plus headroom for future
                                   * test growth. Deliberately kept as tight as safely possible:
                                   * every byte NOT reserved here is returned to the resident
                                   * heap. History: shrunk from $1BEA to $1B00 to $1A50 to $1400
                                   * during development, chasing a
                                   * heap-corruption regression root-caused to this margin --
                                   * growing test_main_resident.c's dispatch code (however
                                   * slightly) shifts __heapfirst/the stack floor by a few bytes,
                                   * and this whole architecture runs close enough to the
                                   * razor-thin stack/heap collision margin (see the file1/file2
                                   * split history) that ANY resident growth can tip a deep call
                                   * chain into corrupting a live heap block. Most recently shrunk
                                   * by 20 bytes ($1400 -> $13EC) to make room for 10 new ABI slots
                                   * (window.c's reposition/refresh, termio.c's
                                   * ttopen/ttclose/ttflush/ttputc/ttgetc/ttgetc_noshow/typahead,
                                   * and the shared sgarbf global) added for the window.c/termio.c
                                   * test suites -- OVERLAY_SIZE + ABI_TABLE_SIZE must always sum
                                   * to the fixed __RESERVED_MEMORY__ region ($14A2). */
#define ABI_TABLE_ADDR   0xBB6A   /* $BC20 - $00B6 (immediately above the overlay window) */
#define ABI_TABLE_SIZE   0x00B6   /* 182 bytes = 91 slots * 2 bytes/slot  */

/* Slot numbers into abi_table[] (tests/target/abi_export.c). Append-only:
 * adding a new suite that needs a new resident symbol should add a new
 * slot at the END of this list, never renumber existing slots (every
 * already-built overlay's compiled macros bake in a slot's numeric
 * offset). */
enum {
    /* -- global data (table stores &real_variable) -- */
    ABI_CURBP = 0,
    ABI_CURWP,
    ABI_BHEADP,
    ABI_THISFLAG,
    ABI_LASTFLAG,
    ABI_CURGOAL,
    ABI_TERM,
    ABI_MPRESF,

    /* -- functions (table stores the function's own address) -- */
    ABI_BFIND,
    ABI_ANYCB,
    ABI_SWBUFFER,
    ABI_NEXTBUFFER,
    ABI_BCLEAR,
    ABI_ZOTBUF,
    ABI_KILLBUFFER,
    ABI_USEBUFFER,
    ABI_GOTOBOL,
    ABI_GOTOEOL,
    ABI_FORWCHAR,
    ABI_BACKCHAR,
    ABI_GOTOBOB,
    ABI_GOTOEOB,
    ABI_GOTOLINE,
    ABI_GETGOAL,
    ABI_FORWLINE,
    ABI_BACKLINE,
    ABI_FORWPAGE,
    ABI_BACKPAGE,
    ABI_SETMARK,
    ABI_SWAPMARK,
    ABI_LINSERT,
    ABI_LNEWLINE,
    ABI_LDELETE,
    ABI_HELP,
    ABI_FFWOPEN,
    ABI_FFPUTLINE,
    ABI_FFCLOSE,
    ABI_MLWRITE,
    ABI_MLPUTS,
    ABI_MLERASE,
    ABI_MOVECURSOR,
    ABI_UPDATE,
    ABI_MLYESNO,
    ABI_MLREPLYT,
    ABI_VTTIDY,

    /* cc65 conio: the resident's single, boot-initialised copy. Overlays
     * MUST NOT use their own linked-in copy of cputs()/cputc() for
     * anything that shares conio state (cursor/reverse-video) with
     * resident rendering -- the overlay's copy has separate module state,
     * so a reference char/string it paints lands with the wrong
     * cursor/reverse mode (e.g. term.t_rev() sets the RESIDENT's reverse
     * flag, but an overlay-local cputc() reads its OWN, never-set one).
     * Route them here so overlay and resident share one conio instance. */
    ABI_CPUTS,
    ABI_CPUTC,

    /* src/file.c + src/fileio.c collaborators, for the file.c suite */
    ABI_FFROPEN,
    ABI_FFGETLINE,
    ABI_GETFILE,
    ABI_READIN,
    ABI_MAKENAME,
    ABI_WRITEOUT,
    ABI_DIRED_FILL,
    ABI_FILESAVE,
    ABI_DIRED,

    /* src/file.c mlreply()-prompted wrappers, for the file3 suite */
    ABI_FILEREAD,
    ABI_FILEFIND,
    ABI_FILEWRITE,
    ABI_FILENAME,

    /* src/line.c collaborators, for the line1/line2 suites. This filled
     * the LAST 4 remaining slots out of the ORIGINAL 64-slot capacity
     * (linsert/lnewline/ldelete were already exported above, for the
     * basic.c/display.c/file suites). */
    ABI_INSSPACE,
    ABI_KDELETE,
    ABI_KINSERT,
    ABI_KREMOVE,

    /* src/random.c, for the random1/random2 suites. The table was full
     * (64/64) before these, so ABI_TABLE_SIZE was grown by 11 slots (22
     * bytes) and OVERLAY_SIZE shrunk by the same amount to compensate
     * -- any FUTURE suite that needs a new resident symbol must repeat
     * that dance (grow ABI_TABLE_SIZE, shrink OVERLAY_SIZE, keeping
     * their sum fixed to the __RESERVED_MEMORY__ region -- see both
     * .cfg files). */
    ABI_SHOWCPOS,
    ABI_GETCCOL,
    ABI_QUOTE,
    ABI_TAB,
    ABI_OPENLINE,
    ABI_NEWLINE,
    ABI_INDENT,
    ABI_FORWDEL,
    ABI_BACKDEL,
    ABI_KILLTEXT,
    ABI_YANK,

    /* src/search.c, for the search suite. Table was full (75/75) before
     * these, so ABI_TABLE_SIZE was grown by 6 slots (12 bytes) --
     * ABI_PAT (global data: &pat[0], so tests can assert its exact
     * stored contents after readpattern()) plus the 5 functions -- and
     * OVERLAY_SIZE shrunk by the same amount to compensate. */
    ABI_PAT,
    ABI_FORWSEARCH,
    ABI_EQ,
    ABI_READPATTERN,
    ABI_FORSCAN,
    ABI_EXPANDP,

    /* src/window.c, for the window suite */
    ABI_REPOSITION,
    ABI_REFRESH,

    /* src/termio.c, for the termio suite */
    ABI_TTOPEN,
    ABI_TTCLOSE,
    ABI_TTFLUSH,
    ABI_TTPUTC,
    ABI_TTGETC,
    ABI_TTGETC_NOSHOW,
    ABI_TYPAHEAD,

    /* global data needed by the window/termio suites (src/edef.h) */
    ABI_SGARBF,

    ABI_NUM_SLOTS
};

#endif /* ABI_TABLE_H */
