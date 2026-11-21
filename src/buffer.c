/*
 * Buffer management.
 * Some of the functions are internal,
 * and some are actually attached to user
 * keys. Like everyone else, they set hints
 * for the display system.
 */
#include        <stdio.h>
#include	"estruct.h"
#include        "efunc.h"
#include        "edef.h"

/*
 * Attach a buffer to a window. The
 * values of dot and mark come from the buffer
 * if the use count is 0. Otherwise, they come
 * from some other window.
 */
unsigned char usebuffer(unsigned char f, int n)
{
        register BUFFER *bp;
        unsigned char s;
        char            bufn[NBUFN];
        (void)f; (void)n;

        if ((s=mlreply("Use buffer: ", bufn, NBUFN)) != TRUE)
                return (s);
        if ((bp=bfind(bufn, TRUE, 0)) == NULL)
                return (FALSE);
	return(swbuffer(bp));
}

unsigned char nextbuffer(unsigned char f, int n)
{
        register BUFFER *bp;
        (void)f; (void)n;

	bp = curbp->b_bufp;
	/* cycle through the buffers to find an eligable one */
	while (bp == NULL || (bp->b_flag & BFTEMP)) {
		if (bp == NULL)
			bp = bheadp;
		else
			bp = bp->b_bufp;
	}
	return(swbuffer(bp));
}

unsigned char swbuffer(BUFFER * bp)
{

        if (--curbp->b_nwnd == 0) {             /* Last use.            */
                curbp->b_dotp  = curwp->w_dotp;
                curbp->b_doto  = curwp->w_doto;
                curbp->b_markp = curwp->w_markp;
                curbp->b_marko = curwp->w_marko;
                /* Evict clean named buffers: free lines back to heap.  */
                /* readin() will restore them when the buffer is revisited. */
                if ((curbp->b_flag & (BFCHG|BFTEMP)) == 0 && curbp->b_fname[0]) {
                        bclear(curbp);
                        curbp->b_active = FALSE;
                }
        }
        curbp = bp;                             /* Switch.              */
	if (curbp->b_active != TRUE) {		/* buffer not active yet*/
		/* read it in and activate it */
		if (curbp->b_flag & BFDIR)
			dired_fill(curbp);
		else
			readin(curbp->b_fname, TRUE);
		curbp->b_dotp = lforw(curbp->b_linep);
		curbp->b_doto = 0;
		curbp->b_active = TRUE;
	}
        curwp->w_bufp  = bp;
        curwp->w_linep = bp->b_linep;           /* For macros, ignored. */
        curwp->w_flag |= WFMODE|WFFORCE|WFHARD; /* Quite nasty.         */
        bp->b_nwnd++;
        curwp->w_dotp  = bp->b_dotp;
        curwp->w_doto  = bp->b_doto;
        curwp->w_markp = bp->b_markp;
        curwp->w_marko = bp->b_marko;
        return (TRUE);
}

/*
 * Dispose of a buffer, by name.
 * Ask for the name. Look it up (don't get too
 * upset if it isn't there at all!). Get quite upset
 * if the buffer is being displayed. Clear the buffer (ask
 * if the buffer has been changed). Then free the header
 * line and the buffer header. Bound to "C-X K".
 */
unsigned char killbuffer(unsigned char f, int n)
{
	register BUFFER *bp;
        unsigned char s;
        char bufn[NBUFN];
        (void)f; (void)n;

        if ((s=mlreply("Kill buffer: ", bufn, NBUFN)) != TRUE)
                return(s);
        if ((bp=bfind(bufn, FALSE, 0)) == NULL) {
                mlwrite("No such buf");
                return (TRUE);
        }
	return(zotbuf(bp));
}

unsigned char zotbuf(register BUFFER * bp)
{
        register BUFFER *bp1;
        register BUFFER *bp2;
        unsigned char s;

        if (bp->b_nwnd != 0) {                  /* Error if on screen.  */
                mlwrite("Buf shown");
                return (FALSE);
        }
        if ((s=bclear(bp)) != TRUE)             /* Blow text away.      */
                return (s);
        free((char *) bp->b_linep);             /* Release header line. */
        bp1 = NULL;                             /* Find the header.     */
        bp2 = bheadp;
        while (bp2 != bp) {
                bp1 = bp2;
                bp2 = bp2->b_bufp;
        }
        bp2 = bp2->b_bufp;                      /* Next one in chain.   */
        if (bp1 == NULL)                        /* Unlink it.           */
                bheadp = bp2;
        else
                bp1->b_bufp = bp2;
        free((char *) bp);                      /* Release buffer block */
        return (TRUE);
}




