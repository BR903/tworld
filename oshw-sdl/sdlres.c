/* sdlres.c: Functions for accessing external resources.
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
#include	"sdlres.h"

/* An automatically-generated file.
 */
#include	"ccicon.c"

typedef	struct tilesetspec {
    int		id;
    int		xpos;
    int		ypos;
    int		xmaskpos;
    int		ymaskpos;
} tilesetspec;

static tilesetspec const small_tilemap[] = {
/* 00 empty space		*/	{ Empty,	     0,  0, -1, -1 },
/* 01 wall			*/	{ Wall,		     0,  1, -1, -1 },
/* 02 chip			*/	{ ICChip,	     0,  2, -1, -1 },
/* 03 water			*/	{ Water,	     0,  3, -1, -1 },
/* 04 fire			*/	{ Fire,		     0,  4, -1, -1 },
/* 05 invisible wall, perm.	*/	{ HiddenWall_Perm,   0,  5, -1, -1 },
/* 06 blocked north		*/	{ Wall_North,	     0,  6, -1, -1 },
/* 07 blocked west		*/	{ Wall_West,	     0,  7, -1, -1 },
/* 08 blocked south		*/	{ Wall_South,	     0,  8, -1, -1 },
/* 09 blocked east		*/	{ Wall_East,	     0,  9, -1, -1 },
/* 0A block			*/	{ Block_Static,	     0, 10, -1, -1 },
/* 0B dirt			*/	{ Dirt,		     0, 11, -1, -1 },
/* 0C ice			*/	{ Ice,		     0, 12, -1, -1 },
/* 0D force south		*/	{ Slide_South,	     0, 13, -1, -1 },
/* 0E cloning block north	*/	{ Block + 0,	     0, 14, -1, -1 },
/* 0F cloning block west	*/	{ Block + 1,	     0, 15, -1, -1 },
/* 10 cloning block south	*/	{ Block + 2,	     1,  0, -1, -1 },
/* 11 cloning block east	*/	{ Block + 3,	     1,  1, -1, -1 },
/* 12 force north		*/	{ Slide_North,	     1,  2, -1, -1 },
/* 13 force east		*/	{ Slide_East,	     1,  3, -1, -1 },
/* 14 force west		*/	{ Slide_West,	     1,  4, -1, -1 },
/* 15 exit			*/	{ Exit,		     1,  5, -1, -1 },
/* 16 blue door			*/	{ Door_Blue,	     1,  6, -1, -1 },
/* 17 red door			*/	{ Door_Red,	     1,  7, -1, -1 },
/* 18 green door		*/	{ Door_Green,	     1,  8, -1, -1 },
/* 19 yellow door		*/	{ Door_Yellow,	     1,  9, -1, -1 },
/* 1A SE ice slide		*/	{ IceWall_Southeast, 1, 10, -1, -1 },
/* 1B SW ice slide		*/	{ IceWall_Southwest, 1, 11, -1, -1 },
/* 1C NW ice slide		*/	{ IceWall_Northwest, 1, 12, -1, -1 },
/* 1D NE ice slide		*/	{ IceWall_Northeast, 1, 13, -1, -1 },
/* 1E blue block, tile		*/	{ BlueWall_Fake,     1, 14, -1, -1 },
/* 1F blue block, wall		*/	{ BlueWall_Real,     1, 15, -1, -1 },
/* 20 not used			*/	{ Overlay_Buffer,    2,  0, -1, -1 },
/* 21 thief			*/	{ Burglar,	     2,  1, -1, -1 },
/* 22 socket			*/	{ Socket,	     2,  2, -1, -1 },
/* 23 green button		*/	{ Button_Green,	     2,  3, -1, -1 },
/* 24 red button		*/	{ Button_Red,	     2,  4, -1, -1 },
/* 25 switch block, closed	*/	{ SwitchWall_Closed, 2,  5, -1, -1 },
/* 26 switch block, open	*/	{ SwitchWall_Open,   2,  6, -1, -1 },
/* 27 brown button		*/	{ Button_Brown,	     2,  7, -1, -1 },
/* 28 blue button		*/	{ Button_Blue,	     2,  8, -1, -1 },
/* 29 teleport			*/	{ Teleport,	     2,  9, -1, -1 },
/* 2A bomb			*/	{ Bomb,		     2, 10, -1, -1 },
/* 2B trap			*/	{ Beartrap,	     2, 11, -1, -1 },
/* 2C invisible wall, temp.	*/	{ HiddenWall_Temp,   2, 12, -1, -1 },
/* 2D gravel			*/	{ Gravel,	     2, 13, -1, -1 },
/* 2E pass once			*/	{ PopupWall,	     2, 14, -1, -1 },
/* 2F hint			*/	{ HintButton,	     2, 15, -1, -1 },
/* 30 blocked SE		*/	{ Wall_Southeast,    3,  0, -1, -1 },
/* 31 cloning machine		*/	{ CloneMachine,	     3,  1, -1, -1 },
/* 32 force all directions	*/	{ Slide_Random,	     3,  2, -1, -1 },
/* 33 drowning Chip		*/	{ Water_Splash,	     3,  3, -1, -1 },
/* 34 burned Chip		*/	{ Burned_Chip,	     3,  4, -1, -1 },
/* 35 burned Chip		*/	{ Bombed_Chip,	     3,  5, -1, -1 },
/* 36 not used / explosion	*/	{ Bomb_Explosion,    3,  6, -1, -1 },
/* 37 not used / dirt splash	*/	{ Dirt_Splash,	     3,  7, -1, -1 },
/* 38 not used			*/
/* 39 Chip in exit		*/	{ Exited_Chip,	     3,  9, -1, -1 },
/* 3A exit - end game		*/	{ Exit_Extra_1,	     3, 10, -1, -1 },
/* 3B exit - end game		*/	{ Exit_Extra_2,	     3, 11, -1, -1 },
/* 3C Chip swimming north	*/	{ Swimming_Chip + 0, 3, 12, -1, -1 },
/* 3D Chip swimming west	*/	{ Swimming_Chip + 1, 3, 13, -1, -1 },
/* 3E Chip swimming south	*/	{ Swimming_Chip + 2, 3, 14, -1, -1 },
/* 3F Chip swimming east	*/	{ Swimming_Chip + 3, 3, 15, -1, -1 },
/* 40 Bug N			*/	{ Bug + 0,	     7,  0, 10,  0 },
/* 41 Bug W			*/	{ Bug + 1,	     7,  1, 10,  1 },
/* 42 Bug S			*/	{ Bug + 2,	     7,  2, 10,  2 },
/* 43 Bug E			*/	{ Bug + 3,	     7,  3, 10,  3 },
/* 44 Fireball N		*/	{ Fireball + 0,	     7,  4, 10,  4 },
/* 45 Fireball W		*/	{ Fireball + 1,	     7,  5, 10,  5 },
/* 46 Fireball S		*/	{ Fireball + 2,	     7,  6, 10,  6 },
/* 47 Fireball E		*/	{ Fireball + 3,	     7,  7, 10,  7 },
/* 48 Pink ball N		*/	{ Ball + 0,	     7,  8, 10,  8 },
/* 49 Pink ball W		*/	{ Ball + 1,	     7,  9, 10,  9 },
/* 4A Pink ball S		*/	{ Ball + 2,	     7, 10, 10, 10 },
/* 4B Pink ball E		*/	{ Ball + 3,	     7, 11, 10, 11 },
/* 4C Tank N			*/	{ Tank + 0,	     7, 12, 10, 12 },
/* 4D Tank W			*/	{ Tank + 1,	     7, 13, 10, 13 },
/* 4E Tank S			*/	{ Tank + 2,	     7, 14, 10, 14 },
/* 4F Tank E			*/	{ Tank + 3,	     7, 15, 10, 15 },
/* 50 Glider N			*/	{ Glider + 0,	     8,  0, 11,  0 },
/* 51 Glider W			*/	{ Glider + 1,	     8,  1, 11,  1 },
/* 52 Glider S			*/	{ Glider + 2,	     8,  2, 11,  2 },
/* 53 Glider E			*/	{ Glider + 3,	     8,  3, 11,  3 },
/* 54 Teeth N			*/	{ Teeth + 0,	     8,  4, 11,  4 },
/* 55 Teeth W			*/	{ Teeth + 1,	     8,  5, 11,  5 },
/* 56 Teeth S			*/	{ Teeth + 2,	     8,  6, 11,  6 },
/* 57 Teeth E			*/	{ Teeth + 3,	     8,  7, 11,  7 },
/* 58 Walker N			*/	{ Walker + 0,	     8,  8, 11,  8 },
/* 59 Walker W			*/	{ Walker + 1,	     8,  9, 11,  9 },
/* 5A Walker S			*/	{ Walker + 2,	     8, 10, 11, 10 },
/* 5B Walker E			*/	{ Walker + 3,	     8, 11, 11, 11 },
/* 5C Blob N			*/	{ Blob + 0,	     8, 12, 11, 12 },
/* 5D Blob W			*/	{ Blob + 1,	     8, 13, 11, 13 },
/* 5E Blob S			*/	{ Blob + 2,	     8, 14, 11, 14 },
/* 5F Blob E			*/	{ Blob + 3,	     8, 15, 11, 15 },
/* 60 Paramecium N		*/	{ Paramecium + 0,    9,  0, 12,  0 },
/* 61 Paramecium W		*/	{ Paramecium + 1,    9,  1, 12,  1 },
/* 62 Paramecium S		*/	{ Paramecium + 2,    9,  2, 12,  2 },
/* 63 Paramecium E		*/	{ Paramecium + 3,    9,  3, 12,  3 },
/* 64 Blue key			*/	{ Key_Blue,	     9,  4, 12,  4 },
/* 65 Red key			*/	{ Key_Red,	     9,  5, 12,  5 },
/* 66 Green key			*/	{ Key_Green,	     9,  6, 12,  6 },
/* 67 Yellow key		*/	{ Key_Yellow,	     9,  7, 12,  7 },
/* 68 Flippers			*/	{ Boots_Water,	     9,  8, 12,  8 },
/* 69 Fire boots		*/	{ Boots_Fire,	     9,  9, 12,  9 },
/* 6A Ice skates		*/	{ Boots_Ice,	     9, 10, 12, 10 },
/* 6B Suction boots		*/	{ Boots_Slide,	     9, 11, 12, 11 },
/* 6C Chip N			*/	{ Chip + 0,	     9, 12, 12, 12 },
/* 6D Chip W			*/	{ Chip + 1,	     9, 13, 12, 13 },
/* 6E Chip S			*/	{ Chip + 2,	     9, 14, 12, 14 },
/* 6F Chip E			*/	{ Chip + 3,	     9, 15, 12, 15 }
};

