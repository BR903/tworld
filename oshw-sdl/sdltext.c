/* sdltext.c: Font-rendering functions for SDL.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../err.h"

/*
 */
static int makefontfromsurface(fontinfo *pf, SDL_Surface *surface)
{
    char		brk[256];
    unsigned char      *p;
    unsigned char      *dest;
    Uint8		foregnd, bkgnd;
    int			pitch, wsum;
    int			count, ch;
    int			x, y, x0, y0, w;

    if (surface->format->BytesPerPixel != 1)
	return FALSE;

    if (SDL_MUSTLOCK(surface))
	SDL_LockSurface(surface);

    pitch = surface->pitch;
    p = surface->pixels;
    foregnd = p[0];
    bkgnd = p[pitch];
    for (y = 1, p += pitch ; y < surface->h && *p == bkgnd ; ++y, p += pitch) ;
    pf->h = y - 1;

    wsum = 0;
    ch = 32;
    memset(pf->w, 0, sizeof pf->w);
    memset(brk, 0, sizeof brk);
    for (y = 0 ; y + pf->h < surface->h && ch < 256 ; y += pf->h + 1) {
	p = surface->pixels;
	p += y * pitch;
	x0 = 1;
	for (x = 1 ; x < surface->w ; ++x) {
	    if (p[x] == bkgnd)
		continue;
	    w = x - x0;
	    x0 = x + 1;
	    pf->w[ch] = w;
	    wsum += w;
	    ++ch;
	    if (ch == 127)
		ch = 160;
	}
	brk[ch] = 1;
    }

    count = ch;
    pf->memory = calloc(wsum, pf->h);

    x0 = 1;
    y0 = 1;
    dest = pf->memory;
    for (ch = 0 ; ch < 256 ; ++ch) {
	pf->bits[ch] = dest;
	if (pf->w[ch] == 0)
	    continue;
	if (brk[ch]) {
	    x0 = 1;
	    y0 += pf->h + 1;
	}
	p = surface->pixels;
	p += y0 * pitch + x0;
	for (y = 0 ; y < pf->h ; ++y, p += pitch)
	    for (x = 0 ; x < pf->w[ch] ; ++x, ++dest)
		*dest = p[x] == bkgnd ? 0 : p[x] == foregnd ? 2 : 1;
	x0 += pf->w[ch] + 1;
    }

    if (SDL_MUSTLOCK(surface))
	SDL_UnlockSurface(surface);

    return TRUE;
}

/*
 *
 */

/*
 */
int _measurecolumns(void const *_texts, SDL_Rect *rects, int count)
{
    unsigned char const* const* texts = _texts;
    unsigned char const	       *p;
    int				maxcol;
    int				w, x, y;

    maxcol = 0;
    rects[0].w = 0;
    for (y = 0 ; y < count ; ++y) {
	x = 0;
	w = 0;
	p = texts[y];
	for (;;) {
	    if (*p == '\0' || *p == '\t') {
		if (w > rects[x].w)
		    rects[x].w = w;
		if (*p == '\0')
		    break;
		++x;
		w = 0;
		if (x > maxcol) {
		    rects[x].w = 0;
		    maxcol = x;
		}
	    }
	    w += sdlg.font.w[*p];
	    ++p;
	}
    }
    return maxcol + 1;
}

/*
 */
static int measuremltext(unsigned char const *text, int len, int maxwidth)
{
    int	brk, w, h, n;

    if (len < 0)
	len = strlen(text);
    h = 0;
    brk = 0;
    for (n = 0, w = 0 ; n < len ; ++n) {
	w += sdlg.font.w[text[n]];
	if (isspace(text[n])) {
	    brk = w;
	} else if (w > maxwidth) {
	    h += sdlg.font.h;
	    if (brk) {
		w -= brk;
		brk = 0;
	    } else {
		w = sdlg.font.w[text[n]];
		brk = 0;
	    }
	}
    }
    if (w)
	h += sdlg.font.h;
    return h;
}

