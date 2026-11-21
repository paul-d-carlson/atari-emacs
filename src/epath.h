#ifndef EPATH_H
#define EPATH_H

/*	PATH:	This file contains certain info needed to locate the
		MicroEMACS files on a system dependant basis.

		Atari 8-bit version.
		Atari DOS device naming: D: = default drive, D1:-D8: = specific.
		Filenames are upper-case, max 8.3 format (e.g. EMACS.HLP).
									*/

/*	possible names and paths of help/init files on Atari DOS	*/

char *pathname[] = {

	"D:EMACS.RC",		/* startup init file			*/
	"D:EMACS.HLP",		/* help file				*/
	"D:",			/* system data path (default drive)	*/
	""
};

#define	NPNAMES	(sizeof(pathname)/sizeof(char *))

#endif /* EPATH_H */