static SDL_Surface	       *cctilesurface = NULL;

static SDL_PixelFormat const   *destfmt = NULL;
static Uint32			transparentpixel = 0;

Uint32			       *cctiles = NULL;
int				cxtile = 0;
int				cytile = 0;

/* Flag array indicating which tiles have transparent pixels.
 */
char				transparency[NTILES];

static int extracttiles(SDL_Surface *tilesrc,
			tilesetspec const *spec, int count)
{
    SDL_Color		bwpal[2] = { { 0, 0, 0, 0 }, { 255, 255, 255, 0 } };
    SDL_Rect		srcrect, destrect;
    SDL_Surface	       *maskbuf;
    Uint32	       *p;
    int			m, n;

    destrect.x = 0;
    destrect.y = 0;
    destrect.w = cxtile;
    destrect.h = cytile * NTILES;
    SDL_FillRect(cctilesurface, &destrect, transparentpixel);

    maskbuf = SDL_CreateRGBSurface(SDL_SWSURFACE, cxtile, cytile, 8,
				   0, 0, 0, 0);
    if (!maskbuf)
	return FALSE;
    SDL_SetPalette(maskbuf, SDL_LOGPAL, bwpal, 0, 2);

    memset(transparency, FALSE, sizeof transparency);
    transparency[0] = TRUE;
    for (n = 0 ; n < count ; ++n) {
	if (spec[n].id < 0)
	    continue;
	srcrect.x = spec[n].xpos * cxtile;
	srcrect.y = spec[n].ypos * cytile;
	destrect.x = 0;
	destrect.y = spec[n].id * cytile;
	srcrect.w = destrect.w = cxtile;
	srcrect.h = destrect.h = cytile;
	SDL_BlitSurface(tilesrc, &srcrect, cctilesurface, &destrect);

	if (spec[n].xmaskpos < 0)
	    continue;

	transparency[spec[n].id] = TRUE;
	srcrect.x = spec[n].xmaskpos * cxtile;
	srcrect.y = spec[n].ymaskpos * cytile;
	destrect.x = 0;
	destrect.y = 0;
	srcrect.w = destrect.w = cxtile;
	srcrect.h = destrect.h = cytile;
	SDL_BlitSurface(tilesrc, &srcrect, maskbuf, &destrect);
	if (SDL_MUSTLOCK(cctilesurface))
	    SDL_LockSurface(cctilesurface);
	if (SDL_MUSTLOCK(maskbuf))
	    SDL_LockSurface(maskbuf);
	p = (Uint32*)cctilesurface->pixels + spec[n].id * cxtile * cytile;
	for (m = 0 ; m < cxtile * cytile ; ++m)
	    if (((Uint8*)(maskbuf->pixels))[m] == 0)
		p[m] = transparentpixel;
	if (SDL_MUSTLOCK(cctilesurface))
	    SDL_UnlockSurface(cctilesurface);
	if (SDL_MUSTLOCK(maskbuf))
	    SDL_UnlockSurface(maskbuf);
    }

    return TRUE;
}

