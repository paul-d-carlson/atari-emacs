/*	This file is for functions having to do with key bindings,
	descriptions, help commands, and command line execution.

	written 11-feb-86 by Daniel Lawrence
								*/

#include	<stdio.h>
#include	"estruct.h"
#include	"efunc.h"
#include	"edef.h"
#include	"epath.h"

unsigned char help(unsigned char f, int n)
{
        unsigned char status;    /* status of I/O operations */
        unsigned char i;         /* index into help file names */
        char fname[NSTRING];    /* buffer to construct file name in */
        (void)f; (void)n;

        /* search through the list of help files */
        for (i=2; i < NPNAMES; i++) {
                strcpy(fname, pathname[i]);
                strcat(fname, pathname[1]);
                status = ffropen(fname);
                if (status == FIOSUC)
                        break;
        }

        if (status == FIOFNF) {
                mlwrite("[Help file is not online]");
                return(FALSE);
        }
        ffclose();      /* close the file to prepare for to read it in */

        /* and read the stuff in */
        if (getfile(fname, FALSE) == FALSE)
                return(FALSE);

        /* make this window in VIEW mode, update all mode lines */
        curwp->w_bufp->b_mode |= MDVIEW;
        curwp->w_flag |= WFMODE;
        return(TRUE);
}

