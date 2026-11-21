/*
 * Window management. Some of the functions are internal, and some are
 * attached to keys that the user actually types.
 */

#include        <stdio.h>
#include        "estruct.h"
#include        "efunc.h"
#include	"edef.h"

/*
 * Reposition dot in the current window to line "n". If the argument is
 * positive, it is that line. If it is negative it is that line from the
 * bottom. If it is 0 the window is centered (this is what the standard
 * redisplay code does). With no argument it defaults to 1. Bound to M-!.
 * Because of the default, it works like in Gosling.
 */
unsigned char reposition(unsigned char f, int n)
    {
    if (f == FALSE)	/* default to 0 to center screen */
	n = 0;
    curwp->w_force = n;
    curwp->w_flag |= WFFORCE;
    return (TRUE);
    }

/*
 * Refresh the screen. With no argument, it just does the refresh. With an
 * argument it recenters "." in the current window. Bound to "C-L".
 */
unsigned char refresh(unsigned char f, int n)
    {
    (void)n;
    if (f == FALSE)
        sgarbf = TRUE;
    else
        {
        curwp->w_force = 0;             /* Center dot. */
        curwp->w_flag |= WFFORCE;
        }

    return (TRUE);
    }