/*
 *
 */

static void drawtextscanline8(Uint8 *scanline, int w, int y, Uint32 colors[],
			      unsigned char const *text, int len)
{
    unsigned char const	       *glyph;
    int				n, x;

    for (n = 0 ; n < len ; ++n) {
	glyph = sdlg.font.bits[text[n]];
	glyph += y * sdlg.font.w[text[n]];
	for (x = 0 ; w && x < sdlg.font.w[text[n]] ; ++x, --w)
	    scanline[x] = (Uint8)colors[glyph[x]];
	scanline += x;
    }
    if (w)
	memset(scanline, colors[0], w);
}

static void drawtextscanline16(Uint16 *scanline, int w, int y,
			       Uint32 colors[],
			       unsigned char const *text, int len)
{
    unsigned char const	       *glyph;
    int				n, x;

    for (n = 0 ; n < len ; ++n) {
	glyph = sdlg.font.bits[text[n]];
	glyph += y * sdlg.font.w[text[n]];
	for (x = 0 ; x < sdlg.font.w[text[n]] ; ++x)
	    scanline[x] = (Uint16)colors[glyph[x]];
	scanline += x;
    }
    while (w--)
	scanline[w] = colors[0];
}

static void drawtextscanline24(Uint8 *scanline, int w, int y,
			       Uint32 colors[],
			       unsigned char const *text, int len)
{
    unsigned char const	       *glyph;
    Uint32			c;
    int				n, x;

    for (n = 0 ; n < len ; ++n) {
	glyph = sdlg.font.bits[text[n]];
	glyph += y * sdlg.font.w[text[n]];
	for (x = 0 ; w && x < sdlg.font.w[text[n]] ; ++x, --w) {
	    c = colors[glyph[x]];
	    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		*scanline++ = (Uint8)(c >> 16);
		*scanline++ = (Uint8)(c >> 8);
		*scanline++ = (Uint8)c;
	    } else {
		*scanline++ = (Uint8)c;
		*scanline++ = (Uint8)(c >> 8);
		*scanline++ = (Uint8)(c >> 16);
	    }
	}
    }
    c = colors[0];
    while (w--) {
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	    *scanline++ = (Uint8)(c >> 16);
	    *scanline++ = (Uint8)(c >> 8);
	    *scanline++ = (Uint8)c;
	} else {
	    *scanline++ = (Uint8)c;
	    *scanline++ = (Uint8)(c >> 8);
	    *scanline++ = (Uint8)(c >> 16);
	}
    }
}

static void *drawtextscanline32(Uint32 *scanline, int w, int y,
				Uint32 colors[],
				unsigned char const *text, int len)
{
    unsigned char const	       *glyph;
    int				n, x;

    for (n = 0 ; n < len ; ++n) {
	glyph = sdlg.font.bits[text[n]];
	glyph += y * sdlg.font.w[text[n]];
	for (x = 0 ; w && x < sdlg.font.w[text[n]] ; ++x, --w)
	    scanline[x] = colors[glyph[x]];
	scanline += x;
    }
    while (w--)
	*scanline++ = colors[0];
    return scanline;
}

/* The routine to draw a line of text to the screen.
 */
