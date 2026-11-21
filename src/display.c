/*
 * The functions in this file handle redisplay. There are two halves, the
 * ones that update the virtual display screen, and the ones that make the
 * physical display screen the same as the virtual display screen. These
 * functions use hints that are left in the windows by the commands.
 *
 */

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include "estruct.h"
#include "efunc.h"
#include "edef.h"

#include <stdarg.h>
#define WFDEBUG 0                       /* Window flag debug. */

/* Atari-specific screen dimensions (must match atari.c) */
#define NROW    24
#define NCOL    40

/* Forward declarations */
void mlwrite(char *fmt, ...);
unsigned char mlreplyt(char *prompt, char *buf, int nbuf, char eolchar);

#define VFCHG   0x0001                  /* Changed flag                 */
#define VFEXT   0x0002                  /* extended (beyond screen width) */
#define VFREV   0x0004                  /* reverse video status         */
#define VFREQ   0x0008                  /* reverse video request        */

unsigned char vtrow   = 0;              /* Row location of SW cursor */
int           vtcol   = 0;              /* Column location of SW cursor (int: can go negative in updext) */
unsigned char ttrow   = HUGE;           /* Row location of HW cursor */
unsigned char ttcol   = HUGE;           /* Column location of HW cursor */
int           lbound  = 0;              /* leftmost column (int: negated as -lbound in updext, can exceed 127) */

/* Static screen buffer — no malloc needed on fixed 24x40 Atari screen */
static char  vbuf[NROW][NCOL];          /* virtual screen content */
static char  pbuf[NROW][NCOL];          /* physical screen shadow  */
static unsigned char vflag[NROW];       /* per-row flags: VFCHG|VFEXT|VFREV|VFREQ */

/*
 * Initialize the data structures used by the display code. The edge vectors
 * used to access the screens are set up. The operating system's terminal I/O
 * channel is set up. All the other things get initialized at compile time.
 * The original window has "WFCHG" set, so that it will get completely
 */

void vtinit(void){
        unsigned char i;
        unsigned char j;

        (*term.t_open)();
        (*term.t_rev)(FALSE);
        for (i = 0; i < NROW; i++) {
                vflag[i] = 0;
                for (j = 0; j < NCOL; j++)
                        vbuf[i][j] = ' ';
        }
}

void vttidy(void)
{
        mlerase();
        movecursor(term.t_nrow, 0);
        (*term.t_close)();
}

void vtmove(int row, int col)
{
        vtrow = row;
        vtcol = col;
}

void vtputc(int c)
{
        if (vtcol >= term.t_ncol) {
                vtcol = (vtcol + 0x07) & ~0x07;
                vbuf[vtrow][term.t_ncol - 1] = '$';
        } else if (c == '\t') {
                do { vtputc(' '); } while ((vtcol & 0x07) != 0);
        } else if (c < 0x20 || c == 0x7F) {
                vtputc('^');
                vtputc(c ^ 0x40);
        } else {
                vbuf[vtrow][vtcol++] = c;
        }
}

void vtpute(int c)
{
        if (vtcol >= (int)term.t_ncol) {
                vtcol = (vtcol + 0x07) & ~0x07;
                vbuf[vtrow][term.t_ncol - 1] = '$';
        } else if (c == '\t') {
                do { vtpute(' '); } while (((vtcol + lbound) & 0x07) != 0);
        } else if (c < 0x20 || c == 0x7F) {
                vtpute('^');
                vtpute(c ^ 0x40);
        } else {
                if (vtcol >= 0)
                        vbuf[vtrow][vtcol] = c;
                ++vtcol;
        }
}

void vteeol(void)
{
        if (vtcol < 0) vtcol = 0;       /* updext may leave vtcol negative */
        while (vtcol < term.t_ncol)
                vbuf[vtrow][vtcol++] = ' ';
}

/*
 * Make sure that the display is right. This is a three part process. First,
 * scan through all of the windows looking for dirty ones. Check the framing,
 * and update the virtual screen. Second, update the virtual screen for any
 * extended line. Third, make the physical and virtual screens the same.
 */
