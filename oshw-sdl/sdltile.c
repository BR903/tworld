/* sdltiles.c: Functions for rendering tile images.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
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
#include	"sdltile.h"

typedef	struct tilemap {
    Uint32     *opaque;
    Uint32     *transp;
} tilemap;

typedef	struct tileidmap {
    short	opaque;
    short	transp;
} tileidmap;

static tileidmap const small_tileidmap[NTILES] = {
/* Nothing		*/ { -1, -1 },
/* Empty		*/ { 0 * 16 +  0,           -1 },
/* Slide_North		*/ { 1 * 16 +  2,           -1 },
/* Slide_West		*/ { 1 * 16 +  4,           -1 },
/* Slide_South		*/ { 0 * 16 + 13,           -1 },
/* Slide_East		*/ { 1 * 16 +  3,           -1 },
/* Slide_Random		*/ { 3 * 16 +  2,           -1 },
/* Ice			*/ { 0 * 16 + 12,           -1 },
/* IceWall_Northwest	*/ { 1 * 16 + 12,           -1 },
/* IceWall_Northeast	*/ { 1 * 16 + 13,           -1 },
/* IceWall_Southwest	*/ { 1 * 16 + 11,           -1 },
/* IceWall_Southeast	*/ { 1 * 16 + 10,           -1 },
/* Gravel		*/ { 2 * 16 + 13,           -1 },
/* Dirt			*/ { 0 * 16 + 11,           -1 },
/* Water		*/ { 0 * 16 +  3,           -1 },
/* Fire			*/ { 0 * 16 +  4,           -1 },
/* Bomb			*/ { 2 * 16 + 10,           -1 },
/* Beartrap		*/ { 2 * 16 + 11,           -1 },
/* Burglar		*/ { 2 * 16 +  1,           -1 },
/* HintButton		*/ { 2 * 16 + 15,           -1 },
/* Button_Blue		*/ { 2 * 16 +  8,           -1 },
/* Button_Green		*/ { 2 * 16 +  3,           -1 },
/* Button_Red		*/ { 2 * 16 +  4,           -1 },
/* Button_Brown		*/ { 2 * 16 +  7,           -1 },
/* Teleport		*/ { 2 * 16 +  9,           -1 },
/* Wall			*/ { 0 * 16 +  1,           -1 },
/* Wall_North		*/ { 0 * 16 +  6,           -1 },
/* Wall_West		*/ { 0 * 16 +  7,           -1 },
/* Wall_South		*/ { 0 * 16 +  8,           -1 },
/* Wall_East		*/ { 0 * 16 +  9,           -1 },
/* Wall_Southeast	*/ { 3 * 16 +  0,           -1 },
/* HiddenWall_Perm	*/ { 0 * 16 +  5,           -1 },
/* HiddenWall_Temp	*/ { 2 * 16 + 12,           -1 },
/* BlueWall_Real	*/ { 1 * 16 + 15,           -1 },
/* BlueWall_Fake	*/ { 1 * 16 + 14,           -1 },
/* SwitchWall_Open	*/ { 2 * 16 +  6,           -1 },
/* SwitchWall_Closed	*/ { 2 * 16 +  5,           -1 },
/* PopupWall		*/ { 2 * 16 + 14,           -1 },
/* CloneMachine		*/ { 3 * 16 +  1,           -1 },
/* Door_Red		*/ { 1 * 16 +  7,           -1 },
/* Door_Blue		*/ { 1 * 16 +  6,           -1 },
/* Door_Yellow		*/ { 1 * 16 +  9,           -1 },
/* Door_Green		*/ { 1 * 16 +  8,           -1 },
/* Socket		*/ { 2 * 16 +  2,           -1 },
/* Exit			*/ { 1 * 16 +  5,           -1 },
/* ICChip		*/ { 0 * 16 +  2,           -1 },
/* Key_Red		*/ { 6 * 16 +  5,  9 * 16 +  5 },
/* Key_Blue		*/ { 6 * 16 +  4,  9 * 16 +  4 },
/* Key_Yellow		*/ { 6 * 16 +  7,  9 * 16 +  7 },
/* Key_Green		*/ { 6 * 16 +  6,  9 * 16 +  6 },
/* Boots_Ice		*/ { 6 * 16 + 10,  9 * 16 + 10 },
/* Boots_Slide		*/ { 6 * 16 + 11,  9 * 16 + 11 },
/* Boots_Fire		*/ { 6 * 16 +  9,  9 * 16 +  9 },
/* Boots_Water		*/ { 6 * 16 +  8,  9 * 16 +  8 },
/* Water_Splash		*/ { 3 * 16 +  3,           -1 },
/* Dirt_Splash		*/ { 3 * 16 +  7,           -1 },
/* Bomb_Explosion	*/ { 3 * 16 +  6,           -1 },
/* Block_Static		*/ { 0 * 16 + 10,           -1 },
/* Burned_Chip		*/ { 3 * 16 +  4,           -1 },
/* Bombed_Chip		*/ { 3 * 16 +  5,           -1 },
/* Exited_Chip		*/ { 3 * 16 +  9,           -1 },
/* Exit_Extra_1		*/ { 3 * 16 + 10,           -1 },
/* Exit_Extra_2		*/ { 3 * 16 + 11,           -1 },
/* Overlay_Buffer	*/ { 2 * 16 +  0,           -1 },
/* Chip			*/ { 6 * 16 + 12,  9 * 16 + 12 },
			   { 6 * 16 + 13,  9 * 16 + 13 },
			   { 6 * 16 + 14,  9 * 16 + 14 },
			   { 6 * 16 + 15,  9 * 16 + 15 },
