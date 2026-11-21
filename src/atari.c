/*
 * atari.c
 *
 * Atari 8-bit terminal driver for MicroEMACS.
 * Uses cc65 conio.h for cursor control, character output, and reverse video.
 * Target: Atari 800/XL/XE, GR.0 text mode (40 columns x 24 rows).
 *
 * Key mapping notes:
 *   - Printable ASCII 0x20-0x7E: identical in ATASCII, no translation needed.
 *   - Ctrl+key (0x01-0x1A): same as ASCII Ctrl codes; getkey() in main.c
 *     promotes them to CTRL|letter automatically.
 *   - ESC (0x1B): used as META prefix by getkey(); same on Atari.
 *   - DELETE key: Atari sends ATASCII 0x7E; bound to backdel in ebind.h.
 */

#include <conio.h>
#include "estruct.h"
#include "edef.h"

#define NROW    24      /* screen rows (GR.0 mode)              */
#define NCOL    40      /* screen columns (GR.0 mode)           */
#define MARGIN  4       /* min margin before horizontal scroll  */
#define SCRSIZ  32      /* scroll region size                   */

/* Forward declarations */
static void atari_open(void);
static void atari_move(unsigned char row, unsigned char col);
static void atari_eeol(void);
static void atari_eeop(void);
static void atari_beep(void);
static void atari_rev(unsigned char state);

extern void ttopen(void);
extern void ttclose(void);
extern int  ttgetc(void);
extern void ttputc(int c);
extern void ttflush(void);

/*
 * The global terminal structure.  This is selected at link time
 * by compiling with atari.c instead of ansi.c.
 */
TERM term = {
    NROW - 1,       /* t_nrow: last row index (0-based)     */
    NCOL,           /* t_ncol: number of columns            */
    MARGIN,         /* t_margin                             */
    SCRSIZ,         /* t_scrsiz                             */
    atari_open,     /* t_open                               */
    ttclose,        /* t_close                              */
    ttgetc,         /* t_getchar                            */
    ttputc,         /* t_putchar                            */
    ttflush,        /* t_flush                              */
    atari_move,     /* t_move                               */
    atari_eeol,     /* t_eeol                               */
    atari_eeop,     /* t_eeop                               */
    atari_beep,     /* t_beep                               */
    atari_rev       /* t_rev                                */
};

/*
 * Open the terminal.  Enable reverse-video support and call the
 * low-level I/O initialiser.
 */
static void atari_open(void)
{
    ttopen();
}

/*
 * Move the cursor to the given row/col (both 0-based).
 * cc65 gotoxy() takes (x=col, y=row).
 */
static void atari_move(unsigned char row, unsigned char col)
{
    gotoxy(col, row);
}

/*
 * Erase from the current cursor position to the end of the current line.
 */
static void atari_eeol(void)
{
    unsigned char col = wherex();
    cclearxy(col, wherey(), NCOL - col);
}

/*
 * Erase from the current cursor position to the end of the screen.
 */
static void atari_eeop(void)
{
    unsigned char row = wherey();
    unsigned char col = wherex();
    unsigned char r;

    /* clear remainder of current line */
    cclearxy(col, row, NCOL - col);

    /* clear all lines below */
    for (r = row + 1; r < NROW; r++)
        cclearxy(0, r, NCOL);
}

/*
 * Ring the terminal bell.  ATASCII 0x07 sent through the E: device
 * produces the Atari system beep.
 */
static void atari_beep(void)
{
    ttputc(0x07);
    ttflush();
}

/*
 * Set or clear reverse-video mode.
 * cc65 conio revers(1) enables inverse characters; revers(0) disables.
 */
static void atari_rev(unsigned char state)
{
    revers(state);
}