void update(void)
{
    register LINE          *lp;
    register WINDOW        *wp;
    register int            i;
    unsigned char           j;
    register unsigned char  c;

    if (typahead())
        return;
    /* update the reverse video flags for any mode lines out there */
    for (i = 0; i < term.t_nrow; ++i)
        vflag[i] &= ~VFREQ;

    vflag[term.t_nrow-1] |= VFREQ;
    wp = curwp;
    /* Process the window if update flags are set. */
    if (wp->w_flag != 0) {
            /* If not force reframe, check the framing. */
            if ((wp->w_flag & WFFORCE) == 0) {
                lp = wp->w_linep;
                for (i = 0; i < term.t_nrow-1; ++i) {
                    if (lp == wp->w_dotp)
                        goto out;
                    if (lp == wp->w_bufp->b_linep)
                        break;
                    lp = lforw(lp);
                }
            }

            /* Compute new top-of-window line and force full redraw. */
            i = wp->w_force;
            if (i > 0) {
                --i;
                if (i >= term.t_nrow-1)
                    i = term.t_nrow-2;
            } else if (i < 0) {
                i += term.t_nrow-1;
                if (i < 0)
                    i = 0;
            } else
                i = (term.t_nrow-1)/2;

            lp = wp->w_dotp;
            while (i != 0 && lback(lp) != wp->w_bufp->b_linep) {
                --i;
                lp = lback(lp);
            }
            wp->w_linep = lp;
            wp->w_flag |= WFHARD;

out:
            lp = wp->w_linep;
            i  = 0;

            if ((wp->w_flag & ~WFMODE) == WFEDIT) {
                while (lp != wp->w_dotp) {
                    ++i;
                    lp = lforw(lp);
                }
                vflag[i] |= VFCHG;
                vtmove(i, 0);
                for (j = 0; j < llength(lp); ++j)
                    vtputc(lgetc(lp, j));
                vteeol();
            } else if ((wp->w_flag & (WFEDIT | WFHARD)) != 0) {
                while (i < term.t_nrow-1) {
                    vflag[i] |= VFCHG;
                    vtmove(i, 0);
                    if (lp != wp->w_bufp->b_linep) {
                        for (j = 0; j < llength(lp); ++j)
                            vtputc(lgetc(lp, j));
                        lp = lforw(lp);
                    }
                    vteeol();
                    ++i;
                }
            }

            if ((wp->w_flag & WFMODE) != 0)
                modeline();
            wp->w_flag  = 0;
            wp->w_force = 0;
        }

    /* Recompute hardware cursor row/col. */
    lp = curwp->w_linep;
    currow = 0;
    while (lp != curwp->w_dotp) {
        ++currow;
        lp = lforw(lp);
    }

    curcol = 0;
    i = 0;
    while (i < curwp->w_doto) {
        c = lgetc(lp, i++);
        if (c == '\t')
            curcol |= 0x07;
        else if (c < 0x20 || c == 0x7F)
            ++curcol;
        ++curcol;
    }

    if (curcol >= term.t_ncol - 1) {   /* extended line */
        vflag[currow] |= VFEXT | VFCHG;
        updext();
    } else
        lbound = 0;

    /* De-extend any lines whose cursor has moved off them. */
    lp = curwp->w_linep;
    i  = 0;
    while (i < term.t_nrow-1) {
        if (vflag[i] & VFEXT) {
            vflag[i] |= VFCHG;
            if ((lp != curwp->w_dotp) || (curcol < term.t_ncol - 1)) {
                vtmove(i, 0);
                for (j = 0; j < llength(lp); ++j)
                    vtputc(lgetc(lp, j));
                vteeol();
                vflag[i] &= ~VFEXT;
            }
        }
        lp = lforw(lp);
        ++i;
    }

    /* If screen is garbage, clear it and force full redraw. */
    if (sgarbf != FALSE) {
        for (i = 0; i < term.t_nrow; ++i)
            vflag[i] |= VFCHG;
        movecursor(0, 0);
        (*term.t_eeop)();
        memset(pbuf, 0, sizeof pbuf);   /* invalidate shadow: force full repaint */
        sgarbf = FALSE;
        mpresf = FALSE;
    }

    /* Update changed lines. */
    for (i = 0; i < term.t_nrow; ++i) {
        j = vflag[i];
        if (((j & VFCHG) != 0) ||
            (((j & VFREV) == 0) != ((j & VFREQ) == 0))) {
            if (typahead())
                return;
            updateline(i, vbuf[i], &vflag[i]);
        }
    }

    movecursor(currow, curcol - lbound);
    (*term.t_flush)();
}

