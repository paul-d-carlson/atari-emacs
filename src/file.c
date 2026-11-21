/*
 * The routines in this file
 * handle the reading and writing of
 * disk files. All of details about the
 * reading and writing of the disk are
 * in "fileio.c".
 */
#include        <stdio.h>
#include        <dirent.h>
#include	"estruct.h"
#include        "efunc.h"
#include        "edef.h"

/*
 * Read a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "read a file into the current buffer" code.
 * Bound to "C-X C-R".
 */
unsigned char fileread(unsigned char f, int n)
{
        unsigned char s;
        char fname[NFILEN];
        (void)f; (void)n;

        if ((s=mlreply("Read file: ", fname, NFILEN)) != TRUE)
                return(s);
        return(readin(fname, TRUE));
}

/*
 * Select a file for editing.
 * Look around to see if you can find the
 * fine in another buffer; if you can find it
 * just switch to the buffer. If you cannot find
 * the file, create a new buffer, read in the
 * text, and switch to the new buffer.
 * Bound to C-X C-F.
 */
unsigned char filefind(unsigned char f, int n)
{
        char fname[NFILEN];	/* file user wishes to find */
        unsigned char s;	/* status return */
        (void)f; (void)n;

        if ((s=mlreply("Find file: ", fname, NFILEN)) != TRUE)
                return(s);
	return(getfile(fname, TRUE));
}

unsigned char getfile(char fname[], int lockfl)
{
        register BUFFER *bp;
        register LINE   *lp;
        unsigned char i;
        unsigned char s;
        char bname[NBUFN];	/* buffer name to put file */

        for (bp=bheadp; bp!=NULL; bp=bp->b_bufp) {
                if ((bp->b_flag&BFTEMP)==0 && strcmp(bp->b_fname, fname)==0) {
                        if (--curbp->b_nwnd == 0) {
                                curbp->b_dotp  = curwp->w_dotp;
                                curbp->b_doto  = curwp->w_doto;
                                curbp->b_markp = curwp->w_markp;
                                curbp->b_marko = curwp->w_marko;
                        }
			swbuffer(bp);
                        lp = curwp->w_dotp;
                        i = (term.t_nrow-1)/2;
                        while (i-- && lback(lp)!=curbp->b_linep)
                                lp = lback(lp);
                        curwp->w_linep = lp;
                        curwp->w_flag |= WFMODE|WFHARD;
                        mlwrite("[Exists]");
                        return (TRUE);
                }
        }
        makename(bname, fname);                 /* New buffer name.     */
        while ((bp=bfind(bname, FALSE, 0)) != NULL) {
                s = mlreply("Buffer name: ", bname, NBUFN);
                if (s == ABORT)                 /* ^G to just quit      */
                        return (s);
                if (s == FALSE) {               /* CR to clobber it     */
                        makename(bname, fname);
                        break;
                }
        }
        if (bp==NULL && (bp=bfind(bname, TRUE, 0))==NULL) {
                mlwrite("No buf");
                return (FALSE);
        }
        if (--curbp->b_nwnd == 0) {             /* Undisplay.           */
                curbp->b_dotp = curwp->w_dotp;
                curbp->b_doto = curwp->w_doto;
                curbp->b_markp = curwp->w_markp;
                curbp->b_marko = curwp->w_marko;
        }
        curbp = bp;                             /* Switch to it.        */
        curwp->w_bufp = bp;
        curbp->b_nwnd++;
        return(readin(fname, lockfl));          /* Read it in.          */
}

/*
 * Read file "fname" into the current
 * buffer, blowing away any text found there. Called
 * by both the read and find commands. Return the final
 * status of the read. Also called by the mainline,
 * to read in a file specified on the command line as
 * an argument.
 */
