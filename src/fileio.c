/*
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here. A better message writing scheme should
 * be used.
 */
#include        <stdio.h>
#include	"estruct.h"
#include        "efunc.h"
#include        "edef.h"

FILE    *ffp;                           /* File pointer, all functions. */

#include <string.h>
#include <ctype.h>
/*
 * Atari CIO requires a device prefix such as "D:" on every filename.
 * If fn contains no ':', copy it into buf as "D:<fn>" (uppercased).
 * If fn already has a ':', uppercase it in-place into buf and return buf.
 * buf must be at least ATARI_PATH_MAX bytes.
 */
#define ATARI_PATH_MAX  32
static const char *atari_qualify(const char *fn, char *buf)
{
        const char *src;
        char *dst = buf;

        if (strchr(fn, ':') == NULL) {
                *dst++ = 'D';
                *dst++ = ':';
        }
        for (src = fn; *src && (dst - buf) < ATARI_PATH_MAX - 1; src++)
                *dst++ = toupper((unsigned char)*src);
        *dst = '\0';
        return buf;
}

/*
 * Open a file for reading.
 */
unsigned char ffropen(char * fn)
{
        char qbuf[ATARI_PATH_MAX];
        fn = (char *)atari_qualify(fn, qbuf);
        if ((ffp=fopen(fn, "r")) == NULL)
                return (FIOFNF);
        return (FIOSUC);
}

/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
unsigned char ffwopen(char * fn)
{
        char qbuf[ATARI_PATH_MAX];
        fn = (char *)atari_qualify(fn, qbuf);
        if ((ffp=fopen(fn, "w")) == NULL) {
                mlwrite("Cannot open file for writing");
                return (FIOERR);
        }
        return (FIOSUC);
}

/*
 * Close a file. Should look at the status in all systems.
 */
unsigned char ffclose(void)
{
	if (ffp)
		fclose(ffp);
	ffp = NULL;
        return (FIOSUC);
}

/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 * Check only at the newline.
 */
unsigned char ffputline(char buf[], int nbuf)
{
        int i;  /* must match nbuf's width: an unsigned char here wraps before reaching nbuf>255 and never terminates */

        for (i = 0; i < nbuf; ++i)
                fputc(buf[i]&0xFF, ffp);

        fputc('\n', ffp);

        if (ferror(ffp)) {
                mlwrite("Write I/O error");
                return (FIOERR);
        }

        return (FIOSUC);
}

/*
 * Read a line from a file, and store the bytes in the supplied buffer. The
 * "nbuf" is the length of the buffer. Complain about long lines and lines
 * at the end of the file that don't have a newline present. Check for I/O
 * errors too. Return status.
 */
unsigned char ffgetline(register char buf[], int nbuf)
{
        register int    c;
        int i;  /* must match nbuf's width: same class of bug as ffputline's fixed one, for nbuf>255 */

        i = 0;

        /* On Atari, CIO returns raw $9B (0x9B) record terminators; cc65's
         * D: file I/O does not translate $9B -> '\n' for disk files the way
         * the E: device does.  Stop on either so the terminator is never
         * stored in the line buffer. */
        while ((c = fgetc(ffp)) != EOF && c != '\n' && (unsigned char)c != 0x9B) {
                if (i >= nbuf-2) {
			buf[nbuf - 2] = c;	/* store last char read */
			buf[nbuf - 1] = 0;	/* and terminate it */
                        mlwrite("File has long line");
                        return (FIOLNG);
                }
                buf[i++] = c;
        }

        if (c == EOF) {
                if (ferror(ffp)) {
                        mlwrite("File read error");
                        return (FIOERR);
                }

                if (i != 0) {
                        mlwrite("File has funny line at EOF");
                        return (FIOERR);
                }
                return (FIOEOF);
        }

        buf[i] = 0;
        return (FIOSUC);
}