/*
 * Update the extended line which the cursor is on when the cursor column
 * is past the terminal width. Scrolls the line right or left.
 */
void updext(void)
{
    register unsigned char  rcursor;
    register LINE          *lp;
    unsigned char           j;

    /*
     * curcol and the term fields are all unsigned char, so cc65 evaluates the
     * scroll-offset expression in 8-bit UNSIGNED arithmetic.  At the extended
     * threshold curcol == t_ncol-1, the subtraction curcol - t_ncol underflows
     * (wraps to 255) and the unsigned modulo returns a bogus offset, jumping
     * the view at the right edge.  That boundary is the only case where the
     * dividend would be negative, so handle it explicitly and keep the rest of
     * the math non-negative.  (Signed -1 % t_scrsiz == -1, so rcursor here is
     * t_margin - 1, matching the original int-arithmetic behavior.)
     */
    if (curcol < term.t_ncol)
        rcursor = term.t_margin - 1;
    else
        rcursor = ((curcol - term.t_ncol) % term.t_scrsiz) + term.t_margin;
    lbound  = curcol - rcursor + 1;

    vtmove(currow, -lbound);
    lp = curwp->w_dotp;
    for (j = 0; j < llength(lp); ++j)
        vtpute(lgetc(lp, j));
    vteeol();
    vbuf[currow][0] = '$';
}

/*
 * Redisplay the mode line for window wp.
 * Format: "ME: <bufname>[*] <filename>"
 */
void modeline(void)
{
    WINDOW *wp = curwp;
    register BUFFER *bp;
    register unsigned char c;
    register char   *cp;
    unsigned char n;
    unsigned char row;

    row = term.t_nrow-1;
    vflag[row] |= VFCHG;
    vtmove(row, 0);
    bp = wp->w_bufp;
    n = 0;
    cp = "ME: ";
    while ((c = *cp++) != 0) { vtputc(c); ++n; }
    cp = bp->b_bname;
    while ((c = *cp++) != 0) { vtputc(c); ++n; }
    if (bp->b_flag & BFCHG) { vtputc('*'); ++n; }
    vtputc(' '); ++n;
    cp = bp->b_fname;
    if (cp[0] != 0) {
        while ((c = *cp++) != 0) { vtputc(c); ++n; }
    }
    while (n < term.t_ncol) { vtputc(' '); ++n; }
}

/*
 * Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col". The row and column arguments are origin 0. Optimize out
 * random calls. Update "ttrow" and "ttcol".
 */
void movecursor(unsigned char row, unsigned char col)
    {
    if (row!=ttrow || col!=ttcol)
        {
        ttrow = row;
        ttcol = col;
        (*term.t_move)(row, col);
        }
    }

/*
 * Erase the message line. This is a special routine because the message line
 * is not considered to be part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed via a call to the flusher.
 */
void mlerase(void)
    {
    movecursor(term.t_nrow, 0);
    (*term.t_eeol)();
    (*term.t_flush)();
    mpresf = FALSE;
    }

/*
 * Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. Used any time a confirmation is required.
 */
unsigned char mlyesno(char * prompt)
{
	unsigned char c;	/* input character */
	char buf[NPAT];		/* prompt to user */

	for (;;) {
		/* build and prompt the user */
		strcpy(buf, prompt);
		strcat(buf, " [y/n]? ");
		mlwrite(buf);

		/* get the responce */
		c = (*term.t_getchar)();

		if (c == BELL)		/* Bail out! */
			return(ABORT);

		if (c=='y' || c=='Y')
			return(TRUE);

		if (c=='n' || c=='N')
			return(FALSE);
	}
}

