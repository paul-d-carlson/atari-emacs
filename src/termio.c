/*
 * termio.c
 *
 * Atari 8-bit keyboard and display I/O for MicroEMACS.
 * Uses cc65 conio.h.  No POSIX/Unix dependencies.
 *
 * Called by atari.c:
 *   ttopen()   - initialise (no-op; screen size is fixed in atari.c)
 *   ttclose()  - shut down (no-op)
 *   ttputc(c)  - output one character at the current cursor position
 *   ttflush()  - flush output (no-op; CIO writes are immediate)
 *   ttgetc()   - wait for and return one ATASCII keycode
 *   typahead() - return TRUE if a keystroke is waiting
 */

#include <conio.h>
#include "estruct.h"
#include "edef.h"

extern int ttrow, ttcol;   /* from display.c */

/*
 * Open terminal I/O.  Screen size is fixed at compile time in atari.c,
 * so nothing to do here.
 */
void ttopen(void)
{
}

/*
 * Close terminal I/O.  Nothing to restore on Atari.
 */
void ttclose(void)
{
}

/*
 * Write one character to the screen at the current cursor position.
 * cc65 cputc() goes through CIO channel 6 (E: screen editor device).
 */
void ttputc(int c)
{
    cputc((unsigned char)c);
}

/*
 * Flush output buffer.  CIO writes are synchronous, so this is a no-op.
 */
void ttflush(void)
{
}

/*
 * Read one character from the Atari keyboard.
 * cgetc() blocks until a key is pressed and returns its ATASCII code.
 * The Atari DELETE key returns 0x7E (bound to backdel in ebind.h).
 */
int ttgetc(void)
{
    int c = 0;
    unsigned char old_cursor = 0;  

    /* Sync conio cursor to the tracked position if valid */
    if (ttrow != HUGE && ttcol != HUGE)
        gotoxy((unsigned char)ttcol, (unsigned char)ttrow);

    old_cursor = cursor(1);
    c = (int)(unsigned char)cgetc();
    cursor(old_cursor);   /* restore previous cursor state */
    return c;
}
/* Read one character from the Atari keyboard WITHOUT toggling the cursor. */
int ttgetc_noshow(void)
{
    return (int)(unsigned char)cgetc();
}

/*
 * Return TRUE if a keystroke is available without blocking.
 * cc65 kbhit() checks the OS keyboard queue.
 */
int typahead(void)
{
    return kbhit() ? TRUE : FALSE;
}
