/* sdltiles.c: Functions for rendering tile images.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../err.h"
#include	"../state.h"

/* The total number of tile images.
 */
#define	NTILES		128

/* Flags that indicate the size and shape of an oversized
 * (transparent) tile image.
 */
#define	SIZE_EXTLEFT	0x01	/* image extended leftwards by one tile */
#define	SIZE_EXTRIGHT	0x02	/* image extended rightwards by one tile */
#define	SIZE_EXTUP	0x04	/* image extended upwards by one tile */
#define	SIZE_EXTDOWN	0x08	/* image extended downards by one tile */
#define	SIZE_EXTALL	0x0F	/* image is 3x3 tiles in size */

/* Structure providing pointers to the various tile images available
 * for a given id.
 */
typedef	struct tilemap {
    Uint32     *opaque[16];	/* ptr to an opaque image */
    Uint32     *transp[16];	/* ptr to one or more transparent images */
    char	celcount;	/* count of animated transparent images */
    char	transpsize;	/* SIZE_* flags for the transparent size */
} tilemap;

enum {
    TILEIMG_IMPLICIT = 0,
    TILEIMG_SINGLEOPAQUE,
    TILEIMG_OPAQUECELS,
    TILEIMG_TRANSPCELS,
    TILEIMG_CREATURE,
    TILEIMG_ANIMATION
};

/* Structure indicating where to find the various tile images in a
 * tile bitmap. The last field indicates what is present in a
 * free-form tile bitmap, and the ordering of the array supplies the
 * order of images. All other fields apply to a fixed-form tile
 * bitmap, and can be in any order.
 */
typedef	struct tileidinfo {
    int		id;		/* the tile ID */
    signed char	xopaque;	/* the coordinates of the opaque image */
    signed char	yopaque;	/*   (expressed in tiles, not pixels) */
    signed char	xtransp;	/* coordinates of the transparent image */
    signed char	ytransp;	/*   (or the first image if animated) */
    signed char	xceloff;	/* offset to the next transparent image */
    signed char	yceloff;	/*   if image is animated */
    signed char	celcount;	/* count of animated transparent images */
    char	transpsize;	/* SIZE_* flags for the transparent size */
    int		shape;		/* enum values for the free-form bitmap */
} tileidinfo;