/*
 * Write a prompt into the message line, then read back a response. Keep
 * track of the physical position of the cursor. If we are in a keyboard
 * macro throw the prompt away, and return the remembered response. This
 * lets macros run at full speed. The reply is always terminated by a carriage
 * return. Handle erase, kill, and abort keys.
 */

unsigned char mlreply(char * prompt, char * buf, int nbuf)
{
	return(mlreplyt(prompt,buf,nbuf,'\n'));
}

/*	A more generalized prompt/reply function allowing the caller
	to specify the proper terminator. If the terminator is not
	a return ('\n') it will echo as "<NL>"
*/

unsigned char mlreplyt(char * prompt, char * buf, int nbuf, char eolchar)
{
    register unsigned char cpos;
    register unsigned char c;
    unsigned char uc;
    unsigned char basec;
    unsigned char erase_cols;
    unsigned char i;
    unsigned char ch;
    int newcol;

    cpos = 0;

    mlwrite(prompt);

    for (;;) {
        c = (*term.t_getchar)();
        uc = (unsigned char)c;

        /* Map CR or Atari RETURN to NL, otherwise use 7-bit base char */
        if (uc == 0x0d || uc == 0x9B)
            basec = '\n';
        else
            basec = uc & 0x7F;

        if (basec == eolchar) {
            buf[cpos++] = 0;
            (*term.t_putchar)('\r');
            ttcol = 0;
            (*term.t_flush)();
            if (buf[0] == 0)
                return(FALSE);
            return(TRUE);

        } else if (basec == 0x07) {    /* Bell, abort */
            (*term.t_putchar)('^');
            (*term.t_putchar)('G');
            ttcol += 2;
            ctrlg(FALSE, 0);
            (*term.t_flush)();
            return(ABORT);

        } else if (basec == 0x7F || basec == 0x08 || basec == 0x7E) {    /* rubout/erase */
            if (cpos != 0) {
                ch = (unsigned char) buf[--cpos];
                if (ch == '\n')
                    erase_cols = 4;
                else if (ch < 0x20)
                    erase_cols = 2;
                else
                    erase_cols = 1;

                newcol = ttcol - erase_cols;
                if (newcol < 0) newcol = 0;

                movecursor(ttrow, newcol);
                for (i = 0; i < erase_cols; ++i) {
                    (*term.t_putchar)(' ');
                    ++ttcol;
                }
                movecursor(ttrow, newcol);
                (*term.t_flush)();
            }

        } else if (basec == 0x15) {    /* C-U, kill */
            while (cpos != 0) {
                ch = (unsigned char) buf[--cpos];
                if (ch == '\n')
                    erase_cols = 4;
                else if (ch < 0x20)
                    erase_cols = 2;
                else
                    erase_cols = 1;

                newcol = ttcol - erase_cols;
                if (newcol < 0) newcol = 0;

                movecursor(ttrow, newcol);
                for (i = 0; i < erase_cols; ++i) {
                    (*term.t_putchar)(' ');
                    ++ttcol;
                }
                movecursor(ttrow, newcol);
            }
            (*term.t_flush)();

        } else {
            if (cpos < nbuf-1) {
                buf[cpos++] = (char) basec;

                if ((basec < ' ') && (basec != '\n')) {
                    (*term.t_putchar)('^');
                    ++ttcol;
                    basec ^= 0x40;
                }

                if (basec != '\n')
                    (*term.t_putchar)(basec);
                else {    /* put out <NL> for <ret> */
                    (*term.t_putchar)('<');
                    (*term.t_putchar)('N');
                    (*term.t_putchar)('L');
                    (*term.t_putchar)('>');
                    ttcol += 3;
                }
                ++ttcol;
                (*term.t_flush)();
            }
        }
    }
}

/*
 * Write a message into the message line. Keep track of the physical cursor
 * position. A small class of printf like format items is handled. Assumes the
 * stack grows down; this assumption is made by the "++" in the argument scan
 * loop. Set the "message line" flag TRUE.
 */

