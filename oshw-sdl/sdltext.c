/* sdltext.c: Font-drawing functions for SDL.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<string.h>
#include	<ctype.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../err.h"

/*
 * Erasing functions.
 */

/* A simple routine to fill part of a scanline with a single color.
 */
static void *fillscanline(void *scanline, int bpp, int count, Uint32 color)
{
    Uint8      *p = scanline;

    if (!color) {
	memset(p, 0, bpp * count);
	return p + bpp * count;
    }

    p = scanline;
    while (count--) {
	switch (bpp) {
	  case 1:	*p = (Uint8)color;		break;
	  case 2:	*(Uint16*)p = (Uint16)color;	break;
	  case 4:	*(Uint32*)p = color;		break;
	  case 3:
	    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		p[0] = (Uint8)(color >> 16);
		p[1] = (Uint8)(color >> 8);
		p[2] = (Uint8)color;
	    } else {
		p[0] = (Uint8)color;
		p[1] = (Uint8)(color >> 8);
		p[2] = (Uint8)(color >> 16);
	    }
	    break;
	}
	p += bpp;
    }
    return p;
}

/* Fill a rectangular area with the background color.
 */
static void fillrect(SDL_Rect const *rect)
{
    unsigned char      *scanline;
    int			bpp, y;

    bpp = sdlg.screen->format->BytesPerPixel;
    scanline = sdlg.screen->pixels;
    scanline += rect->y * sdlg.screen->pitch + rect->x * bpp;
    for (y = rect->h ; y ; --y, scanline += sdlg.screen->pitch)
	fillscanline(scanline, bpp, rect->w, sdlg.font.bkgnd);
}

/*
 * The text rendering functions.
 */

/* The routine to draw a line of text to the screen.
 */
static void drawtext(SDL_Rect *rect, char const *text, int len, int flags)
{
    Uint8		       *scanline;
    Uint8		       *p;
    unsigned char const	       *glyph;
    unsigned char		bit;
    Uint32			c;
    int				pitch, bpp, indent;
    int				n, x, y;

    n = len * sdlg.font.w;
    if (n > rect->w) {
	len = rect->w / sdlg.font.w;
	n = len * sdlg.font.w;
    }
    indent = flags & PT_RIGHT ? rect->w - n
	   : flags & PT_CENTER ? (rect->w - n) / 2 : 0;

    pitch = sdlg.screen->pitch;
    bpp = sdlg.screen->format->BytesPerPixel;
    scanline = sdlg.screen->pixels + rect->y * pitch + rect->x * bpp;

    for (y = 0 ; y < sdlg.font.h && y < rect->h ; ++y) {
	p = fillscanline(scanline, bpp, indent, sdlg.font.bkgnd);
	for (n = 0 ; n < len ; ++n) {
	    glyph = sdlg.font.bits;
	    glyph += *(unsigned char const*)(text + n) * sdlg.font.h;
	    for (x = sdlg.font.w, bit = 128 ; x ; --x, bit >>= 1) {
		c = glyph[y] & bit ? sdlg.font.color : sdlg.font.bkgnd;
		switch (bpp) {
		  case 1:	*p = (Uint8)c;			break;
		  case 2:	*(Uint16*)p = (Uint16)c;	break;
		  case 4:	*(Uint32*)p = c;		break;
		  case 3:
		    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			p[0] = (Uint8)(c >> 16);
			p[1] = (Uint8)(c >> 8);
			p[2] = (Uint8)c;
		    } else {
			p[0] = (Uint8)c;
			p[1] = (Uint8)(c >> 8);
			p[2] = (Uint8)(c >> 16);
		    }
		    break;
		}
		p += bpp;
	    }
	}
	x = rect->w - len * sdlg.font.w;
	if (x > 0)
	    fillscanline(p, bpp, x, sdlg.font.bkgnd);
	scanline += pitch;
    }
    if (flags & PT_UPDATERECT) {
	rect->y += y;
	rect->h -= y;
    }
}

/* Determine the number of lines needed to display a text.
 */
static void measuremltext(char const *text, int *w, int *h)
{
    int		maxline, linecount;
    int		index, width, n;

    width = *w / sdlg.font.w;
    index = 0;
    maxline = 0;
    linecount = 0;
    for (;;) {
	while (isspace(text[index]))
	    ++index;
	if (!text[index])
	    break;
	n = strlen(text + index);
	if (n > width) {
	    n = width;
	    while (!isspace(text[index + n]) && n >= 0)
		--n;
	    if (n < 0)
		n = width;
	}
	if (n > maxline)
	    maxline = n;
	++linecount;
	index += n;
    }
    *w = maxline * sdlg.font.w;
    *h = linecount * sdlg.font.h;
}

/*
 * Exported text-drawing functions.
 */

/* Draw a line of text.
 */
