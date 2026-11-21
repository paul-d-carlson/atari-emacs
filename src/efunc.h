#ifndef EFUNC_H
#define EFUNC_H

/*	EFUNC.H:	MicroEMACS function declarations and names

		This file list all the C code functions used by MicroEMACS
	and the names to use to bind keys to them. To add functions,
	declare it here in both the extern function list and the name
	binding table.

	Update History:

	Daniel Lawrence
		29-jan-86
		- separeted out external declarations to a separate .h file
		- built original name to function binding table
		30-jan-86
		- added command declarations for Fill Paragraph command
		11-feb-86
		- added declaration for help and describe key commands
		13-feb-86
		- added declarations for view-file command
		15-feb-86
		- added declataitons for insert file command
		17-feb-86
		- added declarations for scroll next window up/down
		20-feb-86
		- expanded binding table to make room for new bindings
		24-feb-86
		- added declarations for bind-to-key and unbind-key
		  execute named command and describe bindings
		26-feb-86
		- added declarations for execute command
		- changed name of "visit-file" to "find-file"
		- added declaration for execute-buffer command
		27-feb-86
		- added declarations for execute-file command
		3-apr-86
		- added declarations for next-buffer command
		5-apr-86
		- added declarations for kill-paragraph command
		8-apr-86
		- added declarations for add/delete global mode
		8-apr-86
		- added declarations for insert space command
*/

/*	External function declarations		  */

extern	unsigned char	ctrlg(unsigned char f, int n);		/* Abort out of things		  */
extern	unsigned char	quit(unsigned char f, int n);		/* Quit				  */
extern	unsigned char	fileread(unsigned char f, int n);		/* Get a file, read only		  */
extern	unsigned char	filefind(unsigned char f, int n);		/* Get a file, read write	  */
extern	unsigned char	filewrite(unsigned char f, int n);		/* Write a file			  */
extern	unsigned char	filesave(unsigned char f, int n);		/* Save current file		  */
extern	unsigned char	filename(unsigned char f, int n);		/* Adjust file name		  */
extern	unsigned char	dired(unsigned char f, int n);		/* Directory listing		  */
extern	unsigned char	dired_fill(BUFFER *bp);		/* (Re)fill a dired buffer	  */
extern	unsigned char	getccol(int bflg);		/* Get current column		  */
extern	unsigned char	gotobol(unsigned char f, int n);		/* Move to start of line		  */
extern	unsigned char	forwchar(unsigned char f, int n);		/* Move forward by characters  */
extern	unsigned char	gotoeol(unsigned char f, int n);		/* Move to end of line		  */
extern	unsigned char	backchar(unsigned char f, int n);		/* Move backward by characters */
extern	unsigned char	forwline(unsigned char f, int n);		/* Move forward by lines		  */
extern	unsigned char	backline(unsigned char f, int n);		/* Move backward by lines	  */
extern	unsigned char	forwpage(unsigned char f, int n);		/* Move forward by pages		  */
extern	unsigned char	backpage(unsigned char f, int n);		/* Move backward by pages	  */
extern	unsigned char	gotobob(unsigned char f, int n);		/* Move to start of buffer	  */
extern	unsigned char	gotoeob(unsigned char f, int n);		/* Move to end of buffer		  */
extern	unsigned char	setmark(unsigned char f, int n);		/* Set mark			  */
extern	unsigned char	swapmark(unsigned char f, int n);		/* Swap "." and mark		  */
extern	unsigned char	forwsearch(unsigned char f, int n);		/* Search forward		  */
extern	int		eq(int bc, int pc);				/* Compare chars, honoring MDEXACT */

