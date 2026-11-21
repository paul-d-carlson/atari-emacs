#ifndef EDEF_H
#define EDEF_H

/*	EDEF:		Global variable definitions for
			MicroEMACS 3.2

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef	maindef

/* for MAIN.C */

/* initialized global definitions */

char    pat[NPAT];                      /* Search pattern		*/
unsigned char   sgarbf  = TRUE;                 /* TRUE if screen is garbage	*/
unsigned char   mpresf  = FALSE;                /* TRUE if message in last line */

/* uninitialized global definitions */

unsigned char currow;           /* Cursor row                   */
unsigned char curcol;           /* Cursor column                */
unsigned char   thisflag;               /* Flags, this command          */
unsigned char   lastflag;               /* Flags, last command          */
unsigned char curgoal;          /* Goal for C-P, C-N            */
WINDOW  *curwp;                 /* Current window               */
BUFFER  *curbp;                 /* Current buffer               */
BUFFER  *bheadp;                /* Head of list of buffers      */
BUFFER  *bfind(register char *bname, int cflag, int bflag); /* Lookup a buffer by name      */
LINE    *lalloc(register int used); /* Allocate a line              */

#else

/* for all the other .C files */

/* initialized global external declarations */

extern  char    pat[];                  /* Search pattern               */
extern	KEYTAB keytab[];		/* key bind to functions table	*/
extern  unsigned char   sgarbf;                 /* State of screen unknown      */
extern  unsigned char   mpresf;                 /* Stuff in message line        */

/* initialized global external declarations */

extern  unsigned char currow;           /* Cursor row                   */
extern  unsigned char curcol;           /* Cursor column                */
extern  unsigned char   thisflag;               /* Flags, this command          */
extern  unsigned char   lastflag;               /* Flags, last command          */
extern  unsigned char curgoal;          /* Goal for C-P, C-N            */
extern  WINDOW  *curwp;                 /* Current window               */
extern  BUFFER  *curbp;                 /* Current buffer               */
extern  BUFFER  *bheadp;                /* Head of list of buffers      */
extern  BUFFER  *bfind(register char *bname, int cflag, int bflag); /* Lookup a buffer by name      */
extern  LINE    *lalloc(register int used); /* Allocate a line              */

#endif

/* terminal table defined only in TERM.C */

extern  TERM    term;                   /* Terminal information.        */

#endif /* EDEF_H */