/* Block		*/ { 0 * 16 + 14,           -1 },
			   { 0 * 16 + 15,           -1 },
			   { 1 * 16 +  0,           -1 },
			   { 1 * 16 +  1,           -1 },
/* Tank			*/ { 4 * 16 + 12,  7 * 16 + 12 },
			   { 4 * 16 + 13,  7 * 16 + 13 },
			   { 4 * 16 + 14,  7 * 16 + 14 },
			   { 4 * 16 + 15,  7 * 16 + 15 },
/* Ball			*/ { 4 * 16 +  8,  7 * 16 +  8 },
			   { 4 * 16 +  9,  7 * 16 +  8 },
			   { 4 * 16 + 10,  7 * 16 + 10 },
			   { 4 * 16 + 11,  7 * 16 + 11 },
/* Glider		*/ { 5 * 16 +  0,  8 * 16 +  0 },
			   { 5 * 16 +  1,  8 * 16 +  1 },
			   { 5 * 16 +  2,  8 * 16 +  2 },
			   { 5 * 16 +  3,  8 * 16 +  3 },
/* Fireball		*/ { 4 * 16 +  4,  7 * 16 +  4 },
			   { 4 * 16 +  5,  7 * 16 +  5 },
			   { 4 * 16 +  6,  7 * 16 +  6 },
			   { 4 * 16 +  7,  7 * 16 +  7 },
/* Walker		*/ { 5 * 16 +  8,  8 * 16 +  8 },
			   { 5 * 16 +  9,  8 * 16 +  9 },
			   { 5 * 16 + 10,  8 * 16 + 10 },
			   { 5 * 16 + 11,  8 * 16 + 11 },
/* Blob			*/ { 5 * 16 + 12,  8 * 16 + 12 },
			   { 5 * 16 + 13,  8 * 16 + 13 },
			   { 5 * 16 + 14,  8 * 16 + 14 },
			   { 5 * 16 + 15,  8 * 16 + 15 },
/* Teeth		*/ { 5 * 16 +  4,  8 * 16 +  4 },
			   { 5 * 16 +  5,  8 * 16 +  5 },
			   { 5 * 16 +  6,  8 * 16 +  6 },
			   { 5 * 16 +  7,  8 * 16 +  7 },
/* Bug			*/ { 4 * 16 +  0,  7 * 16 +  0 },
			   { 4 * 16 +  1,  7 * 16 +  1 },
			   { 4 * 16 +  2,  7 * 16 +  2 },
			   { 4 * 16 +  3,  7 * 16 +  3 },
