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


static void *fillscanline(void *scanline, int bpp, int count, Uint32 color)
{
    Uint8      *p = scanline;

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

/* Draw a line of text.
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

    n = len * sdlg.font->w;
    if (n > rect->w) {
	len = rect->w / sdlg.font->w;
	n = len * sdlg.font->w;
    }
    indent = flags & PT_RIGHT ? rect->w - n
	   : flags & PT_CENTER ? (rect->w - n) / 2 : 0;

    pitch = sdlg.screen->pitch;
    bpp = sdlg.screen->format->BytesPerPixel;
    scanline = sdlg.screen->pixels + rect->y * pitch + rect->x * bpp;

    for (y = 0 ; y < sdlg.font->h && y < rect->h ; ++y) {
	p = fillscanline(scanline, bpp, indent, sdlg.font->bkgnd);
	for (n = 0 ; n < len ; ++n) {
	    glyph = sdlg.font->bits;
	    glyph += *(unsigned char const*)(text + n) * sdlg.font->h;
	    for (x = sdlg.font->w, bit = 128 ; x ; --x, bit >>= 1) {
		c = glyph[y] & bit ? sdlg.font->color : sdlg.font->bkgnd;
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
	x = rect->w - len * sdlg.font->w;
	if (x > 0)
	    fillscanline(p, bpp, x, sdlg.font->bkgnd);
	scanline += pitch;
    }
    if (flags & PT_UPDATERECT) {
	rect->y += y;
	rect->h -= y;
    }
}

/*
 * Exported font-drawing functions.
 */

/* Draw a line of text.
 */
static void putntext(SDL_Rect *rect, char const *text, int len, int flags)
{
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);
    drawtext(rect, text, len, flags);
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
}

/* Draw a line of NUL-terminated text.
 */
static void puttext(SDL_Rect *rect, char const *text, int flags)
{
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);
    drawtext(rect, text, strlen(text), flags);
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
}

/* Draw multiple lines of text, looking for whitespace characters to
 * break lines at and subtracting each line from area as it gets used.
 */
static void putmltext(SDL_Rect *area, char const *text, int flags)
{
    fontinfo const     *font = sdlg.font;
    int			index, width, n;

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);

    width = area->w / font->w;
    index = 0;
    while (area->h >= font->h) {
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
	drawtext(area, text + index, n, flags | PT_UPDATERECT);
	index += n;
    }

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
}

/*
 * The scrollable list functions.
 */

/* Draw the visible lines of the list, with the selected line in its
 * own color.
 */
static void scrollredraw(scrollinfo *scroll)
{
    SDL_Rect	area;
    Uint32	fontcolor;
    int		n;

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);

    area = scroll->area;
    fontcolor = sdlg.font->color;
    for (n = scroll->topitem ; n < scroll->itemcount ; ++n) {
	if (area.h < sdlg.font->h)
	    break;
	if (n == scroll->index)
	    sdlg.font->color = scroll->highlight;
	drawtext(&area, scroll->items[n], strlen(scroll->items[n]),
			PT_UPDATERECT);
	if (n == scroll->index)
	    sdlg.font->color = fontcolor;
    }
    if (area.h)
	drawtext(&area, "", 0, 0);

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
}

/* Return the index of the selected line.
 */
static int scrollindex(scrollinfo const *scroll)
{
    return scroll->index;
}

/* Change the index of the selected line to pos, ensuring that the
 * number is in range and forcing that line to be visible.
 */
static int scrollsetindex(scrollinfo *scroll, int pos)
{
    int	half;

    scroll->index = pos < 0 || pos >= scroll->itemcount ?
					scroll->itemcount - 1 : pos;

    if (scroll->linecount >= scroll->itemcount)
	return TRUE;

    half = scroll->linecount / 2;
    if (scroll->index < half)
	scroll->topitem = 0;
    else if (scroll->index >= scroll->itemcount - half)
	scroll->topitem = scroll->itemcount - scroll->linecount;
    else
	scroll->topitem = scroll->index - half;
    return TRUE;
}

/* Change the selected line relative to the current selection.
 */
static int scrollmove(scrollinfo *scroll, int delta)
{
    int	n;
    int	r = TRUE;

    n = scroll->index;
    switch (delta) {
      case ScrollUp:		--n;					break;
      case ScrollDn:		++n;					break;
      case ScrollHalfPageUp:	n -= (scroll->linecount + 1) / 2;	break;
      case ScrollHalfPageDn:	n += (scroll->linecount + 1) / 2;	break;
      case ScrollPageUp:	n -= scroll->linecount;			break;
      case ScrollPageDn:	n += scroll->linecount;			break;
      case ScrollToTop:		n = 0;					break;
      case ScrollToBot:		n = scroll->itemcount - 1;		break;
    }
    if (n < 0) {
	n = 0;
	r = FALSE;
    } else if (n >= scroll->itemcount) {
	n = scroll->itemcount - 1;
	r = FALSE;
    }
    scrollsetindex(scroll, n);
    return r;
}

/* Initialize the scrolling list's structure.
 */
static int createscroll(scrollinfo *scroll, SDL_Rect const *area,
			Uint32 highlightcolor, int itemcount,
			char const **items)
{
    fontinfo const     *font = sdlg.font;

    scroll->area = *area;
    scroll->highlight = highlightcolor;
    scroll->itemcount = itemcount;
    scroll->items = items;
    scroll->linecount = area->h / font->h;
    scroll->maxlen = area->w / font->w;
    scroll->topitem = 0;
    scroll->index = 0;
    return TRUE;
}

/*
 *
 */

int _sdltextinitialize(void)
{
    sdlg.puttext = puttext;
    sdlg.putntext = putntext;
    sdlg.putmltext = putmltext;
    sdlg.createscroll = createscroll;
    sdlg.scrollredraw = scrollredraw;
    sdlg.scrollindex = scrollindex;
    sdlg.scrollsetindex = scrollsetindex;
    sdlg.scrollmove = scrollmove;
    return TRUE;
}
