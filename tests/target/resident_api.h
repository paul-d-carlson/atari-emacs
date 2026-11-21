/*
 * resident_api.h
 *
 * Included by every test_*.c OVERLAY suite (test_buffer.c, test_atari.c,
 * test_basic.c, test_bind.c, test_display.c) INSTEAD OF efunc.h/edef.h.
 *
 * Each suite is linked as its own tiny, separate image (see
 * test-overlay.cfg) -- it never links the real application object
 * files, so
 * it cannot reference bfind()/mlwrite()/curbp/term/etc by their real
 * names (no such symbols exist in an overlay's own link). Every one of
 * those names is instead redefined here as a macro that reaches into
 * the resident image's fixed-address ABI table (abi_table.h,
 * populated by tests/target/abi_export.c) -- see abi_table.h's own
 * comment for the full rationale and the exact fixed addresses.
 *
 * estruct.h is still included directly (struct layouts/#defines only,
 * no symbols to resolve), so overlay code can declare BUFFER, WINDOW,
 * LINE and TERM locals and use lforw()/lback()/lgetc()/lputc()/llength()
 * (plain macros in estruct.h, not real symbols -- no ABI slot needed).
 */
#ifndef RESIDENT_API_H
#define RESIDENT_API_H

#include <string.h>

#include "estruct.h"
#include "abi_table.h"

/* Slot n's raw stored value (a function's own address, or the address
 * of a resident global variable), as `void *`. */
#define ABI_SLOT(n) (*(void **)(ABI_TABLE_ADDR + (unsigned)(n) * 2))

/* -- global data: table stores &real_variable, so dereference once more
 * to reach the real variable itself as a normal, assignable lvalue. -- */
#define curbp    (*(BUFFER **)ABI_SLOT(ABI_CURBP))
#define curwp    (*(WINDOW **)ABI_SLOT(ABI_CURWP))
#define bheadp   (*(BUFFER **)ABI_SLOT(ABI_BHEADP))
#define thisflag (*(unsigned char *)ABI_SLOT(ABI_THISFLAG))
#define lastflag (*(unsigned char *)ABI_SLOT(ABI_LASTFLAG))
#define curgoal  (*(unsigned char *)ABI_SLOT(ABI_CURGOAL))
#define term     (*(TERM *)ABI_SLOT(ABI_TERM))
#define mpresf   (*(unsigned char *)ABI_SLOT(ABI_MPRESF))
#define pat      ((char *)ABI_SLOT(ABI_PAT))

/* -- functions: table stores the function's own entry address; cast to
 * the right function-pointer type and call through it directly. -- */
#define bfind      ((BUFFER *(*)(char *, int, int))ABI_SLOT(ABI_BFIND))
#define anycb      ((int (*)(void))ABI_SLOT(ABI_ANYCB))
#define swbuffer   ((unsigned char (*)(BUFFER *))ABI_SLOT(ABI_SWBUFFER))
#define nextbuffer ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_NEXTBUFFER))
#define bclear     ((unsigned char (*)(BUFFER *))ABI_SLOT(ABI_BCLEAR))
#define zotbuf     ((unsigned char (*)(BUFFER *))ABI_SLOT(ABI_ZOTBUF))
#define killbuffer ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_KILLBUFFER))
#define usebuffer  ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_USEBUFFER))
#define gotobol    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_GOTOBOL))
#define gotoeol    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_GOTOEOL))
#define forwchar   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FORWCHAR))
#define backchar   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_BACKCHAR))
#define gotobob    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_GOTOBOB))
#define gotoeob    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_GOTOEOB))
#define gotoline   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_GOTOLINE))
#define getgoal    ((int (*)(LINE *))ABI_SLOT(ABI_GETGOAL))
#define forwline   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FORWLINE))
#define backline   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_BACKLINE))
#define forwpage   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FORWPAGE))
#define backpage   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_BACKPAGE))
#define setmark    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_SETMARK))
#define swapmark   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_SWAPMARK))
#define linsert    ((unsigned char (*)(int, int))ABI_SLOT(ABI_LINSERT))
#define lnewline   ((unsigned char (*)(void))ABI_SLOT(ABI_LNEWLINE))
#define ldelete    ((unsigned char (*)(int, unsigned char))ABI_SLOT(ABI_LDELETE))
#define help       ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_HELP))
#define ffropen    ((unsigned char (*)(char *))ABI_SLOT(ABI_FFROPEN))
#define ffgetline  ((unsigned char (*)(char *, int))ABI_SLOT(ABI_FFGETLINE))
#define ffwopen    ((unsigned char (*)(char *))ABI_SLOT(ABI_FFWOPEN))
#define ffputline  ((unsigned char (*)(char *, int))ABI_SLOT(ABI_FFPUTLINE))
#define ffclose    ((unsigned char (*)(void))ABI_SLOT(ABI_FFCLOSE))
#define getfile    ((unsigned char (*)(char *, int))ABI_SLOT(ABI_GETFILE))
#define readin     ((unsigned char (*)(char *, int))ABI_SLOT(ABI_READIN))
#define makename   ((void (*)(char *, char *))ABI_SLOT(ABI_MAKENAME))
#define writeout   ((unsigned char (*)(char *))ABI_SLOT(ABI_WRITEOUT))
#define dired_fill ((unsigned char (*)(BUFFER *))ABI_SLOT(ABI_DIRED_FILL))
#define filesave   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FILESAVE))
#define dired      ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_DIRED))
#define fileread   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FILEREAD))
#define filefind   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FILEFIND))
#define filewrite  ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FILEWRITE))
#define filename   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FILENAME))
#define insspace   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_INSSPACE))
#define kdelete    ((void (*)(void))ABI_SLOT(ABI_KDELETE))
#define kinsert    ((unsigned char (*)(int))ABI_SLOT(ABI_KINSERT))
#define kremove    ((int (*)(int))ABI_SLOT(ABI_KREMOVE))

