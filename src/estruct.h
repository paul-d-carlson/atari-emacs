#ifndef ESTRUCT_H
#define ESTRUCT_H

/*	ESTRUCT:	Structure and preprocessor definitions for
			MicroEMACS 3.6 - Atari 8-bit cc65 port

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence

			Atari 8-bit only: cc65 compiler, target atari.
*/

/*	Terminal Output definitions		*/

#define	ATARI   1			/* Atari 8-bit cc65 target			*/

/*	Configuration options	*/

/*	internal constants	*/

#define	NBINDS	47			/* max # of bound keys		*/
#define NFILEN  16                      /* # of bytes, file name (D1:FILENAME.EXT = 15 max) */
#define NBUFN   16                      /* # of bytes, buffer name      */
#define NLINE   128                     /* # of bytes, line             */
#define	NSTRING	64			/* # of bytes, string buffers	*/
#define NPAT    32                      /* # of bytes, search pattern   */
#define HUGE    255                     /* Cursor sentinel (fits unsigned char) */

#define METACH  0x1B                    /* M- prefix,   Control-[, ESC  */

#undef  CTRL
#define CTRL    0x0100                  /* Control flag, or'ed in       */
#define META    0x0200                  /* Meta flag, or'ed in          */
#define CTLX    0x0400                  /* ^X flag, or'ed in            */

#define FALSE   0                       /* False, no, bad, etc.         */
#define TRUE    1                       /* True, yes, good, etc.        */
#define ABORT   2                       /* Death, ^G, abort, etc.       */

#define FIOSUC  0                       /* File I/O, success.           */
#define FIOFNF  1                       /* File I/O, file not found.    */
#define FIOEOF  2                       /* File I/O, end of file.       */
#define FIOERR  3                       /* File I/O, error.             */
#define	FIOLNG	4			/*line longer than allowed len	*/

#define CFCPCN  0x0001                  /* Last command was C-P, C-N    */
#define CFKILL  0x0002                  /* Last command was a kill      */

#define	BELL	0x07			/* a bell character		*/
#define	TAB	0x09			/* a tab character		*/

/*
 * There is a window structure allocated for every active display window. The
 * windows are kept in a big list, in top to bottom screen order, with the
 * listhead at "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands to guide
 * redisplay; although this is a bit of a compromise in terms of decoupling,
 * the full blown redisplay is just too expensive to run for every input
 * character.
 */
typedef struct  WINDOW {
        struct  BUFFER *w_bufp;         /* Buffer displayed in window   */
        struct  LINE *w_linep;          /* Top line in the window       */
        struct  LINE *w_dotp;           /* Line containing "."          */
        int     w_doto;                 /* Byte offset for "."          */
        struct  LINE *w_markp;          /* Line containing "mark"       */
        int     w_marko;                /* Byte offset for "mark"       */
        char    w_force;                /* If NZ, forcing row.          */
        unsigned char w_flag;                 /* Flags.                       */
}       WINDOW;

#define WFFORCE 0x01                    /* Window needs forced reframe  */
#define WFMOVE  0x02                    /* Movement from line to line   */
#define WFEDIT  0x04                    /* Editing within a line        */
#define WFHARD  0x08                    /* Better to a full display     */
#define WFMODE  0x10                    /* Update mode line.            */

/*
 * Text is kept in buffers. A buffer header, described below, exists for every
 * buffer in the system. The buffers are kept in a big list, so that commands
 * that search for a buffer by name can find the buffer header. There is a
 * safe store for the dot and mark in the header, but this is only valid if
 * the buffer is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with a pointer to
 * the header line in "b_linep".
 * 	Buffers may be "Inactive" which means the files accosiated with them
 * have not been read in yet. These get read in at "use buffer" time.
 */
typedef struct  BUFFER {
        struct  BUFFER *b_bufp;         /* Link to next BUFFER          */
        struct  LINE *b_dotp;           /* Link to "." LINE structure   */
        int     b_doto;                 /* Offset of "." in above LINE  */
        struct  LINE *b_markp;          /* The same as the above two,   */
        int     b_marko;                /* but for the "mark"           */
        struct  LINE *b_linep;          /* Link to the header LINE      */
	unsigned char b_active;		/* window activated flag	*/
        unsigned char b_nwnd;                 /* Count of windows on buffer   */
        unsigned char b_flag;                 /* Flags                        */
	unsigned char b_mode;			/* editor mode of this buffer	*/
        char    b_fname[NFILEN];        /* File name                    */
        char    b_bname[NBUFN];         /* Buffer name                  */
}       BUFFER;

#define BFTEMP  0x01                    /* Internal temporary buffer    */
#define BFCHG   0x02                    /* Changed since last write     */
#define BFDIR   0x04                    /* Synthetic dir buffer; no evict */

/*	mode flags	*/
#define	MDEXACT	0x0001			/* Exact matching for searches	*/
#define	MDVIEW	0x0002			/* read-only buffer		*/

/*
 * All text is kept in circularly linked lists of "LINE" structures. These
 * begin at the header line (which is the blank line beyond the end of the
 * buffer). This line is pointed to by the "BUFFER". Each line contains a the
 * number of bytes in the line (the "used" size), the size of the text array,
 * and the text. The end of line is not stored as a byte; it's implied. Future
 * additions will include update hints, and a list of marks into the line.
 */
typedef struct  LINE {
        struct  LINE *l_fp;             /* Link to the next line        */
        struct  LINE *l_bp;             /* Link to the previous line    */
        int     l_size;                 /* Allocated size               */
        int     l_used;                 /* Used size                    */
        char    l_text[1];              /* A bunch of characters.       */
}       LINE;

#define lforw(lp)       ((lp)->l_fp)
#define lback(lp)       ((lp)->l_bp)
#define lgetc(lp, n)    ((lp)->l_text[(n)]&0xFF)
#define lputc(lp, n, c) ((lp)->l_text[(n)]=(c))
#define llength(lp)     ((lp)->l_used)

/*
 * The editor communicates with the display using a high level interface. A
 * "TERM" structure holds useful variables, and indirect pointers to routines
 * that do useful operations. The low level get and put routines are here too.
 * This lets a terminal, in addition to having non standard commands, have
 * funny get and put character code too. The calls might get changed to
 * "termp->t_field" style in the future, to make it possible to run more than
 * one terminal type.
 */
typedef struct  {
        unsigned char t_nrow;           /* Number of rows.              */
        unsigned char t_ncol;           /* Number of columns.           */
	unsigned char t_margin;		/* min margin for extended lines*/
	unsigned char t_scrsiz;		/* size of scroll region "	*/
        void    (*t_open)(void);        /* Open terminal at the start.  */
        void    (*t_close)(void);       /* Close terminal at end.       */
        int     (*t_getchar)(void);     /* Get character from keyboard. */
        void    (*t_putchar)(int c);    /* Put character to display.    */
        void    (*t_flush)(void);       /* Flush output buffers.        */
        void    (*t_move)(unsigned char row, unsigned char col); /* Move the cursor, origin 0. */
        void    (*t_eeol)(void);        /* Erase to end of line.        */
        void    (*t_eeop)(void);        /* Erase to end of page.        */
        void    (*t_beep)(void);        /* Beep.                        */
	void	(*t_rev)(unsigned char state);	/* set reverse video state	*/
}       TERM;

/*	structure for the table of initial key bindings		*/

typedef struct  {
        short   k_code;                 /* Key code                     */
        unsigned char (*k_fp)(unsigned char f, int n);  /* Routine to handle it         */
}       KEYTAB;

#endif /* ESTRUCT_H */