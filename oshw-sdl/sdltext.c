/* sdltext.c: Font-drawing functions for SDL.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<string.h>
#include	<ctype.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"sdltext.h"
#include	"../err.h"

/* The surface to draw text on.
 */
static SDL_Surface     *surface = NULL;

/* The font to draw text with.
 */
static fontinfo const  *font = NULL;

/* A pointer to one of the following four functions.
 */
static void (*putline)(int, int, int, char const*) = NULL;

/*
 * The functions that do all the drawing. It is assumed that surface
 * has been locked before calling these functions.
 */

static void putline8(int xpos, int ypos, int len, char const *text)
{
    unsigned char const	       *glyph;
    Uint8		       *top;
    Uint8		       *p;
    unsigned char		ch;
    unsigned char		b;
    int				n, x, y;

    top = (Uint8*)((unsigned char*)surface->pixels + ypos * surface->pitch);
    top += xpos;
    for (n = 0 ; n < len ; ++n) {
	ch = *(unsigned char const*)(text + n);
	glyph = font->bits + ch * font->h;
	for (y = 0, p = top ; y < font->h ; ++y, p += surface->pitch)
	    for (x = 0, b = 128 ; b ; ++x, b >>= 1)
		p[x] = glyph[y] & b ? font->color : font->bkgnd;
	top += font->w;
    }
}

static void putline16(int xpos, int ypos, int len, char const *text)
{
    unsigned char const	       *glyph;
    Uint16		       *top;
    Uint16		       *p;
    unsigned char		ch;
    unsigned char		b;
    int				n, x, y;

    top = (Uint16*)((unsigned char*)surface->pixels + ypos * surface->pitch);
    top += xpos;
    for (n = 0 ; n < len ; ++n) {
	ch = *(unsigned char const*)(text + n);
	glyph = font->bits + ch * font->h;
	p = top;
	for (y = 0 ; y < font->h ; ++y) {
	    for (x = 0, b = 128 ; b ; ++x, b >>= 1)
		p[x] = glyph[y] & b ? font->color : font->bkgnd;
	    p = (Uint16*)((unsigned char*)p + surface->pitch);
	}
	top += font->w;
    }
}

static void putline24(int xpos, int ypos, int len, char const *text)
{
    unsigned char const	       *glyph;
    unsigned char	       *top;
    unsigned char	       *p;
    unsigned char		ch;
    unsigned char		b;
    int				n, x, y;
    Uint32			c;

    top = (unsigned char*)surface->pixels + ypos * surface->pitch;
    top += xpos * 3;
    for (n = 0 ; n < len ; ++n) {
	ch = *(unsigned char const*)(text + n);
	glyph = font->bits + ch * font->h;
	for (y = 0, p = top ; y < font->h ; ++y, p += surface->pitch) {
	    for (x = 0, b = 128 ; b ; x += 3, b >>= 1) {
		c = glyph[y] & b ? font->color : font->bkgnd;
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		    p[x + 0] = (c >> 16) & 0xFF;
		    p[x + 1] = (c >> 8) & 0xFF;
		    p[x + 2] = c & 0xFF;
		} else {
		    p[x + 0] = c & 0xFF;
		    p[x + 1] = (c >> 8) & 0xFF;
		    p[x + 2] = (c >> 16) & 0xFF;
		}
	    }
	}
	top += font->w * 3;
    }
}

static void putline32(int xpos, int ypos, int len, char const *text)
{
    unsigned char const	       *glyph;
    Uint32		       *top;
    Uint32		       *p;
    unsigned char		ch;
    unsigned char		b;
    int				n, x, y;

    top = (Uint32*)((unsigned char*)surface->pixels + ypos * surface->pitch);
    top += xpos;
    for (n = 0 ; n < len ; ++n) {
	ch = *(unsigned char const*)(text + n);
	glyph = font->bits + ch * font->h;
	p = top;
	for (y = 0 ; y < font->h ; ++y) {
	    for (x = 0, b = 128 ; b ; ++x, b >>= 1)
		p[x] = glyph[y] & b ? font->color : font->bkgnd;
	    p = (Uint32*)((unsigned char*)p + surface->pitch);
	}
	top += font->w;
    }
}