/* src/random.c, for the random1/random2 suites. All 11 share the same
 * (unsigned char f, int n) command-function signature except getccol,
 * which takes a plain int bflg. */
#define showcpos   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_SHOWCPOS))
#define getccol    ((unsigned char (*)(int))ABI_SLOT(ABI_GETCCOL))
#define quote      ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_QUOTE))
#define tab        ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_TAB))
#define openline   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_OPENLINE))
#define newline    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_NEWLINE))
#define indent     ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_INDENT))
#define forwdel    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FORWDEL))
#define backdel    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_BACKDEL))
#define killtext   ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_KILLTEXT))
#define yank       ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_YANK))
#define mlwrite    ((void (*)(char *, ...))ABI_SLOT(ABI_MLWRITE))
#define mlputs     ((void (*)(char *))ABI_SLOT(ABI_MLPUTS))
#define mlerase    ((void (*)(void))ABI_SLOT(ABI_MLERASE))
#define movecursor ((void (*)(unsigned char, unsigned char))ABI_SLOT(ABI_MOVECURSOR))
#define update     ((void (*)(void))ABI_SLOT(ABI_UPDATE))
#define mlyesno    ((unsigned char (*)(char *))ABI_SLOT(ABI_MLYESNO))
#define mlreplyt   ((unsigned char (*)(char *, char *, int, char))ABI_SLOT(ABI_MLREPLYT))
#define vttidy     ((void (*)(void))ABI_SLOT(ABI_VTTIDY))

/* cc65 conio: overlays must reach the resident's single, boot-initialised
 * conio copy, not their own separately-linked one (whose cursor/reverse
 * state is independent of the resident rendering they compare against).
 * Must be included AFTER <conio.h> so conio.h's own prototype parses
 * first, then this macro redirects the overlay's call sites. */
#define cputs      ((int (*)(const char *))ABI_SLOT(ABI_CPUTS))
#define cputc      ((void (*)(char))ABI_SLOT(ABI_CPUTC))

/* src/search.c, for the search suite. eq()/forscan()/expandp() take
 * plain int/char* args (not the usual (unsigned char f, int n) command
 * signature); forwsearch() does. */
#define forwsearch ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_FORWSEARCH))
#define eq         ((int (*)(int, int))ABI_SLOT(ABI_EQ))
#define readpattern ((unsigned char (*)(char *))ABI_SLOT(ABI_READPATTERN))
#define forscan    ((unsigned char (*)(char *, int))ABI_SLOT(ABI_FORSCAN))
#define expandp    ((unsigned char (*)(char *, char *, signed char))ABI_SLOT(ABI_EXPANDP))

/* src/window.c, for the window suite. Both take the usual
 * (unsigned char f, int n) command-function signature. */
#define reposition ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_REPOSITION))
#define refresh    ((unsigned char (*)(unsigned char, int))ABI_SLOT(ABI_REFRESH))

/* src/termio.c, for the termio suite. */
#define ttopen     ((void (*)(void))ABI_SLOT(ABI_TTOPEN))
#define ttclose    ((void (*)(void))ABI_SLOT(ABI_TTCLOSE))
#define ttflush    ((void (*)(void))ABI_SLOT(ABI_TTFLUSH))
#define ttputc     ((void (*)(int))ABI_SLOT(ABI_TTPUTC))
#define ttgetc     ((int (*)(void))ABI_SLOT(ABI_TTGETC))
#define ttgetc_noshow ((int (*)(void))ABI_SLOT(ABI_TTGETC_NOSHOW))
#define typahead   ((int (*)(void))ABI_SLOT(ABI_TYPAHEAD))

/* src/edef.h global, needed by the window/termio suites. */
#define sgarbf     (*(unsigned char *)ABI_SLOT(ABI_SGARBF))

#endif /* RESIDENT_API_H */