/*
 * Look through the list of
 * buffers. Return TRUE if there
 * are any changed buffers. Buffers
 * that hold magic internal stuff are
 * not considered; who cares if the
 * list of buffer names is hacked.
 * Return FALSE if no buffers
 * have been changed.
 */
int anycb(void)
{
        register BUFFER *bp;

        bp = bheadp;
        while (bp != NULL) {
                if ((bp->b_flag&BFTEMP)==0 && (bp->b_flag&BFCHG)!=0)
                        return (TRUE);
                bp = bp->b_bufp;
        }
        return (FALSE);
}

/*
 * Find a buffer, by name. Return a pointer
 * to the BUFFER structure associated with it. If
 * the named buffer is found, but is a TEMP buffer (like
 * the buffer list) conplain. If the buffer is not found
 * and the "cflag" is TRUE, create it. The "bflag" is
 * the settings for the flags in in buffer.
 */
BUFFER * bfind(register char * bname, int cflag, int bflag)
{
        register BUFFER *bp;
	register BUFFER *sb;	/* buffer to insert after */
        register LINE   *lp;

        bp = bheadp;
        while (bp != NULL) {
                if (strcmp(bname, bp->b_bname) == 0) {
                        if ((bp->b_flag&BFTEMP) != 0) {
                                mlwrite("Builtin buf");
                                return (NULL);
                        }
                        return (bp);
                }
                bp = bp->b_bufp;
        }
        if (cflag != FALSE) {
                if ((bp=(BUFFER *)malloc(sizeof(BUFFER))) == NULL)
                        return (NULL);
                if ((lp=lalloc(0)) == NULL) {
                        free((char *) bp);
                        return (NULL);
                }
		/* find the place in the list to insert this buffer */
		if (bheadp == NULL || strcmp(bheadp->b_bname, bname) > 0) {
			/* insert at the begining */
	                bp->b_bufp = bheadp;
        	        bheadp = bp;
        	} else {
			sb = bheadp;
			while (sb->b_bufp != NULL) {
				if (strcmp(sb->b_bufp->b_bname, bname) > 0)
					break;
				sb = sb->b_bufp;
			}

			/* and insert it */
       			bp->b_bufp = sb->b_bufp;
        		sb->b_bufp = bp;
       		}

		/* and set up the other buffer fields */
		bp->b_active = TRUE;
                bp->b_dotp  = lp;
                bp->b_doto  = 0;
                bp->b_markp = NULL;
                bp->b_marko = 0;
                bp->b_flag  = bflag;
		bp->b_mode  = 0;
                bp->b_nwnd  = 0;
                bp->b_linep = lp;
                strcpy(bp->b_fname, "");
                strcpy(bp->b_bname, bname);
                lp->l_fp = lp;
                lp->l_bp = lp;
        }
        return (bp);
}

/*
 * This routine blows away all of the text
 * in a buffer. If the buffer is marked as changed
 * then we ask if it is ok to blow it away; this is
 * to save the user the grief of losing text. The
 * window chain is nearly always wrong if this gets
 * called; the caller must arrange for the updates
 * that are required. Return TRUE if everything
 * looks good.
 */
unsigned char bclear(register BUFFER * bp)
{
        register LINE   *lp;
        unsigned char s;

        if ((bp->b_flag&BFTEMP) == 0            /* Not scratch buffer.  */
        && (bp->b_flag&BFCHG) != 0              /* Something changed    */
        && (s=mlyesno("Discard?")) != TRUE)
                return (s);
        bp->b_flag  &= ~BFCHG;                  /* Not changed          */
        while ((lp=lforw(bp->b_linep)) != bp->b_linep)
                lfree(lp);
        bp->b_dotp  = bp->b_linep;              /* Fix "."              */
        bp->b_doto  = 0;
        bp->b_markp = NULL;                     /* Invalidate "mark"    */
        bp->b_marko = 0;
        return (TRUE);
}