/*
 * Exported font-drawing functions.
 */

/* Specify the surface to draw on and the font to draw with.
 */
void _sdlsettextfont(fontinfo const *f) { font = f; }

void _sdlsettextsurface(SDL_Surface *s)
{
    surface = s;
    switch (surface->format->BytesPerPixel) {
      case 1:	putline = putline8;	break;
      case 2:	putline = putline16;	break;
      case 3:	putline = putline24;	break;
      case 4:	putline = putline32;	break;
    }
}

/* Draw a line of NUL-terminated text.
 */
void _sdlputtext(int xpos, int ypos, char const *text)
{
    if (SDL_MUSTLOCK(surface))
	SDL_LockSurface(surface);
    putline(xpos, ypos, strlen(text), text);
    if (SDL_MUSTLOCK(surface))
	SDL_UnlockSurface(surface);
}

/* Draw a line of len characters.
 */
void _sdlputntext(int xpos, int ypos, int len, char const *text)
{
    if (SDL_MUSTLOCK(surface))
	SDL_LockSurface(surface);
    putline(xpos, ypos, len, text);
    if (SDL_MUSTLOCK(surface))
	SDL_UnlockSurface(surface);
}

/* Draw multiple lines of text, looking for whitespace characters to
 * break lines at and subtracting each line from area as it gets used.
 */
void _sdlputmltext(SDL_Rect *area, char const *text)
{
    int	index, width, n;

    if (SDL_MUSTLOCK(surface))
	SDL_LockSurface(surface);

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
	putline(area->x, area->y, n, text + index);
	index += n;
	area->y += font->h;
	area->h -= font->h;
    }

    if (SDL_MUSTLOCK(surface))
	SDL_UnlockSurface(surface);
}

/*
 * The scrollable list functions.
 */

/* Draw the visible lines of the list, with the selected line in its
 * own color.
 */
void _sdlscrollredraw(scrollinfo *scroll)
{
    fontinfo		selfont;
    fontinfo const     *origfont;
    int			len, n, y;

    SDL_FillRect(surface, &scroll->area, font->bkgnd);
    if (SDL_MUSTLOCK(surface))
	SDL_LockSurface(surface);

    y = scroll->area.y;
    origfont = font;
    selfont = *font;
    selfont.color = scroll->highlight;
    for (n = scroll->topitem ; n < scroll->itemcount ; ++n) {
	if (scroll->area.y + scroll->area.h - y < font->h)
	    break;
	if (n == scroll->index)
	    font = &selfont;
	len = strlen(scroll->items[n]);
	if (len > scroll->maxlen)
	    putline(scroll->area.x, y, scroll->maxlen, scroll->items[n]);
	else
	    putline(scroll->area.x, y, len, scroll->items[n]);
	if (n == scroll->index)
	    font = origfont;
	y += font->h;
    }

    if (SDL_MUSTLOCK(surface))
	SDL_UnlockSurface(surface);
}

/* Return the index of the selected line.
 */
int _sdlscrollindex(scrollinfo const *scroll)
{
    return scroll->index;
}

/* Change the index of the selected line to pos, ensuring that the
 * number is in range and forcing that line to be visible.
 */
int _sdlscrollsetindex(scrollinfo *scroll, int pos)
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
int _sdlscrollmove(scrollinfo *scroll, int delta)
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
    _sdlscrollsetindex(scroll, n);
    return r;
}

/* Initialize the scrolling list's structure.
 */
int _sdlcreatescroll(scrollinfo *scroll, SDL_Rect const *area,
		     Uint32 highlightcolor, int itemcount, char const **items)
{
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