void mlwrite(char *fmt, ...)
    {
    register unsigned char c;
    va_list ap;

    movecursor(term.t_nrow, 0);
    va_start(ap, fmt);
    while ((c = *fmt++) != 0) {
        if (c != '%') {
            (*term.t_putchar)(c);
            ++ttcol;
            }
        else
            {
            c = *fmt++;
            switch (c) {
                case 'd':
                    mlputi(va_arg(ap, int), 10);
                    break;

                default:
                    (*term.t_putchar)(c);
                    ++ttcol;
                }
            }
        }
    va_end(ap);
    (*term.t_eeol)();
    (*term.t_flush)();
    mpresf = TRUE;
    }

/*
 * Write out a string. Update the physical cursor position. This assumes that
 * the characters in the string all have width "1"; if this is not the case
 * things will get screwed up a little.
 */
void mlputs(char * s)
    {
    register unsigned char c;

    while ((c = *s++) != 0)
        {
        (*term.t_putchar)(c);
        ++ttcol;
        }
    }

/*
 * Write out an integer, in the specified radix. Update the physical cursor
 * position. This will not handle any negative numbers; maybe it should.
 */
void mlputi(int i, int r)
    {
    register int q;
    static char hexdigits[] = "0123456789ABCDEF";

    if (i < 0)
        {
        i = -i;
        (*term.t_putchar)('-');
        }

    q = i/r;

    if (q != 0)
        mlputi(q, r);

    (*term.t_putchar)(hexdigits[i%r]);
    ++ttcol;
    }

/*
 * Update a single line. Skips matching characters at left and right,
 * uses erase-to-EOL when possible. Handles reverse video (REVSTA).
 */
void updateline(unsigned char row, char vline[], unsigned char *flags)
{
    char               *pline  = pbuf[row];
    unsigned char j;
    unsigned char req    = (*flags & VFREQ) != 0;
    unsigned char was    = (*flags & VFREV) != 0;
    register int  lo, hi;
    register int  last_ns;    /* sentinel: lo-1 can be -1 when lo==0 */
    unsigned char cs, cl;

    /* Force full-row repaint when reverse-video attribute changes */
    if (req != was) {
        lo = 0;
        hi = term.t_ncol - 1;
    } else {
        /* Narrow to minimal dirty span */
        lo = 0;
        while (lo < term.t_ncol && vline[lo] == pline[lo]) ++lo;
        if (lo >= term.t_ncol) goto done;   /* row unchanged */
        hi = term.t_ncol - 1;
        while (vline[hi] == pline[hi]) --hi;
    }

    /* Find rightmost non-space within dirty span */
    last_ns = lo - 1;
    for (j = lo; j <= hi; ++j)
        if (vline[j] != ' ') last_ns = j;

    /* Write non-space portion */
    if (last_ns >= lo) {
        movecursor(row, lo);
        (*term.t_rev)(req);
        for (j = lo; j <= last_ns; ++j)
            (*term.t_putchar)(vline[j]);
        ttcol = last_ns + 1;
        (*term.t_rev)(FALSE);
    }

    /* Clear trailing-space portion */
    if (last_ns < hi) {
        cs = last_ns + 1;
        cl = hi - last_ns;
        if (!req) {
            /* Normal video: cclearxy writes direct to screen RAM — faster than CIO */
            cclearxy((unsigned char)cs, (unsigned char)row, (unsigned char)cl);
            ttrow = HUGE;               /* invalidate: cclearxy moves HW cursor */
            ttcol = HUGE;
        } else {
            /* Reverse-video row: use putchar so revers() attribute is applied */
            movecursor(row, cs);
            (*term.t_rev)(TRUE);
            for (j = cs; j <= hi; ++j)
                (*term.t_putchar)(' ');
            ttcol = hi + 1;
            (*term.t_rev)(FALSE);
        }
    }

    /* Update physical screen shadow */
    memcpy(pline + lo, vline + lo, hi - lo + 1);

done:
    *flags &= ~VFCHG;
    if (req) *flags |= VFREV;
    else     *flags &= ~VFREV;
}

/* getname removed — was only used by bindtokey and namedcmd (both removed) */