static int loadtileset(char const *filename, int nxtiles, int nytiles,
		       tilesetspec const *spec, int count, int complain)
{
    SDL_Surface	       *bmp;
    int			x, y;

    bmp = SDL_LoadBMP(filename);
    if (!bmp) {
	if (complain)
	    errmsg(filename, "cannot read bitmap: %s", SDL_GetError());
	return FALSE;
    }
    if (bmp->w % nxtiles || bmp->h % nytiles) {
	errmsg(filename, "image file has invalid dimensions");
	return FALSE;
    }
    x = bmp->w / nxtiles;
    y = bmp->h / nytiles;
    if (x % 4 || y % 4) {
	errmsg("%s: invalid images: tile dimensions must be divisible by four",
	       filename);
	return FALSE;
    }

    freetileset();

    cxtile = x;
    cytile = y;
    if (!destfmt) {
	warn("loadtileset() called before _sdlsettileformat()");
	destfmt = SDL_GetVideoInfo()->vfmt;
    }
    cctilesurface = SDL_CreateRGBSurface(SDL_SWSURFACE,
					 cxtile, cytile * NTILES,
					 destfmt->BitsPerPixel,
					 destfmt->Rmask, destfmt->Gmask,
					 destfmt->Bmask, destfmt->Amask);
    if (!cctilesurface)
	die("Cannot create tile surface: %s", SDL_GetError());

    extracttiles(bmp, spec, count);

    SDL_FreeSurface(bmp);

    if (SDL_MUSTLOCK(cctilesurface))
	SDL_LockSurface(cctilesurface);
    cctiles = cctilesurface->pixels;

    return TRUE;
}

