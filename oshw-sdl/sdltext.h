/* sdltext.h: Internal font-drawing functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_sdltext_h_
#define	_sdltext_h_

#include	"SDL.h"

/*
 * Basic text display.
 */

/* The definition of a font.
 */
typedef	struct fontinfo {
    unsigned char      *bits;	/* 256 characters times h bytes */
    Uint32		color;	/* the color index to use for the font */
    short		w;	/* width of each character */
    short		h;	/* height of each character */
} fontinfo;

/* Specify the surface to draw on and the font to use for the
 * remaining functions in this module.
 */
extern void _sdlsettextsurface(SDL_Surface *s);
extern void _sdlsettextfont(fontinfo const *f);

/* Draw a line of NUL-terminated text. xpos and ypos specify the
 * upper-left corner of the rectangle to draw in.
 */
extern void _sdlputtext(int xpos, int ypos, char const *text);

/* Draw a line of text len characters long.
 */
extern void _sdlputntext(int xpos, int ypos, int len, char const *text);

/* Draw one or more lines of text, breaking the string up at the
 * whitespace characters (if possible). area defines the rectangle to
 * draw in. Upon return, area specifies the area not draw in.
 */
extern void _sdlputmltext(SDL_Rect *area, char const *text);

/*
 * Functions for displaying a scrollable list.
 */

/* The data mantained for a scrolling list.
 */
typedef	struct scrollinfo {
    SDL_Rect		area;		/* the rectangle to draw in */
    Uint32		bkgnd;		/* the background color index */
    Uint32		highlight;	/* the highlight color index */
    int			itemcount;	/* the number of items in the list */
    short		linecount;	/* how many lines fit in area */
    short		maxlen;		/* how many chars fit in one line */
    int			topitem;	/* the uppermost visible line */
    int			index;		/* the line currently selected */
    char const	      **items;		/* the list of lines of text */
} scrollinfo;

/* Create a scrolling list, initializing the given scrollinfo
 * structure with the other arguments.
 */
extern int _sdlcreatescroll(scrollinfo *scroll, SDL_Rect const *area,
			    Uint32 bkgndcolor, Uint32 highlightcolor,
			    int itemcount, char const **items);

/* Display the scrolling list.
 */
extern void _sdlscrollredraw(scrollinfo *scroll);

/* Return the index of the selected line.
 */
extern int _sdlscrollindex(scrollinfo const *scroll);

/* Change the index of the selected line to pos.
 */
extern int _sdlscrollsetindex(scrollinfo *scroll, int pos);

/* Change the index of the selected line.
 */
extern int _sdlscrollmove(scrollinfo *scroll, int delta);


#endif