unsigned char readin(char fname[], int lockfl)
{
        register LINE   *lp1;
        register LINE   *lp2;
        unsigned char i;
        register BUFFER *bp;
        unsigned char s;
        unsigned char nbytes;
        register int    nline;
	int		lflag;		/* any lines longer than allowed? */
        char            line[NLINE];
        (void)lockfl;

        bp = curbp;                             /* Cheap.               */
        if ((s=bclear(bp)) != TRUE)             /* Might be old.        */
                return (s);
        bp->b_flag &= ~(BFTEMP|BFCHG);
        strcpy(bp->b_fname, fname);
        if ((s=ffropen(fname)) == FIOERR)       /* Hard file open.      */
                goto out;
        if (s == FIOFNF) {                      /* File not found.      */
                mlwrite("[New]");
                goto out;
        }
        mlwrite("[Reading]");
        nline = 0;
	lflag = FALSE;
        while ((s=ffgetline(line, NLINE)) == FIOSUC || s == FIOLNG) {
		if (s == FIOLNG)
			lflag = TRUE;
                nbytes = strlen(line);
                if ((lp1=lalloc(nbytes)) == NULL) {
                        s = FIOERR;             /* Keep message on the  */
                        break;                  /* display.             */
                }
                lp2 = lback(curbp->b_linep);
                lp2->l_fp = lp1;
                lp1->l_fp = curbp->b_linep;
                lp1->l_bp = lp2;
                curbp->b_linep->l_bp = lp1;
                for (i=0; i<nbytes; ++i)
                        lputc(lp1, i, line[i]);
                ++nline;
        }
        ffclose();                              /* Ignore errors.       */
        if (s == FIOEOF) {                      /* Don't zap message!   */
                if (nline == 1)
                        mlwrite("[1 line]");
                else
                        mlwrite("[%d lines]", nline);
        }
    if (lflag)
        mlwrite("[%d wrapped]", nline);
out:
        if (curwp->w_bufp == curbp) {
                curwp->w_linep = lforw(curbp->b_linep);
                curwp->w_dotp  = lforw(curbp->b_linep);
                curwp->w_doto  = 0;
                curwp->w_markp = NULL;
                curwp->w_marko = 0;
                curwp->w_flag |= WFMODE|WFHARD;
        }
        if (s == FIOERR || s == FIOFNF)		/* False if error.      */
                return(FALSE);
        return (TRUE);
}

/*
 * Take a file name, and from it
 * fabricate a buffer name. This routine knows
 * about the syntax of file names on the target system.
 * I suppose that this information could be put in
 * a better place than a line of code.
 */
void makename(char bname[], char fname[])
{
        register char   *cp1;
        register char   *cp2;

        cp1 = &fname[0];
        while (*cp1 != 0)
                ++cp1;

        while (cp1!=&fname[0] && cp1[-1]!='/')
                --cp1;
        cp2 = &bname[0];
        while (cp2!=&bname[NBUFN-1] && *cp1!=0 && *cp1!=';')
                *cp2++ = *cp1++;
        *cp2 = 0;
}

/*
 * Ask for a file name, and write the
 * contents of the current buffer to that file.
 * Update the remembered file name and clear the
 * buffer changed flag. This handling of file names
 * is different from the earlier versions, and
 * is more compatable with Gosling EMACS than
 * with ITS EMACS. Bound to "C-X C-W".
 */
unsigned char filewrite(unsigned char f, int n)
{
        unsigned char s;
        char            fname[NFILEN];
        (void)f; (void)n;

        if ((s=mlreply("Write file: ", fname, NFILEN)) != TRUE)
                return (s);
        if ((s=writeout(fname)) == TRUE) {
                strcpy(curbp->b_fname, fname);
                curbp->b_flag &= ~BFCHG;
                curwp->w_flag |= WFMODE;
        }
        return (s);
}

/*
 * Save the contents of the current
 * buffer in its associatd file. No nothing
 * if nothing has changed (this may be a bug, not a
 * feature). Error if there is no remembered file
 * name for the buffer. Bound to "C-X C-S". May
 * get called by "C-Z".
 */
unsigned char filesave(unsigned char f, int n)
{
        unsigned char s;
        (void)f; (void)n;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if ((curbp->b_flag&BFCHG) == 0)         /* Return, no changes.  */
                return (TRUE);
        if (curbp->b_fname[0] == 0) {           /* Must have a name.    */
                mlwrite("No fname");
                return (FALSE);
        }
        if ((s=writeout(curbp->b_fname)) == TRUE) {
                curbp->b_flag &= ~BFCHG;
                curwp->w_flag |= WFMODE;
        }
        return (s);
}

/*
 * This function performs the details of file
 * writing. Uses the file management routines in the
 * "fileio.c" package. The number of lines written is
 * displayed. Sadly, it looks inside a LINE; provide
 * a macro for this. Most of the grief is error
 * checking of some sort.
 */