extern	unsigned char	showcpos(unsigned char f, int n);		/* Show the cursor position	  */
extern	unsigned char	usebuffer(unsigned char f, int n);		/* Switch a window to a buffer  */
extern	unsigned char	killbuffer(unsigned char f, int n);		/* Make a buffer go away.  */
extern	unsigned char	reposition(unsigned char f, int n);		/* Reposition window  */
extern	unsigned char	refresh(unsigned char f, int n);		/* Refresh the screen  */
extern	unsigned char	tab(unsigned char f, int n);		/* Insert tab  */
extern	unsigned char	newline(unsigned char f, int n);		/* Insert CR-LF  */
extern	unsigned char	indent(unsigned char f, int n);		/* Insert CR-LF, then indent  */
extern	unsigned char	openline(unsigned char f, int n);		/* Open up a blank line  */
extern	unsigned char	quote(unsigned char f, int n);		/* Insert literal  */
extern	unsigned char	forwdel(unsigned char f, int n);		/* Forward delete  */
extern	unsigned char	backdel(unsigned char f, int n);		/* Backward delete  */
extern	unsigned char	killtext(unsigned char f, int n);		/* Kill forward  */
extern	unsigned char	yank(unsigned char f, int n);		/* Yank back from killbuffer.  */
extern	unsigned char	gotoline(unsigned char f, int n);		/* go to a numbered line  */
extern	unsigned char	help(unsigned char f, int n);		/* get the help file here  */
extern	unsigned char	nextbuffer(unsigned char f, int n);		/* switch to the next buffer  */
extern	unsigned char	insspace(unsigned char f, int n);		/* insert a space forword  */

/* --- Internal helper and utility function declarations --- */

/* main.c */
extern	void	edinit(char bname[]);
extern	unsigned char	execute(int c, unsigned char f, int n);
extern	int	getkey(void);
extern	int	getctl(void);

/* display.c */
extern	void	mlwrite(char *fmt, ...);
extern	void	mlputs(char *s);
extern	void	mlputi(int i, int r);
extern	void	mlerase(void);
extern	unsigned char	mlreply(char *prompt, char *buf, int nbuf);
extern	unsigned char	mlyesno(char *prompt);
extern	unsigned char	mlreplyt(char *prompt, char *buf, int nbuf, char eolchar);
extern	void	update(void);
extern	void	updateline(unsigned char row, char vline[], unsigned char *flags);
extern	void	updext(void);
extern	void	modeline(void);
extern	void	movecursor(unsigned char row, unsigned char col);
extern	void	vtinit(void);
extern	void	vtmove(int row, int col);
extern	void	vtputc(int c);
extern	void	vtpute(int c);
extern	void	vteeol(void);
extern	void	vttidy(void);

/* random.c */
extern	unsigned char	rdonly(void);

/* buffer.c */
extern	unsigned char	bclear(BUFFER *bp);
extern	unsigned char	zotbuf(BUFFER *bp);
extern	int	anycb(void);
extern	unsigned char	swbuffer(BUFFER *bp);
extern	void	makename(char bname[], char fname[]);

/* line.c */
extern	void	lfree(LINE *lp);
extern	unsigned char	linsert(int n, int c);
extern	unsigned char	ldelete(int n, unsigned char kflag);
extern	unsigned char	ldelnewline(void);
extern	unsigned char	lnewline(void);
extern	void	lchange(int flag);
extern	void	kdelete(void);
extern	unsigned char	kinsert(int c);
extern	int	kremove(int n);

/* file.c */
extern	unsigned char	getfile(char fname[], int lockfl);
extern	unsigned char	readin(char fname[], int lockfl);
extern	unsigned char	writeout(char *fn);

/* fileio.c */
extern	unsigned char	ffropen(char *fn);
extern	unsigned char	ffwopen(char *fn);
extern	unsigned char	ffclose(void);
extern	unsigned char	ffgetline(char buf[], int nbuf);
extern	unsigned char	ffputline(char buf[], int nbuf);

/* search.c */
extern	unsigned char	forscan(char *patrn, int leavep);
extern	unsigned char	readpattern(char *prompt);
extern	int	eq(int bc, int pc);

/* bind.c */
extern	unsigned char	expandp(char *srcstr, char *deststr, signed char maxlength);

/* termio.c */
extern	int	typahead(void);

/* basic.c (file-local forward reference) */
extern	int	getgoal(LINE *dlp);

#endif /* EFUNC_H */
