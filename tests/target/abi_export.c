/*
 * abi_export.c
 *
 * Resident-side half of the disk-overlay cross-image call table (see
 * abi_table.h for the full rationale). This file is linked ONLY into
 * the resident test image (test_main_resident.c) -- never into an
 * overlay suite -- and is the ONE place that takes the real addresses
 * of the real application symbols an overlay suite might need.
 *
 * abi_table[] is pinned to the fixed address ABI_TABLE_ADDR via the
 * "ABITABLE" segment (see test-resident.cfg), so every overlay suite
 * (linked completely separately, see test-overlay.cfg + resident_api.h)
 * can find it at a constant, build-independent address regardless of
 * how much the rest of the resident image's code/data grows.
 *
 * Global *variables* store the variable's OWN address (&curbp), so
 * resident_api.h's macros can dereference through it as an assignable
 * lvalue; functions store their own entry address directly.
 */
#include <conio.h>

#include "estruct.h"
#include "efunc.h"
#include "edef.h"
#include "abi_table.h"

/* src/termio.c has no header of its own; its functions aren't
 * prototyped anywhere except locally within src/atari.c/src/main.c
 * (the only other callers). Redeclared here, same convention, purely
 * so their addresses can be taken below. */
extern void ttputc(int c);
extern int  ttgetc(void);
extern int  ttgetc_noshow(void);
extern void ttopen(void);
extern void ttclose(void);
extern void ttflush(void);

#pragma rodata-name (push, "ABITABLE")
void * const abi_table[ABI_NUM_SLOTS] = {
    /* global data -- store &variable */
    (void *)&curbp,
    (void *)&curwp,
    (void *)&bheadp,
    (void *)&thisflag,
    (void *)&lastflag,
    (void *)&curgoal,
    (void *)&term,
    (void *)&mpresf,

    /* functions -- store the function's own address */
    (void *)bfind,
    (void *)anycb,
    (void *)swbuffer,
    (void *)nextbuffer,
    (void *)bclear,
    (void *)zotbuf,
    (void *)killbuffer,
    (void *)usebuffer,
    (void *)gotobol,
    (void *)gotoeol,
    (void *)forwchar,
    (void *)backchar,
    (void *)gotobob,
    (void *)gotoeob,
    (void *)gotoline,
    (void *)getgoal,
    (void *)forwline,
    (void *)backline,
    (void *)forwpage,
    (void *)backpage,
    (void *)setmark,
    (void *)swapmark,
    (void *)linsert,
    (void *)lnewline,
    (void *)ldelete,
    (void *)help,
    (void *)ffwopen,
    (void *)ffputline,
    (void *)ffclose,
    (void *)mlwrite,
    (void *)mlputs,
    (void *)mlerase,
    (void *)movecursor,
    (void *)update,
    (void *)mlyesno,
    (void *)mlreplyt,
    (void *)vttidy,

    /* cc65 conio -- resident's single boot-initialised copy */
    (void *)cputs,
    (void *)cputc,

    /* src/file.c + src/fileio.c collaborators, for the file.c suite */
    (void *)ffropen,
    (void *)ffgetline,
    (void *)getfile,
    (void *)readin,
    (void *)makename,
    (void *)writeout,
    (void *)dired_fill,
    (void *)filesave,
    (void *)dired,

    /* src/file.c mlreply()-prompted wrappers, for the file3 suite */
    (void *)fileread,
    (void *)filefind,
    (void *)filewrite,
    (void *)filename,

    /* src/line.c collaborators, for the line1/line2 suites */
    (void *)insspace,
    (void *)kdelete,
    (void *)kinsert,
    (void *)kremove,

    /* src/random.c, for the random1/random2 suites */
    (void *)showcpos,
    (void *)getccol,
    (void *)quote,
    (void *)tab,
    (void *)openline,
    (void *)newline,
    (void *)indent,
    (void *)forwdel,
    (void *)backdel,
    (void *)killtext,
    (void *)yank,

    /* src/search.c, for the search suite */
    (void *)&pat[0],
    (void *)forwsearch,
    (void *)eq,
    (void *)readpattern,
    (void *)forscan,
    (void *)expandp,

    /* src/window.c, for the window suite */
    (void *)reposition,
    (void *)refresh,

    /* src/termio.c, for the termio suite */
    (void *)ttopen,
    (void *)ttclose,
    (void *)ttflush,
    (void *)ttputc,
    (void *)ttgetc,
    (void *)ttgetc_noshow,
    (void *)typahead,

    /* src/edef.h global, needed by the window/termio suites */
    (void *)&sgarbf,
};
#pragma rodata-name (pop)