static void drawtext(SDL_Rect *rect, unsigned char const *text,
		     int len, int flags)
{
    Uint32	colors[3];
    void       *p;
    void       *q;
    int		l, r;
    int		pitch, bpp, n, w, y;

    if (len < 0)
	len = text ? strlen(text) : 0;

    w = 0;
    for (n = 0 ; n < len ; ++n)
	w += sdlg.font.w[text[n]];
    if (flags & PT_CALCSIZE) {
	rect->h = sdlg.font.h;
	rect->w = w;
	return;
    }
    if (w >= rect->w) {
	w = rect->w;
	l = r = 0;
    } else if (flags & PT_RIGHT) {
	l = rect->w - w;
	r = 0;
    } else if (flags & PT_CENTER) {
	l = (rect->w - w) / 2;
	r = (rect->w - w) - l;
    } else {
	l = 0;
	r = rect->w - w;
    }

    colors[0] = sdlg.bkgndcolor;
    colors[1] = sdlg.halfcolor;
    colors[2] = sdlg.textcolor;

    pitch = sdlg.screen->pitch;
    bpp = sdlg.screen->format->BytesPerPixel;
    p = (unsigned char*)sdlg.screen->pixels + rect->y * pitch + rect->x * bpp;
    for (y = 0 ; y < sdlg.font.h && y < rect->h ; ++y) {
	switch (bpp) {
	  case 1: drawtextscanline8(p, w, y, colors, text, len);  break;
	  case 2: drawtextscanline16(p, w, y, colors, text, len); break;
	  case 3: drawtextscanline24(p, w, y, colors, text, len); break;
	  case 4:
	    q = drawtextscanline32(p, l, y, colors, "", 0);
	    q = drawtextscanline32(q, w, y, colors, text, len);
	    q = drawtextscanline32(q, r, y, colors, "", 0);
	    break;
	}
	p = (unsigned char*)p + pitch;
    }
    if (flags & PT_UPDATERECT) {
	rect->y += y;
	rect->h -= y;
    }
}

/*
 */
static void drawmultilinetext(SDL_Rect *rect, unsigned char const *text,
			      int len, int flags)
{
    SDL_Rect	area;
    int		index, brkw, brkn;
    int		w, n;

    if (flags & PT_CALCSIZE) {
	rect->h = measuremltext(text, len, rect->w);
	return;
    }

    if (len < 0)
	len = strlen(text);

    area = *rect;
    brkw = 0;
    index = 0;
    for (n = 0, w = 0 ; n < len ; ++n) {
	w += sdlg.font.w[text[n]];
	if (isspace(text[n])) {
	    brkn = n;
	    brkw = w;
	} else if (w > rect->w) {
	    if (brkw) {
		drawtext(&area, text + index, brkn - index,
				 flags | PT_UPDATERECT);
		index = brkn + 1;
		w -= brkw;
	    } else {
		drawtext(&area, text + index, n - index,
				 flags | PT_UPDATERECT);
		w = sdlg.font.w[text[n]];
	    }
	    brkw = 0;
	}
    }
    if (w)
	drawtext(&area, text + index, len - index, flags | PT_UPDATERECT);
    if (flags & PT_UPDATERECT) {
	*rect = area;
    } else {
	while (area.h)
	    drawtext(&area, "", 0, PT_UPDATERECT);
    }
}

/*
 * Exported text-drawing function.
 */

/* Free a font's memory.
 */
void freefont(void)
{
    if (sdlg.font.h) {
	free(sdlg.font.memory);
	sdlg.font.memory = NULL;
	sdlg.font.h = 0;
    }
}

/* Load a proportional font.
 */
int loadfontfromfile(char const *filename)
{
    SDL_Surface	       *bmp;
    fontinfo		font;

    bmp = SDL_LoadBMP(filename);
    if (!bmp)
	die("%s: can't load font bitmap: %s", filename, SDL_GetError());
    if (!makefontfromsurface(&font, bmp))
	die("failed to make font");
    SDL_FreeSurface(bmp);
    freefont();
    sdlg.font = font;
    return TRUE;
}

/* Draw a line of text.
 */
void _puttext(SDL_Rect *rect, char const *text, int len, int flags)
{
    if (!sdlg.font.h)
	die("No font!");

    if (len < 0)
	len = text ? strlen(text) : 0;

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);

    if (flags & PT_MULTILINE)
	drawmultilinetext(rect, (unsigned char const*)text, len, flags);
    else
	drawtext(rect, (unsigned char const*)text, len, flags);

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
}

int _sdltextinitialize(void)
{
    sdlg.font.h = 0;
    sdlg.puttextfunc = _puttext;
    sdlg.measurecolumnsfunc = _measurecolumns;
    return TRUE;
}

/*
 *
 */