static tileidinfo const tileidmap[NTILES] = {
    { Empty,		 0,  0, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Slide_North,	 1,  2, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Slide_West,	 1,  4, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Slide_South,	 0, 13, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Slide_East,	 1,  3, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Slide_Random,	 3,  2, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Ice,		 0, 12, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { IceWall_Northwest, 1, 12, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { IceWall_Northeast, 1, 13, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { IceWall_Southwest, 1, 11, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { IceWall_Southeast, 1, 10, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Gravel,		 2, 13, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Dirt,		 0, 11, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Water,		 0,  3, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Fire,		 0,  4, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Bomb,		 2, 10, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Beartrap,		 2, 11, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Burglar,		 2,  1, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { HintButton,	 2, 15, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Button_Blue,	 2,  8, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Button_Green,	 2,  3, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Button_Red,	 2,  4, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Button_Brown,	 2,  7, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Teleport,		 2,  9, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Wall,		 0,  1, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Wall_North,	 0,  6, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Wall_West,	 0,  7, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Wall_South,	 0,  8, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Wall_East,	 0,  9, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Wall_Southeast,	 3,  0, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { HiddenWall_Perm,	 0,  5, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { HiddenWall_Temp,	 2, 12, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { BlueWall_Real,	 1, 14, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { BlueWall_Fake,	 1, 15, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { SwitchWall_Open,	 2,  6, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { SwitchWall_Closed, 2,  5, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { PopupWall,	 2, 14, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { CloneMachine,	 3,  1, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Door_Red,		 1,  7, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Door_Blue,	 1,  6, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Door_Yellow,	 1,  9, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Door_Green,	 1,  8, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Socket,		 2,  2, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Exit,		 1,  5, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { ICChip,		 0,  2, -1, -1, 0, 0, 0, 0, TILEIMG_OPAQUECELS },
    { Key_Red,		 6,  5,  9,  5, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Key_Blue,		 6,  4,  9,  4, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Key_Yellow,	 6,  7,  9,  7, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Key_Green,	 6,  6,  9,  6, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Boots_Ice,	 6, 10,  9, 10, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Boots_Slide,	 6, 11,  9, 11, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Boots_Fire,	 6,  9,  9,  9, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Boots_Water,	 6,  8,  9,  8, 0, 0, 1, 0, TILEIMG_TRANSPCELS },
    { Block_Static,	 0, 10, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { Overlay_Buffer,	 2,  0, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { Exit_Extra_1,	 3, 10, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Exit_Extra_2,	 3, 11, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Burned_Chip,	 3,  4, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Bombed_Chip,	 3,  5, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Exited_Chip,	 3,  9, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Water_Splash,	 3,  3, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Swimming_Chip + 0, 3, 12, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Swimming_Chip + 1, 3, 13, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Swimming_Chip + 2, 3, 14, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Swimming_Chip + 3, 3, 15, -1, -1, 0, 0, 0, 0, TILEIMG_SINGLEOPAQUE },
    { Chip + 0,		 6, 12,  9, 12, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Chip + 1,		 6, 13,  9, 13, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Chip + 2,		 6, 14,  9, 14, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Chip + 3,		 6, 15,  9, 15, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Pushing_Chip + 0,	 6, 12,  9, 12, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Pushing_Chip + 1,	 6, 13,  9, 13, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Pushing_Chip + 2,	 6, 14,  9, 14, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Pushing_Chip + 3,	 6, 15,  9, 15, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Block + 0,	 0, 14, -1, -1, 0, 0, 0, 0, TILEIMG_CREATURE },
    { Block + 1,	 0, 15, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { Block + 2,	 1,  0, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { Block + 3,	 1,  1, -1, -1, 0, 0, 0, 0, TILEIMG_IMPLICIT },
    { Tank + 0,		 4, 12,  7, 12, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Tank + 1,		 4, 13,  7, 13, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Tank + 2,		 4, 14,  7, 14, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Tank + 3,		 4, 15,  7, 15, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Ball + 0,		 4,  8,  7,  8, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Ball + 1,		 4,  9,  7,  9, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Ball + 2,		 4, 10,  7, 10, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Ball + 3,		 4, 11,  7, 11, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Glider + 0,	 5,  0,  8,  0, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Glider + 1,	 5,  1,  8,  1, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Glider + 2,	 5,  2,  8,  2, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Glider + 3,	 5,  3,  8,  3, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Fireball + 0,	 4,  4,  7,  4, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Fireball + 1,	 4,  5,  7,  5, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Fireball + 2,	 4,  6,  7,  6, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Fireball + 3,	 4,  7,  7,  7, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Walker + 0,	 5,  8,  8,  8, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Walker + 1,	 5,  9,  8,  9, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Walker + 2,	 5, 10,  8, 10, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Walker + 3,	 5, 11,  8, 11, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Blob + 0,		 5, 12,  8, 12, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Blob + 1,		 5, 13,  8, 13, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Blob + 2,		 5, 14,  8, 14, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Blob + 3,		 5, 15,  8, 15, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Teeth + 0,	 5,  4,  8,  4, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Teeth + 1,	 5,  5,  8,  5, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Teeth + 2,	 5,  6,  8,  6, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Teeth + 3,	 5,  7,  8,  7, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Bug + 0,		 4,  0,  7,  0, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Bug + 1,		 4,  1,  7,  1, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Bug + 2,		 4,  2,  7,  2, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Bug + 3,		 4,  3,  7,  3, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Paramecium + 0,	 6,  0,  9,  0, 0, 0, 1, 0, TILEIMG_CREATURE },
    { Paramecium + 1,	 6,  1,  9,  1, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Paramecium + 2,	 6,  2,  9,  2, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Paramecium + 3,	 6,  3,  9,  3, 0, 0, 1, 0, TILEIMG_IMPLICIT },
    { Water_Splash,	 3,  3, -1, -1, 0, 0, 0, 0, TILEIMG_ANIMATION },
    { Bomb_Explosion,	 3,  6, -1, -1, 0, 0, 0, 0, TILEIMG_ANIMATION },
    { Entity_Explosion,	 3,  7, -1, -1, 0, 0, 0, 0, TILEIMG_ANIMATION }
};

static Uint32	       *cctiles = NULL;
static tilemap		tileptr[NTILES];

static int settilesize(int w, int h)
{
    if (w % 4 || h % 4) {
	warn("tile dimensions must be divisible by four");
	return FALSE;
    }
    sdlg.wtile = w;
    sdlg.htile = h;
    sdlg.cptile = w * h;
    sdlg.cbtile = sdlg.cptile * sizeof(Uint32);
    return TRUE;
}

/*
 * Functions for obtaining tile images.
 */

static void addtransparenttile(Uint32 *dest, int id, int n)
{
    Uint32     *src;
    int		w, x, y;

    src = tileptr[id].transp[n];
    w = sdlg.wtile;
    if (tileptr[id].transpsize & SIZE_EXTRIGHT)
	w += sdlg.wtile;
    if (tileptr[id].transpsize & SIZE_EXTLEFT) {
	src += sdlg.wtile;
	w += sdlg.wtile;
    }
    if (tileptr[id].transpsize & SIZE_EXTUP)
	src += sdlg.htile * w;
    for (y = sdlg.htile ; y ; --y, src += w, dest += sdlg.wtile)
	for (x = 0 ; x < sdlg.wtile ; ++x)
	    if (src[x] != sdlg.transpixel)
		dest[x] = src[x];
}

#if 0
/* Return a pointer to a specific tile image.
 */
static Uint32 const *_gettileimage(int id, int transp)
{
    if (transp)
	return tileptr[id].transp[0] ? tileptr[id].transp[0]
				     : tileptr[id].opaque[0];
    else
	return tileptr[id].opaque ? tileptr[id].opaque[0]
				  : tileptr[id].transp[0];
}
#endif

/* Return a pointer to a tile image for a creature, completing the
 * fields of the given rect.
 */
static Uint32 const *_getcreatureimage(SDL_Rect *rect,
				       int id, int dir, int moving, int frame)
{
    tilemap const      *q;
    int			n;

    rect->w = sdlg.wtile;
    rect->h = sdlg.htile;
    q = tileptr + id;
    if (!isanimation(id))
	q += diridx(dir);

    if (!q->transpsize || isanimation(id)) {
	if (moving > 0) {
	    switch (dir) {
	      case NORTH:	rect->y += moving * rect->h / 8;	break;
	      case WEST:	rect->x += moving * rect->w / 8;	break;
	      case SOUTH:	rect->y -= moving * rect->h / 8;	break;
	      case EAST:	rect->x -= moving * rect->w / 8;	break;
	    }
	}
    }
    if (q->transpsize) {
	if (q->transpsize & SIZE_EXTLEFT) {
	    rect->x -= sdlg.wtile;
	    rect->w += sdlg.wtile;
	}
	if (q->transpsize & SIZE_EXTRIGHT)
	    rect->w += sdlg.wtile;
	if (q->transpsize & SIZE_EXTUP) {
	    rect->y -= sdlg.htile;
	    rect->h += sdlg.htile;
	}
	if (q->transpsize & SIZE_EXTDOWN)
	    rect->h += sdlg.htile;
    }
    n = q->celcount > 1 ? frame : 0;
    if (n >= q->celcount)
	die("requested cel #%d from a %d-cel sequence (%d+%d)",
	    n, q->celcount, id, diridx(dir));
    return q->transp[n] ? q->transp[n] : q->opaque[n];
}

/* Return a pointer to a tile image for a cell. If the top image is
 * transparent, the appropriate image is created in the overlay
 * buffer.
 */
static Uint32 const *_getcellimage(int top, int bot, int timerval)
{
    static Uint32      *opaquetile = NULL;
    Uint32	       *dest;
    int			nt, nb;

    if (!tileptr[top].celcount)
	die("map element %02X has no suitable image", top);
    nt = (timerval + 1) % tileptr[top].celcount;
    if (bot == Nothing || bot == Empty || !tileptr[top].transp[0]) {
	if (tileptr[top].opaque[nt])
	    return tileptr[top].opaque[nt];
	if (!opaquetile)
	    xalloc(opaquetile, sdlg.cbtile);
	memcpy(opaquetile, tileptr[Empty].opaque[0], sdlg.cbtile);
	addtransparenttile(opaquetile, top, nt);
	return opaquetile;
    }

    if (!tileptr[bot].celcount)
	die("map element %02X has no suitable image", bot);
    nb = (timerval + 1) % tileptr[bot].celcount;
    dest = tileptr[Overlay_Buffer].opaque[0];
    if (tileptr[bot].opaque[nb])
	memcpy(dest, tileptr[bot].opaque[nb], sdlg.cbtile);
    else {
	memcpy(dest, tileptr[Empty].opaque[0], sdlg.cbtile);
	addtransparenttile(dest, bot, nb);
    }
    addtransparenttile(dest, top, nt);
    return dest;
}

/*
 *
 */

/* Translate the given surface to one with the same color layout as
 * the display surface.
 */
static SDL_Surface *loadtilesto32(char const *filename)
{
    SDL_PixelFormat    *fmt;
    SDL_Surface	       *bmp;
    SDL_Surface	       *tiles;
    SDL_Rect		rect;

    bmp = SDL_LoadBMP(filename);
    if (!bmp)
	return NULL;

    rect.x = 0;
    rect.y = 0;
    rect.w = bmp->w;
    rect.h = bmp->h;

    if (!sdlg.screen) {
	warn("copytilesto32() called before creating 32-bit surface");
	fmt = SDL_GetVideoInfo()->vfmt;
    } else
	fmt = sdlg.screen->format;

    tiles = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 32,
				 fmt->Rmask, fmt->Gmask,
				 fmt->Bmask, fmt->Amask);
    if (!tiles) {
	SDL_FreeSurface(bmp);
	return NULL;
    }

    if (SDL_BlitSurface(bmp, &rect, tiles, &rect)) {
	SDL_FreeSurface(bmp);
	SDL_FreeSurface(tiles);
	return NULL;
    }
    
    SDL_FreeSurface(bmp);
    return tiles;
}

/* Extract the mask section of the given image to an 8-bit surface.
 */
static SDL_Surface *extractmask(SDL_Surface *src,
				int xmask, int ymask, int wmask, int hmask)
{
    SDL_Surface	       *mask;
    SDL_Color		pal[2];
    SDL_Rect		srcrect, destrect;

    mask = SDL_CreateRGBSurface(SDL_SWSURFACE, wmask, hmask, 8, 0, 0, 0, 0);
    if (!mask)
	return NULL;
    pal[0].r = pal[0].g = pal[0].b = 0;
    pal[1].r = pal[1].g = pal[1].b = 255;
    SDL_SetPalette(mask, SDL_LOGPAL, pal, 0, 2);

    srcrect.x = xmask;
    srcrect.y = ymask;
    srcrect.w = wmask;
    srcrect.h = hmask;
    destrect.x = 0;
    destrect.y = 0;
    SDL_BlitSurface(src, &srcrect, mask, &destrect);

    return mask;
}

/*
 *
 */

/* Individually transfer the tiles to a one-dimensional array. The
 * magenta pixels in the "clear" area are made transparent.
 */
static int initsmalltileset(SDL_Surface *tiles, int wset, int hset,
			    int xclear, int yclear, int wclear, int hclear)
{
    Uint32	       *src;
    Uint32	       *dest;
    Uint32		magenta;
    int			id, x, y, n;

    if (SDL_MUSTLOCK(tiles))
	SDL_LockSurface(tiles);

    magenta = SDL_MapRGB(tiles->format, 255, 0, 255);
    dest = (Uint32*)((char*)tiles->pixels + yclear * tiles->pitch);
    dest += xclear * sdlg.wtile;
    for (y = 0 ; y < hclear * sdlg.htile ; ++y) {
	for (x = 0 ; x < wclear * sdlg.wtile ; ++x)
	    if (dest[x] == magenta)
		dest[x] = sdlg.transpixel;
	dest = (Uint32*)((char*)dest + tiles->pitch);
    }

    if (!(cctiles = calloc(wset * hset, sdlg.cbtile)))
	memerrexit();
    dest = cctiles;
    for (x = 0 ; x < wset * sdlg.wtile ; x += sdlg.wtile) {
	src = (Uint32*)tiles->pixels + x;
	for (y = hset * sdlg.htile ; y ; --y, dest += sdlg.wtile) {
	    memcpy(dest, src, sdlg.wtile * sizeof *dest);
	    src = (Uint32*)((char*)src + tiles->pitch);
	}
    }
    if (SDL_MUSTLOCK(tiles))
	SDL_UnlockSurface(tiles);

    for (n = 0 ; n < NTILES ; ++n) {
	id = tileidmap[n].id;
	if (tileidmap[n].xtransp >= 0) {
	    tileptr[id].celcount = 1;
	    tileptr[id].opaque[0] = NULL;
	    tileptr[id].transp[0] = cctiles + (tileidmap[n].xopaque * hset
					+ tileidmap[n].yopaque) * sdlg.cptile;
	    tileptr[id].transpsize = 0;
	} else if (tileidmap[n].xopaque >= 0) {
	    tileptr[id].celcount = 1;
	    tileptr[id].opaque[0] = cctiles + (tileidmap[n].xopaque * hset
					+ tileidmap[n].yopaque) * sdlg.cptile;
	    tileptr[id].transp[0] = NULL;
	    tileptr[id].transpsize = 0;
	} else {
	    tileptr[id].celcount = 0;
	    tileptr[id].opaque[0] = NULL;
	    tileptr[id].transp[0] = NULL;
	    tileptr[id].transpsize = 0;
	}
    }

    return TRUE;
}

/*
 *
 */

/* Individually transfer the tiles to a one-dimensional array. If mask
 * is NULL, then magenta pixels in the mask area are made transparent.
 */
static int initmaskedtileset(SDL_Surface *tiles, int wset, int hset,
			     SDL_Surface *maskimage, int wmask, int hmask,
			     int xmaskdest, int ymaskdest)
{
    Uint8      *mask;
    Uint32     *src;
    Uint32     *dest;
    int		id, x, y, n;

    if (SDL_MUSTLOCK(tiles))
	SDL_LockSurface(tiles);

    if (SDL_MUSTLOCK(maskimage))
	SDL_LockSurface(maskimage);
    mask = (Uint8*)maskimage->pixels;
    dest = (Uint32*)((char*)tiles->pixels + ymaskdest * tiles->pitch);
    dest += xmaskdest * sdlg.wtile;
    for (y = 0 ; y < hmask * sdlg.htile ; ++y) {
	for (x = 0 ; x < wmask * sdlg.wtile ; ++x)
	    if (!mask[x])
		dest[x] = (Uint32)sdlg.transpixel;
	mask = (Uint8*)((char*)mask + maskimage->pitch);
	dest = (Uint32*)((char*)dest + tiles->pitch);
    }
    if (SDL_MUSTLOCK(maskimage))
	SDL_UnlockSurface(maskimage);

    if (!(cctiles = calloc(wset * hset * sdlg.cptile, sizeof *cctiles)))
	memerrexit();
    dest = cctiles;
    for (x = 0 ; x < wset * sdlg.wtile ; x += sdlg.wtile) {
	for (y = 0 ; y < hset * sdlg.htile ; y += sdlg.htile) {
	    src = (Uint32*)((char*)tiles->pixels + y * tiles->pitch) + x;
	    for (n = sdlg.htile ; n ; --n, dest += sdlg.wtile) {
		memcpy(dest, src, sdlg.wtile * sizeof *dest);
		src = (Uint32*)((char*)src + tiles->pitch);
	    }
	}
    }
    if (SDL_MUSTLOCK(tiles))
	SDL_UnlockSurface(tiles);

    for (n = 0 ; n < NTILES ; ++n) {
	id = tileidmap[n].id;
	tileptr[id].opaque[0] = NULL;
	tileptr[id].transp[0] = NULL;
	tileptr[id].celcount = 0;
	tileptr[id].transpsize = 0;
	if (tileidmap[n].xopaque >= 0) {
	    tileptr[id].celcount = 1;
	    tileptr[id].opaque[0] = cctiles + (tileidmap[n].xopaque * hset
					+ tileidmap[n].yopaque) * sdlg.cptile;
	}
	if (tileidmap[n].xtransp >= 0) {
	    tileptr[id].celcount = 1;
	    tileptr[id].transp[0] = cctiles + (tileidmap[n].xtransp * hset
					+ tileidmap[n].ytransp) * sdlg.cptile;
	}
    }

    return TRUE;
}

/*
 *
 */

static void extractopaquetileseq(SDL_Surface *tiles, SDL_Rect const *rect,
				 int count, Uint32 **pdest, Uint32** ptrs,
				 Uint32 transpclr)
{
    Uint32     *tile;
    Uint32     *dest;
    Uint32     *p;
    int		n, x, y;

    dest = *pdest;
    tile = (Uint32*)((char*)tiles->pixels + rect->y * tiles->pitch) + rect->x;
    for (n = 0 ; n < count ; ++n) {
	ptrs[n] = dest;
	p = tile;
	memcpy(dest, tileptr[Empty].opaque[0], sdlg.cbtile);
	for (y = 0 ; y < sdlg.htile ; ++y) {
	    for (x = 0 ; x < sdlg.wtile ; ++x)
		if (p[x] != transpclr)
		    dest[x] = p[x];
	    p = (Uint32*)((char*)p + tiles->pitch);
	    dest += sdlg.wtile;
	}
	tile += rect->w;
    }
    *pdest = dest;
}

static void extracttransptileseq(SDL_Surface *tiles, SDL_Rect const *rect,
				 int count, Uint32 **pdest, Uint32** ptrs,
				 Uint32 transpclr)
{
    Uint32     *tile;
    Uint32     *dest;
    Uint32     *p;
    int		n, x, y;

    dest = *pdest;
    tile = (Uint32*)((char*)tiles->pixels + rect->y * tiles->pitch) + rect->x;
    for (n = count - 1 ; n >= 0 ; --n) {
	ptrs[n] = dest;
	p = tile;
	for (y = 0 ; y < rect->h ; ++y) {
	    for (x = 0 ; x < rect->w ; ++x)
		dest[x] = p[x] == transpclr ? sdlg.transpixel : p[x];
	    p = (Uint32*)((char*)p + tiles->pitch);
	    dest += x;
	}
	tile += rect->w;
    }
    *pdest = dest;
}

static int extracttileimage(SDL_Surface *tiles, int x, int y, int w, int h,
			    int id, int shape, Uint32 **pd, Uint32 transpclr)
{
    SDL_Rect	rect;
    int		n;

    rect.x = x;
    rect.y = y;
    rect.w = sdlg.wtile;
    rect.h = sdlg.htile;

    switch (shape) {
      case TILEIMG_SINGLEOPAQUE:
	if (h != 1 || w != 1) {
	    warn("outsized single tiles not permitted (%02X=%dx%d)", id, w, h);
	    return FALSE;
	}
	tileptr[id].transpsize = 0;
	tileptr[id].celcount = 1;
	extractopaquetileseq(tiles, &rect, 1, pd, tileptr[id].opaque,
			     transpclr);
	break;

      case TILEIMG_OPAQUECELS:
	if (h != 1) {
	    warn("outsized map tiles not permitted (%02X=%dx%d)", id, w, h);
	    return FALSE;
	}
	tileptr[id].transpsize = 0;
	tileptr[id].celcount = w;
	extractopaquetileseq(tiles, &rect, w, pd, tileptr[id].opaque,
			     transpclr);
	break;

      case TILEIMG_TRANSPCELS:
	if (h != 1) {
	    warn("outsized map tiles not permitted (%02X=%dx%d)", id, w, h);
	    return FALSE;
	}
	tileptr[id].transpsize = 0;
	tileptr[id].celcount = w;
	extracttransptileseq(tiles, &rect, w, pd, tileptr[id].transp,
			     transpclr);
	break;

      case TILEIMG_ANIMATION:
	if (h == 2 || (h == 3 && w % 3 != 0)) {
	    warn("off-center animation not permitted (%02X=%dx%d)", id, w, h);
	    return FALSE;
	}
	if (h == 3) {
	    tileptr[id].transpsize = SIZE_EXTALL;
	    tileptr[id].celcount = w / 3;
	    rect.w = 3 * sdlg.wtile;
	    rect.h = 3 * sdlg.htile;
	} else {
	    tileptr[id].transpsize = 0;
	    tileptr[id].celcount = w;
	    rect.w = sdlg.wtile;
	    rect.h = sdlg.htile;
	}
	extracttransptileseq(tiles, &rect, tileptr[id].celcount,
			     pd, tileptr[id].transp, transpclr);
	break;

      case TILEIMG_CREATURE:
	if (h == 1) {
	    if (w == 1) {
		tileptr[id].transpsize = 0;
		tileptr[id].celcount = 1;
		extracttransptileseq(tiles, &rect, 1, pd,
				     tileptr[id].transp, transpclr);
		tileptr[id + 1] = tileptr[id];
		tileptr[id + 2] = tileptr[id];
		tileptr[id + 3] = tileptr[id];
	    } else if (w == 4) {
		for (n = 0 ; n < 4 ; ++n) {
		    tileptr[id + n].transpsize = 0;
		    tileptr[id + n].celcount = 1;
		    extracttransptileseq(tiles, &rect, 1, pd,
					 tileptr[id + n].transp, transpclr);
		    rect.x += sdlg.wtile;
		}
	    } else {
		warn("invalid packing of creature tiles (%02X=%dx%d)",
		     id, w, h);
		return FALSE;
	    }
	} else if (h == 2) {
	    if (w == 2) {
		tileptr[id].transpsize = 0;
		tileptr[id].celcount = 1;
		extracttransptileseq(tiles, &rect, 1, pd,
				     tileptr[id].transp, transpclr);
		rect.x += sdlg.wtile;
		tileptr[id + 1].transpsize = 0;
		tileptr[id + 1].celcount = 1;
		extracttransptileseq(tiles, &rect, 1, pd,
				     tileptr[id + 1].transp, transpclr);
		rect.x -= sdlg.wtile;
		rect.y += sdlg.htile;
		tileptr[id + 2].transpsize = 0;
		tileptr[id + 2].celcount = 1;
		extracttransptileseq(tiles, &rect, 1, pd,
				     tileptr[id + 2].transp, transpclr);
		rect.x += sdlg.wtile;
		tileptr[id + 3].transpsize = 0;
		tileptr[id + 3].celcount = 1;
		extracttransptileseq(tiles, &rect, 1, pd,
				     tileptr[id + 3].transp, transpclr);
	    } else if (w == 8) {
		tileptr[id].transpsize = 0;
		tileptr[id].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id].transp, transpclr);
		rect.x += 4 * sdlg.wtile;
		tileptr[id + 1].transpsize = 0;
		tileptr[id + 1].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id + 1].transp, transpclr);
		rect.x -= 4 * sdlg.wtile;
		rect.y += sdlg.htile;
		tileptr[id + 2].transpsize = 0;
		tileptr[id + 2].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id + 2].transp, transpclr);
		rect.x += 4 * sdlg.wtile;
		tileptr[id + 3].transpsize = 0;
		tileptr[id + 3].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id + 3].transp, transpclr);
	    } else if (w == 16) {
		rect.w = sdlg.wtile;
		rect.h = 2 * sdlg.htile;
		tileptr[id].transpsize = SIZE_EXTDOWN;
		tileptr[id].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id].transp, transpclr);
		rect.x += 4 * sdlg.wtile;
		tileptr[id + 2].transpsize = SIZE_EXTUP;
		tileptr[id + 2].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id + 2].transp, transpclr);
		rect.x += 4 * sdlg.wtile;
		rect.w = 2 * sdlg.wtile;
		rect.h = sdlg.htile;
		tileptr[id + 1].transpsize = SIZE_EXTRIGHT;
		tileptr[id + 1].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id + 1].transp, transpclr);
		rect.y += sdlg.htile;
		tileptr[id + 3].transpsize = SIZE_EXTLEFT;
		tileptr[id + 3].celcount = 4;
		extracttransptileseq(tiles, &rect, 4, pd,
				     tileptr[id + 3].transp, transpclr);
	    } else {
		warn("invalid packing of creature tiles (%02X=%dx%d)",
		     id, w, h);
		return FALSE;
	    }
	} else {
	    warn("invalid packing of creature tiles (%02X=%dx%d)", id, w, h);
	    return FALSE;
	}
	break;
    }

    return TRUE;
}

static int initfreeformtileset(SDL_Surface *tiles)
{
    Uint32     *row;
    Uint32     *dest;
    char       *nextrow;
    Uint32	transpclr;
    long	size;
    int		rowcount;
    int		n, x, y, w, h;

    if (SDL_MUSTLOCK(tiles))
	SDL_LockSurface(tiles);

    row = tiles->pixels;
    transpclr = row[1];
    for (w = 1 ; w < tiles->w ; ++w)
	if (row[w] != transpclr)
	    break;
    if (w == tiles->w) {
	warn("Can't find tile separators");
	return FALSE;
    }
    if (w % 4 != 0) {
	warn("Tiles must have a width divisible by 4.");
	return FALSE;
    }
    for (h = 0 ; h < tiles->h ; ++h) {
	row = (Uint32*)((char*)row + tiles->pitch);
	if (*row != transpclr)
	    break;
    }
    if (h % 4 != 0) {
	warn("Tiles must have a height divisible by 4.");
	return FALSE;
    }
    if (!settilesize(w, h))
	return FALSE;

    size = 1;
    rowcount = 0;
    h = 1;
    row = (Uint32*)((char*)tiles->pixels + tiles->pitch);
    for (y = sdlg.htile ; y < tiles->h ; y += sdlg.htile) {
	row = (Uint32*)((char*)row + sdlg.htile * tiles->pitch);
	if (*row == transpclr) {
	    ++h;
	    continue;
	}
	++y;
	size += h * rowcount;
	h = 0;
	rowcount = 0;
	for (x = sdlg.wtile ; x < tiles->w ; x += sdlg.wtile)
	    if (row[x] != transpclr)
		rowcount = x;
	if (!rowcount)
	    break;
	rowcount /= sdlg.wtile;
	size += rowcount;
	row = (Uint32*)((char*)row + tiles->pitch);
    }

    if (!(cctiles = calloc(size, sdlg.cbtile)))
	memerrexit();
    dest = cctiles;

    for (n = 0 ; n < NTILES ; ++n) {
	tileptr[n].opaque[0] = NULL;
	tileptr[n].transp[0] = NULL;
	tileptr[n].celcount = 0;
	tileptr[n].transpsize = 0;
    }

    row = tiles->pixels;
    nextrow = (char*)row + (sdlg.htile + 1) * tiles->pitch;
    h = 1;
    x = 0;
    y = 0;
    for (n = 0 ; n < (int)(sizeof tileidmap / sizeof *tileidmap) ; ++n) {
	if (tileidmap[n].shape == TILEIMG_IMPLICIT)
	    continue;

      findwidth:
	w = 0;
	for (;;) {
	    ++w;
	    if (x + w * sdlg.wtile >= tiles->w) {
		w = 0;
		break;
	    }
	    if (row[x + w * sdlg.wtile] != transpclr)
		break;
	}
	if (!w) {
	    row = (Uint32*)nextrow;
	    nextrow += tiles->pitch;
	    y += 1 + h * sdlg.htile;
	    h = 0;
	    do {
		++h;
		if (y + h * sdlg.htile >= tiles->h) {
		    h = 0;
		    break;
		}
		nextrow += sdlg.htile * tiles->pitch;
	    } while (*(Uint32*)nextrow == transpclr);
	    if (!h) {
		warn("incomplete tile set: missing %02X",
		     tileidmap[n].id);
		goto failure;
	    }
	    x = 0;
	    goto findwidth;
	}
	if (!extracttileimage(tiles, x + 1, y + 1, w, h,
			      tileidmap[n].id, tileidmap[n].shape,
			      &dest, transpclr))
	    goto failure;
	x += w * sdlg.wtile;
    }

    if (SDL_MUSTLOCK(tiles))
	SDL_UnlockSurface(tiles);

    tileptr[Overlay_Buffer].opaque[0] = dest;
    tileptr[Overlay_Buffer].celcount = 1;

    tileptr[HiddenWall_Perm] = tileptr[Empty];
    tileptr[HiddenWall_Temp] = tileptr[Empty];
    tileptr[BlueWall_Fake] = tileptr[BlueWall_Real];

    tileptr[Block_Static].opaque[0] = tileptr[Block + 0].transp[0];
    tileptr[Block_Static].celcount = 1;

    return TRUE;

  failure:
    if (cctiles) {
	free(cctiles);
	cctiles = NULL;
    }
    sdlg.wtile = 0;
    sdlg.htile = 0;
    if (SDL_MUSTLOCK(tiles))
	SDL_UnlockSurface(tiles);
    return FALSE;
}

/*
 *
 */

void freetileset(void)
{
    free(cctiles);
    cctiles = NULL;
    sdlg.wtile = 0;
    sdlg.htile = 0;
    sdlg.cptile = 0;
    sdlg.cbtile = 0;
    memset(tileptr, 0, sizeof tileptr);
}

int loadtileset(char const *filename, int complain)
{
    SDL_Surface	       *tiles = NULL;
    SDL_Surface	       *mask = NULL;
    int			f, w, h;

    tiles = loadtilesto32(filename);
    if (!tiles) {
	if (complain)
	    errmsg(filename, "cannot read bitmap: %s", SDL_GetError());
	return FALSE;
    }

    if (tiles->w % 2 != 0) {
	freetileset();
	f = initfreeformtileset(tiles);
    } else if (tiles->w % 13 == 0 && tiles->h % 16 == 0) {
	w = tiles->w / 13;
	h = tiles->h / 16;
	mask = extractmask(tiles, 10 * w, 0, 3 * w, tiles->h);
	if (!mask) {
	    errmsg(filename, "couldn't create temporary mask surface: %s",
			     SDL_GetError());
	    f = FALSE;
	} else {
	    freetileset();
	    f = settilesize(w, h)
		&& initmaskedtileset(tiles, 10, 16, mask, 3, 16, 7, 0);
	    SDL_FreeSurface(mask);
	    mask = NULL;
	}
    } else if (tiles->w % 7 == 0 && tiles->h % 16 == 0) {
	freetileset();
	f = settilesize(tiles->w / 7, tiles->h / 16)
	    && initsmalltileset(tiles, 7, 16, 4, 0, 3, 16);
    } else {
	errmsg(filename, "image file has invalid dimensions");
	f = FALSE;
    }

    SDL_FreeSurface(tiles);
    return f;
}

/*
 *
 */

int _sdltileinitialize(void)
{
#if 0
    sdlg.gettileimagefunc = _gettileimage;
#endif
    sdlg.getcreatureimagefunc = _getcreatureimage;
    sdlg.getcellimagefunc = _getcellimage;
    return TRUE;
}
