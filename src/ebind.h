#ifndef EBIND_H
#define EBIND_H

/*	EBIND:		Initial default key to function bindings for
			MicroEMACS 3.2

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

/*
 * Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This expains the funny location of the
 * control-X commands.
 */
KEYTAB  keytab[NBINDS] = {
	{CTRL|'A',		gotobol},
	{CTRL|'B',		backchar},
	{CTRL|'C',		insspace},
	{CTRL|'D',		forwdel},
	{CTRL|'E',		gotoeol},
	{CTRL|'F',		forwchar},
	{CTRL|'G',		ctrlg},
	{CTRL|'H',		backdel},
	{CTRL|'I',		tab},
	{CTRL|'J',		indent},
	{CTRL|'K',		killtext},
	{CTRL|'L',		refresh},
	{CTRL|'M',		newline},
	{CTRL|'N',		forwline},
	{CTRL|'O',		openline},
	{CTRL|'P',		backline},
	{CTRL|'Q',		quote},
	{CTRL|'S',		forwsearch},
	{CTRL|'V',		forwpage},
	{CTRL|'Y',		yank},
	{CTRL|'Z',		backpage},
	{CTLX|CTRL|'C',		quit},          /* Hard quit.           */
	{CTLX|CTRL|'F',		filefind},
	{CTLX|CTRL|'R',		fileread},
	{CTLX|CTRL|'S',		filesave},
	{CTLX|CTRL|'W',		filewrite},
	{CTLX|CTRL|'X',		swapmark},
	{CTLX|'=',		showcpos},

	{CTLX|'B',			usebuffer},
	{CTLX|'D',			dired},		/* Directory listing */
	{CTLX|'K',			killbuffer},
	{CTLX|'N',		filename},
	{CTLX|'S',		filesave},
	{CTLX|'X',		nextbuffer},
	{META|CTRL|'L',		reposition},
	{META|' ',		setmark},
	{META|'?',		help},
	{META|'!',		reposition},
	{META|'.',		setmark},
	{META|'>',		gotoeob},
	{META|'<',		gotobob},
	{META|'G',		gotoline},
	{META|'V',		backpage},
	{META|'Z',		quit},
	{0x7E,			backdel},	/* Atari DELETE key (ATASCII 0x7E) */
	{0x9B,			newline},	/* Atari RETURN key (ATASCII 0x9B EOL) */
	{0,			NULL}
};

#endif /* EBIND_H */