unsigned char writeout(char * fn)
{
        unsigned char s;
        register LINE   *lp;
        register int    nline;

        if ((s=ffwopen(fn)) != FIOSUC)          /* Open writes message. */
                return (FALSE);
	mlwrite("[Writing]");			/* tell us were writing */
        lp = lforw(curbp->b_linep);             /* First line.          */
        nline = 0;                              /* Number of lines.     */
        while (lp != curbp->b_linep) {
                if ((s=ffputline(&lp->l_text[0], llength(lp))) != FIOSUC)
                        break;
                ++nline;
                lp = lforw(lp);
        }
        if (s == FIOSUC) {                      /* No write error.      */
                s = ffclose();
                if (s == FIOSUC) {              /* No close error.      */
                        if (nline == 1)
                                mlwrite("[Wrote 1]");
                        else
                                mlwrite("[Wrote %d]", nline);
                }
        } else                                  /* Ignore close error   */
                ffclose();                      /* if a write error.    */
        if (s != FIOSUC)                        /* Some sort of error.  */
                return (FALSE);
        return (TRUE);
}

/*
 * The command allows the user
 * to modify the file name associated with
 * the current buffer. It is like the "f" command
 * in UNIX "ed". The operation is simple; just zap
 * the name in the BUFFER structure, and mark the windows
 * as needing an update. You can type a blank line at the
 * prompt if you wish.
 */
unsigned char filename(unsigned char f, int n)
{
        unsigned char s;
        char            fname[NFILEN];
        (void)f; (void)n;

        if ((s=mlreply("Name: ", fname, NFILEN)) == ABORT)
                return (s);
        if (s == FALSE)
                strcpy(curbp->b_fname, "");
        else
                strcpy(curbp->b_fname, fname);
        curwp->w_flag |= WFMODE;
	curbp->b_mode &= ~MDVIEW;	/* no longer read only mode */
        return (TRUE);
}

/*
 * dired -- read-only directory listing, bound to ^X-D.
 *
 * Uses cc65's opendir/readdir (<dirent.h>) which opens CIO with AUX1=6
 * (directory read) internally — no assembly required.  Each readdir call
 * returns one dirent with d_name set to the clean 8.3 filename ("FOO.BAR").
 * The resulting *dired* buffer is read-only (MDVIEW); navigate with ^P/^N
 * and open files with ^X^F.
 */
/*
 * Fill (or refill) a dired buffer with the current directory listing.
 * Called both by dired() on first open and by swbuffer() on re-entry.
 */
unsigned char dired_fill(BUFFER *bp)
{
        LINE            *lp, *lp2;
        DIR             *dp;
        struct dirent   *de;
        unsigned char   len, i;

        /* Do NOT set BFTEMP: bfind returns NULL for BFTEMP buffers on
         * re-entry.  MDVIEW prevents edits so BFCHG stays clear, meaning
         * bclear will never prompt "Discard changes?" for this buffer. */
        if (bclear(bp) != TRUE)
                return (FALSE);

        dp = opendir("D:*.*");
        if (dp == NULL) {
                mlwrite("No dir");
                return (FALSE);
        }

        while ((de = readdir(dp)) != NULL) {
                len = strlen(de->d_name);
                if (len == 0)
                        continue;
                lp = lalloc(len);
                if (lp == NULL) {
                        closedir(dp);
                        mlwrite("No mem");
                        return (FALSE);
                }
                for (i = 0; i < len; i++)
                        lputc(lp, i, de->d_name[i]);
                lp2 = lback(bp->b_linep);
                lp2->l_fp = lp;
                lp->l_fp  = bp->b_linep;
                lp->l_bp  = lp2;
                bp->b_linep->l_bp = lp;
        }
        closedir(dp);

        bp->b_mode   |= MDVIEW;
        bp->b_flag   |= BFDIR;
        bp->b_active  = TRUE;
        strcpy(bp->b_fname, "D:");
        return (TRUE);
}

unsigned char dired(unsigned char f, int n)
{
        BUFFER          *bp;
        (void)f; (void)n;

        bp = bfind("*dired*", TRUE, 0);
        if (bp == NULL) {
                mlwrite("No dired");
                return (FALSE);
        }
        if (dired_fill(bp) != TRUE)
                return (FALSE);
        return (swbuffer(bp));
}