/* Paramecium		*/ { 6 * 16 +  0,  9 * 16 +  0 },
			   { 6 * 16 +  1,  9 * 16 +  1 },
			   { 6 * 16 +  2,  9 * 16 +  2 },
			   { 6 * 16 +  3,  9 * 16 +  3 },
/* Swimming_Chip	*/ { 3 * 16 + 12,           -1 },
			   { 3 * 16 + 13,           -1 },
			   { 3 * 16 + 14,           -1 },
			   { 3 * 16 + 15,           -1 },
/* Pushing_Chip		*/ { 6 * 16 + 12,  9 * 16 + 12 },
			   { 6 * 16 + 13,  9 * 16 + 13 },
			   { 6 * 16 + 14,  9 * 16 + 14 },
			   { 6 * 16 + 15,  9 * 16 + 15 },
/* Entity_Reserved3	*/ { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 },
/* Entity_Reserved2	*/ { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 },
/* Entity_Reserved1	*/ { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 }
};

static Uint32	       *cctiles = NULL;
static tilemap		tileptr[NTILES];

/*
 * Functions for obtaining tile images.
 */

/* Return a pointer to a specific tile image.
 */
Uint32 const *_sdlgettileimage(int id, int transp)
{
    if (transp)
	return tileptr[id].transp ? tileptr[id].transp
				  : tileptr[id].opaque;
    else
	return tileptr[id].opaque ? tileptr[id].opaque
				  : tileptr[id].transp;
}

/* Return a pointer to a tile image for a creature.
 */
Uint32 const *_sdlgetcreatureimage(int id, int dir, int moving)
{
    (void)moving;
    id += (0x30210 >> (dir * 2)) & 3;
    return tileptr[id].transp ? tileptr[id].transp : tileptr[id].opaque;
}

/* Return a pointer to a tile image for a cell. If the top image is
 * transparent, the appropriate image is created in the overlay
 * buffer.
 */
Uint32 const *_sdlgetcellimage(int top, int bot)
{
    Uint32     *src;
    Uint32     *dest;
    int		n;

    if (bot == Nothing || bot == Empty || tileptr[top].transp == 0)
	return tileptr[top].opaque;
    src = tileptr[bot].opaque;
    dest = tileptr[Overlay_Buffer].opaque;
    memcpy(dest, src, sdlg.cbtile);
    src = tileptr[top].transp;
    for (n = 0 ; n < sdlg.cptile ; ++n)
	if (src[n] != (Uint32)sdlg.transpixel)
	    dest[n] = src[n];
    return dest;
}

/*
 *
 */

/* First, the tiles are transferred from the bitmap surface to a
 * surface with the same pixel format as the display. Then, the mask
 * is applied, to set the transparent pixels. Finally, the tiles are
 * transferred to a one-dimensional surface for easy copying.
 */