static void _puttext(SDL_Rect *rect, char const *text, int len, int flags)
{
    if (!sdlg.font.w)
	die("No font!");

    if (len < 0)
	len = strlen(text);
    if (flags & PT_CALCSIZE) {
	rect->w = len * sdlg.font.w;
	rect->h = sdlg.font.h;
	return;
    }

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);
    drawtext(rect, text, len, flags);
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
}

/* Draw multiple lines of text, breaking lines at whitespace.
 */
static void _putmltext(SDL_Rect *rect, char const *text, int flags)
{
    SDL_Rect	area;
    int		index, width, n;

    if (!sdlg.font.w)
	die("No font!");

    if (flags & PT_CALCSIZE) {
	width = rect->w;
	measuremltext(text, &width, &n);
	rect->w = width;
	rect->h = n;
	return;
    }

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);

    if (text) {
	area = *rect;
	width = area.w / sdlg.font.w;
	index = 0;
	while (area.h >= sdlg.font.h) {
	    while (isspace(text[index]))
		++index;
	    if (!text[index])
		break;
	    n = strlen(text + index);
	    if (n > width) {
		n = width;
		while (!isspace(text[index + n]) && n >= 0)
		    --n;
		if (n < 0)
		    n = width;
	    }
	    drawtext(&area, text + index, n, flags | PT_UPDATERECT);
	    index += n;
	}
	if (flags & PT_UPDATERECT)
	    *rect = area;
    } else {
	fillrect(rect);
    }

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
}

/*
 * The scrollable list functions.
 */

/* Initialize the scrolling list's structure.
 */
static int _createscroll(scrollinfo *scroll, SDL_Rect const *area,
			 Uint32 highlightcolor, int itemcount,
			 char const **items)
{
    if (!sdlg.font.w || !sdlg.font.h)
	return FALSE;

    scroll->area = *area;
    scroll->highlight = highlightcolor;
    scroll->itemcount = itemcount;
    scroll->items = items;
    scroll->linecount = area->h / sdlg.font.h;
    scroll->maxlen = area->w / sdlg.font.w;
    scroll->topitem = 0;
    scroll->index = 0;
    return TRUE;
}

/* Draw the visible lines of the list, with the selected line in its
 * own color.
 */
static void redrawscroll(scrollinfo *scroll)
{
    SDL_Rect	area;
    Uint32	fontcolor;
    int		n;

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);

    area = scroll->area;
    fontcolor = sdlg.font.color;
    for (n = scroll->topitem ; n < scroll->itemcount ; ++n) {
	if (area.h < sdlg.font.h)
	    break;
	if (n == scroll->index)
	    sdlg.font.color = scroll->highlight;
	drawtext(&area, scroll->items[n], strlen(scroll->items[n]),
			PT_UPDATERECT);
	if (n == scroll->index)
	    sdlg.font.color = fontcolor;
    }

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);

    if (area.h)
	SDL_FillRect(sdlg.screen, &area, sdlg.font.bkgnd);
    SDL_UpdateRect(sdlg.screen, scroll->area.x, scroll->area.y,
				scroll->area.w, scroll->area.h);
}

/* Change the index of the selected line according to the value of
 * move, ensuring that the number is in range and forcing that line to
 * be visible.
 */
static int _scrollmove(scrollinfo *scroll, int move)
{
    int	n, r;

    n = scroll->index;
    switch (move) {
      case SCROLL_NOP:		return TRUE;
      case SCROLL_UP:		--n;					break;
      case SCROLL_DN:		++n;					break;
      case SCROLL_HALFPAGE_UP:	n -= (scroll->linecount + 1) / 2;	break;
      case SCROLL_HALFPAGE_DN:	n += (scroll->linecount + 1) / 2;	break;
      case SCROLL_PAGE_UP:	n -= scroll->linecount;			break;
      case SCROLL_PAGE_DN:	n += scroll->linecount;			break;
      case SCROLL_ALLTHEWAY_UP:	n = 0;					break;
      case SCROLL_ALLTHEWAY_DN:	n = scroll->itemcount - 1;		break;
      default:			n = move;				break;
    }

    if (n < 0) {
	scroll->index = 0;
	r = FALSE;
    } else if (n >= scroll->itemcount) {
	scroll->index = scroll->itemcount - 1;
	r = FALSE;
    } else {
	scroll->index = n;
	r = TRUE;
    }

    if (scroll->linecount < scroll->itemcount) {
	n = scroll->linecount / 2;
	if (scroll->index < n)
	    scroll->topitem = 0;
	else if (scroll->index >= scroll->itemcount - n)
	    scroll->topitem = scroll->itemcount - scroll->linecount;
	else
	    scroll->topitem = scroll->index - n;
    }

    redrawscroll(scroll);
    return r;
}

/*
 *
 */

int _sdltextinitialize(void)
{
    sdlg.puttextfunc = _puttext;
    sdlg.putmltextfunc = _putmltext;
    sdlg.createscrollfunc = _createscroll;
    sdlg.scrollmovefunc = _scrollmove;
    return TRUE;
}