void _sdlsettransparentcolor(Uint32 color) { transparentpixel = color; }
void _sdlsettileformat(SDL_PixelFormat const *fmt) { destfmt = fmt; }

/*
 *
 */

int loadsmalltileset(char const *filename, int complain)
{
    return loadtileset(filename, 13, 16, small_tilemap,
		       sizeof small_tilemap / sizeof *small_tilemap, complain);
}

int loadlargetileset(char const *filename, int complain)
{
    return loadtileset(filename, 13, 16, small_tilemap,
		       sizeof small_tilemap / sizeof *small_tilemap, complain);
}

void freetileset(void)
{
    if (cctilesurface) {
	if (SDL_MUSTLOCK(cctilesurface))
	    SDL_UnlockSurface(cctilesurface);
	SDL_FreeSurface(cctilesurface);
    }
    cctilesurface = NULL;
    cctiles = NULL;
    cxtile = 0;
    cytile = 0;
}

/*
 *
 */

int _sdlresourceinitialize(void)
{
    SDL_Surface	       *icon;

    icon = SDL_CreateRGBSurfaceFrom(cciconimage, CXCCICON, CYCCICON,
				    32, 4 * CXCCICON,
				    0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (!icon)
	warn("%s", SDL_GetError());
    if (icon) {
	SDL_WM_SetIcon(icon, cciconmask);
	SDL_FreeSurface(icon);
    }

    return TRUE;
}
