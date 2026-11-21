/*
 * The functions in this file implement commands that search in the forward
 * and backward directions. There are no special characters in the search
 * strings. Probably should have a regular expression search, or something
 * like that.
 *
 */

#include        <stdio.h>
#include	"estruct.h"
#include        "efunc.h"
#include        "edef.h"

/*
 * Search forward. Get a search string from the user, and search, beginning at
 * ".", for the string. If found, reset the "." to be just after the match
 * string, and [perhaps] repaint the display. Bound to "C-S".
 */

/*	string search input parameters	*/

#define	PTBEG	1	/* leave the point at the begining on search */
#define	PTEND	2	/* leave the point at the end on search */

unsigned char forwsearch(unsigned char f, int n)
{
	unsigned char status;
        (void)f;

	/* resolve the repeat count */
	if (n == 0)
		n = 1;
	/* ask the user for the text of a pattern */
	if ((status = readpattern("Search")) != TRUE)
		return(status);

	/* search for the pattern */
	while (n-- > 0) {
		if ((status = forscan(&pat[0],PTEND)) == FALSE)
			break;
	}

	/* and complain if not there */
	if (status == FALSE)
		mlwrite("Not found");
	return(status);
}




/*
 * Compare two characters. The "bc" comes from the buffer. It has it's case
 * folded out. The "pc" is from the pattern.
 */
int eq(int bc, int pc)
{
	if ((curwp->w_bufp->b_mode & MDEXACT) == 0) {
		if (bc>='a' && bc<='z')
			bc -= 0x20;

		if (pc>='a' && pc<='z')
			pc -= 0x20;
	}

	if (bc == pc)
		return(TRUE);

	return(FALSE);
}

/*
 * Read a pattern. Stash it in the external variable "pat". The "pat" is not
 * updated if the user types in an empty line. If the user typed an empty line,
 * and there is no old pattern, it is an error. Display the old pattern, in the
 * style of Jeff Lomicka. There is some do-it-yourself control expansion.
 * change to using <ESC> to delemit the end-of-pattern to allow <NL>s in
 * the search string.
 */
unsigned char readpattern(char * prompt)
{
	unsigned char s;
	char tpat[NPAT+20];

	strcpy(tpat, prompt);	/* copy prompt to output string */
	strcat(tpat, " [");	/* build new prompt string */
	expandp(&pat[0], &tpat[strlen(tpat)], NPAT/2);	/* add old pattern */
	strcat(tpat, "]<ESC>: ");

	s = mlreplyt(tpat, tpat, NPAT, 27);	/* Read pattern */

	if (s == TRUE)				/* Specified */
		strcpy(pat, tpat);
	else if (s == FALSE && pat[0] != 0)	/* CR, but old one */
		s = TRUE;

	return(s);
}

unsigned char forscan(char * patrn, int leavep)
{
	register LINE *curline;		/* current line during scan */
	unsigned char curoff;		/* position within current line */
	register LINE *lastline;		/* last line position during scan */
	unsigned char lastoff;		/* position within last line */
	register unsigned char c;		/* character at current position */
	register LINE *matchline;		/* current line during matching */
	unsigned char matchoff;		/* position in matching line */
	register char *patptr;		/* pointer into pattern */

	/* setup local scan pointers to global "." */

	curline = curwp->w_dotp;
	curoff = curwp->w_doto;

	/* scan each character until we hit the head link record */

	while (curline != curbp->b_linep) {

		/* save the current position in case we need to
		   restore it on a match			*/

		lastline = curline;
		lastoff = curoff;

		/* get the current character resolving EOLs */

		if (curoff == llength(curline)) {	/* if at EOL */
			curline = lforw(curline);	/* skip to next line */
			curoff = 0;
			c = '\n';			/* and return a <NL> */
		} else
			c = lgetc(curline, curoff++);	/* get the char */

		/* test it against first char in pattern */
		if (eq(c, patrn[0]) != FALSE) {	/* if we find it..*/
			/* setup match pointers */
			matchline = curline;
			matchoff = curoff;
			patptr = &patrn[0];

			/* scan through patrn for a match */
			while (*++patptr != 0) {
				/* advance all the pointers */
				if (matchoff == llength(matchline)) {
					/* advance past EOL */
					matchline = lforw(matchline);
					matchoff = 0;
					c = '\n';
				} else
					c = lgetc(matchline, matchoff++);

				/* and test it against the pattern */
				if (eq(*patptr, c) == FALSE)
					goto fail;
			}

			/* A SUCCESSFULL MATCH!!! */
			/* reset the global "." pointers */
			if (leavep == PTEND) {	/* at end of string */
				curwp->w_dotp = matchline;
				curwp->w_doto = matchoff;
			} else {		/* at begining of string */
				curwp->w_dotp = lastline;
				curwp->w_doto = lastoff;
			}
			curwp->w_flag |= WFMOVE; /* flag that we have moved */
			return(TRUE);

		}
fail:;			/* continue to search */
	}

	/* we could not find a match */

	return(FALSE);
}

/* 	expandp:	expand control key sequences for output		*/

unsigned char expandp(char *srcstr, char *deststr, signed char maxlength)
{
	char c;		/* current char to translate */

	/* scan through the string */
	while ((c = *srcstr++) != 0) {
		if (c == '\n') {		/* its an EOL */
			*deststr++ = '<';
			*deststr++ = 'N';
			*deststr++ = 'L';
			*deststr++ = '>';
			maxlength -= 4;
		} else if (c < 0x20 || c == 0x7f) {	/* control character */
			*deststr++ = '^';
			*deststr++ = c ^ 0x40;
			maxlength -= 2;
		} else if (c == '%') {
			*deststr++ = '%';
			*deststr++ = '%';
			maxlength -= 2;

		} else {			/* any other character */
			*deststr++ = c;
			maxlength--;
		}

		/* check for maxlength */
		if (maxlength < 4) {
			*deststr++ = '$';
			*deststr = '\0';
			return(FALSE);
		}
	}
	*deststr = '\0';
	return(TRUE);
}