static int initializetileset(SDL_Surface *bmp, int wset, int hset,
			     int xmask, int ymask, int wmask, int hmask,
			     int xmaskdest, int ymaskdest)
{
    SDL_PixelFormat    *fmt;
    SDL_Surface	       *temp;
    SDL_Surface	       *mask;
    Uint8	       *src;
    Uint32	       *dest;
    Uint32	       *tiledest;
    SDL_Rect		srcrect;
    SDL_Rect		destrect;
    SDL_Color		pal[2];
    int			x, y, z, n;

    temp = sdlg.screen;
    if (!temp) {
	warn("initializetileset() called before creating 32-bit screen");
	fmt = SDL_GetVideoInfo()->vfmt;
    } else
	fmt = temp->format;

    temp = SDL_CreateRGBSurface(SDL_SWSURFACE,
				wset * sdlg.wtile, hset * sdlg.htile, 32,
				fmt->Rmask, fmt->Gmask,
				fmt->Bmask, fmt->Amask);
    if (!temp)
	die("couldn't create intermediate tile surface: %s", SDL_GetError());
    mask = SDL_CreateRGBSurface(SDL_SWSURFACE,
				wmask * sdlg.wtile, hmask * sdlg.htile, 8,
				0, 0, 0, 0);
    if (!mask)
	die("couldn't create mask surface: %s", SDL_GetError());
    pal[0].r = pal[0].g = pal[0].b = 0;
    pal[1].r = pal[1].g = pal[1].b = 255;
    SDL_SetPalette(mask, SDL_LOGPAL, pal, 0, 2);

    srcrect.x = 0;
    srcrect.y = 0;
    srcrect.w = wset * sdlg.wtile;
    srcrect.h = hset * sdlg.htile;
    destrect = srcrect;
    SDL_BlitSurface(bmp, &srcrect, temp, &destrect);
    destrect.x = 0;
    destrect.y = 0;
    srcrect.x = xmask * sdlg.wtile;
    srcrect.y = ymask * sdlg.htile;
    srcrect.w = wmask * sdlg.wtile;
    srcrect.h = hmask * sdlg.htile;
    SDL_BlitSurface(bmp, &srcrect, mask, &destrect);
    if (SDL_MUSTLOCK(temp))
	SDL_LockSurface(temp);
    if (SDL_MUSTLOCK(mask))
	SDL_LockSurface(mask);
    src = (Uint8*)mask->pixels;
    dest = (Uint32*)((char*)temp->pixels + ymaskdest * temp->pitch);
    dest += xmaskdest * sdlg.wtile;
    for (y = 0 ; y < hmask * sdlg.htile ; ++y) {
	for (x = 0 ; x < wmask * sdlg.wtile ; ++x)
	    if (!src[x])
		dest[x] = (Uint32)sdlg.transpixel;
	src = (Uint8*)((char*)src + mask->pitch);
	dest = (Uint32*)((char*)dest + temp->pitch);
    }
    if (SDL_MUSTLOCK(mask))
	SDL_UnlockSurface(mask);
    SDL_FreeSurface(mask);

    cctiles = calloc(wset * hset * sdlg.cptile, sizeof *cctiles);
    if (!dest)
	die("insufficient memory");
    dest = cctiles;
    for (x = 0 ; x < wset * sdlg.wtile ; x += sdlg.wtile) {
	for (y = 0 ; y < hset * sdlg.htile ; y += sdlg.htile) {
	    tiledest = (Uint32*)((char*)temp->pixels + y * temp->pitch) + x;
	    for (z = sdlg.htile ; z ; --z, dest += sdlg.wtile) {
		memcpy(dest, tiledest, sdlg.wtile * sizeof *dest);
		tiledest = (Uint32*)((char*)tiledest + temp->pitch);
	    }
	}
    }

    for (n = 0 ; n < NTILES ; ++n) {
	if (small_tileidmap[n].opaque >= 0)
	    tileptr[n].opaque = cctiles
			+ small_tileidmap[n].opaque * sdlg.cptile;
	else
	    tileptr[n].opaque = NULL;
	if (small_tileidmap[n].transp >= 0)
	    tileptr[n].transp = cctiles
			+ small_tileidmap[n].transp * sdlg.cptile;
	else
	    tileptr[n].transp = NULL;
    }

    return TRUE;
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

int loadsmalltileset(char const *filename, int complain)
{
    SDL_Surface	       *bmp;
    int			x, y;

    bmp = SDL_LoadBMP(filename);
    if (!bmp) {
	if (complain)
	    errmsg(filename, "cannot read bitmap: %s", SDL_GetError());
	return FALSE;
    }
    if (bmp->w % 13 || bmp->h % 16) {
	errmsg(filename, "image file has invalid dimensions");
	return FALSE;
    }
    x = bmp->w / 13;
    y = bmp->h / 16;
    if (x % 4 || y % 4) {
	errmsg("%s: invalid images: tile dimensions must be divisible by four",
	       filename);
	return FALSE;
    }

    freetileset();
    sdlg.wtile = x;
    sdlg.htile = y;
    sdlg.cptile = x * y;
    sdlg.cbtile = sdlg.cptile * sizeof(Uint32);
    initializetileset(bmp, 10, 16, 10, 0, 3, 16, 7, 0);

    SDL_FreeSurface(bmp);
    return cctiles != NULL;
}

int loadlargetileset(char const *filename, int complain)
{
    return loadsmalltileset(filename, complain);
}
