/* sdlout.c: Creating the program's displays.
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

/* Space to leave between graphic objects.
 */
#define	MARGINW		8
#define	MARGINH		8

/* Size of the end-message icons.
 */
#define	ENDICONW	16
#define	ENDICONH	10

/* The dimensions of the visible area of the map (in tiles).
 */
#define	NXTILES		9
#define	NYTILES		9

/* The graphic output buffer.
 */
static SDL_Surface     *screen = NULL;

/* Some prompting icons.
 */
static SDL_Surface     *endmsgicons = NULL;

/* Special pixel values.
 */
static Uint32		clr_black, clr_white, clr_gray, clr_red, clr_yellow;

/* Coordinates specifying the layout of the screen elements.
 */
static SDL_Rect		titleloc, infoloc, hintloc, invloc;
static SDL_Rect		onomatopoeialoc, endmsgloc, listloc, displayloc;
static int		screenw, screenh;

/* Create some simple icons used to prompt the user.
 */

static int createendmsgicons(void)
{
    static Uint8 iconpixels[] = {
	0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,
	0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
	0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,
	0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,
	0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,
	0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,
	0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0
    };

    if (!endmsgicons) {
	endmsgicons = SDL_CreateRGBSurfaceFrom(iconpixels,
					       ENDICONW, 3 * ENDICONH,
					       8, ENDICONW, 0, 0, 0, 0);
	if (!endmsgicons) {
	    warn("couldn't create SDL surface: %s", SDL_GetError());
	    return FALSE;
	}
	endmsgicons->format->palette->colors[0].r = 0;
	endmsgicons->format->palette->colors[0].g = 0;
	endmsgicons->format->palette->colors[0].b = 0;
	endmsgicons->format->palette->colors[1].r = 192;
	endmsgicons->format->palette->colors[1].g = 192;
	endmsgicons->format->palette->colors[1].b = 192;
    }
    return TRUE;
}

/*
 *
 */

static void layoutlistarea(void)
{
    listloc.x = MARGINW;
    listloc.y = MARGINH;
    listloc.w = (screenw - MARGINW) - listloc.x;
    listloc.h = (screenh - MARGINH - sdlg.font.h) - listloc.y;
}

/* Calculate the positions of all the elements of the game display.
 */
static int layoutscreen(void)
{
    int	w;

    if (sdlg.wtile <= 0 || sdlg.htile <= 0)
	return FALSE;

    displayloc.x = MARGINW;
    displayloc.y = MARGINH;
    displayloc.w = NXTILES * sdlg.wtile;
    displayloc.h = NYTILES * sdlg.htile;

    titleloc.x = displayloc.x;
    titleloc.y = displayloc.y + displayloc.h + MARGINH;
    titleloc.w = displayloc.w;
    titleloc.h = sdlg.font.h;

    w = 4 * sdlg.wtile;
    if (w < 18 * sdlg.font.w)
	w = 18 * sdlg.font.w;

    infoloc.x = displayloc.x + displayloc.w + MARGINW;
    infoloc.y = MARGINH;
    infoloc.w = w;
    infoloc.h = 6 * sdlg.font.h;

    invloc.x = infoloc.x;
    invloc.y = infoloc.y + infoloc.h + MARGINH;
    invloc.w = w;
    invloc.h = 2 * sdlg.htile;

    endmsgloc.x = infoloc.x + infoloc.w - ENDICONW;
    endmsgloc.y = titleloc.y + titleloc.h - ENDICONH;
    endmsgloc.w = ENDICONW;
    endmsgloc.h = ENDICONH;

    onomatopoeialoc.x = infoloc.x;
    onomatopoeialoc.y = displayloc.y + displayloc.h - sdlg.font.h;
    onomatopoeialoc.w = infoloc.w;
    onomatopoeialoc.h = sdlg.font.h;

    hintloc.x = infoloc.x;
    hintloc.y = invloc.y + invloc.h + MARGINH;
    hintloc.w = w;
    hintloc.h = endmsgloc.y - MARGINH - hintloc.y;

    screenw = infoloc.x + infoloc.w + MARGINW;
    screenh = titleloc.y + titleloc.h + MARGINH;
    w = 48 * sdlg.font.w + 2 * MARGINW;
    if (screenw < w)
	screenw = w;

     return TRUE;
}

/* Change the dimensions of the game surface.
 */
static int setdisplaysize(void)
{
    if (screen) {
	SDL_FreeSurface(screen);
	screen = NULL;
    }
    if (!(screen = SDL_SetVideoMode(screenw, screenh, 32, SDL_HWSURFACE)))
	die("Cannot open %dx%d display: %s\n",
	    screenw, screenh, SDL_GetError());
    if (screen->w != screenw || screen->h != screenh)
	die("Requested a %dx%d display, got %dx%d instead!",
	    screen->w, screen->h);
    if (screen->format->BitsPerPixel != 32)
	die("Requested a display with 32-bit depth, got %d-bit instead!",
	    screen->format->BitsPerPixel);
    SDL_FillRect(screen, NULL, clr_black);

    sdlg.screen = screen;
    layoutlistarea();

    return TRUE;
}

static char const *getonomatopoeia(unsigned long sfx)
{
    static char const  *sounds[] = { "\"Bummer\"",
	"Tadaa!", "Clang!", "-Tick-", "Mnphf!", "Chack!", "Slurp!",
	"Flonk!", "Bamff!", "Spang!", "Dring!", "Click!", "Whisk!",
	"Chunk!", "Shunk!", "Scrrr!", "Booom!", "Plash!",
	"(slurp slurp)", "(snick snick)", "(plip plip)", "(crackle crackle)",
	"Whizz ...", "Whing!", "Drrrr ..."
    };
    unsigned long	flag;
    int			i;

    flag = 1;
    for (i = 0 ; i < (int)(sizeof sounds / sizeof *sounds) ; ++i) {
	if (sfx & flag)
	    return sounds[i];
	flag <<= 1;
    }
    return "";
}

/*
 * Tile display functions.
 */

static void drawopaquetile(int xpos, int ypos, Uint32 const *src)
{
    void const	       *endsrc;
    unsigned char      *dest;

    endsrc = src + sdlg.cptile;
    dest = (unsigned char*)screen->pixels + ypos * screen->pitch;
    dest = (unsigned char*)((Uint32*)dest + xpos);
    for ( ; src != endsrc ; src += sdlg.wtile, dest += screen->pitch)
	memcpy(dest, src, sdlg.wtile * sizeof *src);
}

#if 0
static void drawtransptile(int xpos, int ypos, Uint32 const *src)
{
    unsigned char      *line;
    Uint32	       *dest;
    void const	       *endsrc;
    int			x;

    endsrc = src + sdlg.cptile;
    line = (unsigned char*)screen->pixels + ypos * screen->pitch;
    line = (unsigned char*)((Uint32*)dest + xpos);
    for ( ; src != endsrc ; src += sdlg.wtile, line += screen->pitch) {
	dest = (Uint32*)line;
	for (x = 0 ; x < sdlg.wtile ; ++x)
	    if (src[x] != sdlg.transpixel)
		dest[x] = src[x];
    }
}
#endif

static void drawopaquetileclipped(int xpos, int ypos, Uint32 const *src)
{
    unsigned char      *dest;
    int			lclip = displayloc.x;
    int			tclip = displayloc.y;
    int			rclip = displayloc.x + displayloc.w;
    int			bclip = displayloc.y + displayloc.h;
    int			y;

    if (xpos > lclip)			lclip = xpos;
    if (ypos > tclip)			tclip = ypos;
    if (xpos + sdlg.wtile < rclip)	rclip = xpos + sdlg.wtile;
    if (ypos + sdlg.htile < bclip)	bclip = ypos + sdlg.htile;
    if (lclip >= rclip || tclip >= bclip)
	return;
    src += (tclip - ypos) * sdlg.wtile + lclip - xpos;
    dest = (unsigned char*)screen->pixels + tclip * screen->pitch;
    dest = (unsigned char*)((Uint32*)dest + lclip);
    for (y = bclip - tclip ; y ; --y, dest += screen->pitch, src += sdlg.wtile)
	memcpy(dest, src, (rclip - lclip) * sizeof *src);
}

static void drawtransptileclipped(int xpos, int ypos, Uint32 const *src)
{
    unsigned char      *line;
    Uint32	       *dest;
    int			lclip = displayloc.x;
    int			tclip = displayloc.y;
    int			rclip = displayloc.x + displayloc.w;
    int			bclip = displayloc.y + displayloc.h;
    int			x, y;

    if (xpos > lclip)			lclip = xpos;
    if (ypos > tclip)			tclip = ypos;
    if (xpos + sdlg.wtile < rclip)	rclip = xpos + sdlg.wtile;
    if (ypos + sdlg.htile < bclip)	bclip = ypos + sdlg.htile;
    if (lclip >= rclip || tclip >= bclip)
	return;
    src += (tclip - ypos) * sdlg.wtile - xpos;
    line = (unsigned char*)screen->pixels + tclip * screen->pitch;
    for (y = bclip - tclip ; y ; --y, line += screen->pitch, src += sdlg.wtile)
	for (x = lclip, dest = (Uint32*)line ; x < rclip ; ++x)
	    if (src[x] != sdlg.transpixel)
		dest[x] = src[x];
}

/*
 * Game display functions.
 */

/* Wipe the display.
 */
void cleardisplay(void)
{
    SDL_FillRect(sdlg.screen, NULL, sdlg.bkgndcolor);
}

/* Render the view of the visible area of the map to the output
 * buffer, including all visible creatures, with Chip centered on the
 * display as much as possible.
 */
static void displaymapview(gamestate const *state)
{
    creature const     *cr;
    int			xdisppos, ydisppos;
    int			lmap, tmap, rmap, bmap;
    int			pos, x, y;

    xdisppos = state->xviewpos / 2 - (NXTILES / 2) * 4;
    ydisppos = state->yviewpos / 2 - (NYTILES / 2) * 4;
    if (xdisppos < 0)
	xdisppos = 0;
    if (ydisppos < 0)
	ydisppos = 0;
    if (xdisppos > (CXGRID - NXTILES) * 4)
	xdisppos = (CXGRID - NXTILES) * 4;
    if (ydisppos > (CYGRID - NYTILES) * 4)
	ydisppos = (CYGRID - NYTILES) * 4;

    lmap = xdisppos / 4;
    tmap = ydisppos / 4;
    rmap = (xdisppos + 3) / 4 + NXTILES;
    bmap = (ydisppos + 3) / 4 + NYTILES;
    for (y = tmap ; y < bmap ; ++y) {
	if (y < 0 || y >= CXGRID)
	    continue;
	for (x = lmap ; x < rmap ; ++x) {
	    if (x < 0 || x >= CXGRID)
		continue;
	    pos = y * CXGRID + x;
	    drawopaquetileclipped(displayloc.x + (x * sdlg.wtile)
					       - (xdisppos * sdlg.wtile / 4),
				  displayloc.y + (y * sdlg.htile)
					       - (ydisppos * sdlg.htile / 4),
				  getcellimage(state->map[pos].top.id,
					       state->map[pos].bot.id));
	}
    }

    lmap = (lmap - 1) * 4;
    tmap = (tmap - 1) * 4;
    rmap = (rmap + 1) * 4;
    bmap = (bmap + 1) * 4;
    for (cr = state->creatures ; cr->id ; ++cr) {
	if (cr->hidden)
	    continue;
	x = (cr->pos % CXGRID) * 4;
	y = (cr->pos / CXGRID) * 4;
	if (cr->moving > 0) {
	    switch (cr->dir) {
	      case NORTH:	y += cr->moving / 2;	break;
	      case WEST:	x += cr->moving / 2;	break;
	      case SOUTH:	y -= cr->moving / 2;	break;
	      case EAST:	x -= cr->moving / 2;	break;
	    }
	}
	if (x < lmap || x >= rmap || y < tmap || y >= bmap)
	    continue;
	drawtransptileclipped(displayloc.x + (x * sdlg.wtile / 4)
					   - (xdisppos * sdlg.wtile / 4),
			      displayloc.y + (y * sdlg.htile / 4)
					   - (ydisppos * sdlg.htile / 4),
			      getcreatureimage(cr->id, cr->dir, 0));
    }
}

/* Render all the various nuggets of data that comprise the
 * information display to the output buffer.
 */
static void displayinfo(gamestate const *state, int timeleft, int besttime)
{
    SDL_Rect	rect;
    char	buf[32];
    int		color;
    int		n;

    puttext(&titleloc, state->game->name, -1, PT_CENTER);

    rect = infoloc;

    sprintf(buf, "Level %d", state->game->number);
    puttext(&rect, buf, -1, PT_UPDATERECT);

    if (state->game->passwd && *state->game->passwd) {
	sprintf(buf, "Password: %s", state->game->passwd);
	puttext(&rect, buf, -1, PT_UPDATERECT);
    } else
	puttext(&rect, "", 0, PT_UPDATERECT);

    puttext(&rect, "", 0, PT_UPDATERECT);

    sprintf(buf, "Chips %3d", state->chipsneeded);
    puttext(&rect, buf, -1, PT_UPDATERECT);

    if (timeleft < 0)
	strcpy(buf, "Time  ---");
    else
	sprintf(buf, "Time  %3d", timeleft);
    puttext(&rect, buf, -1, PT_UPDATERECT);

    if (besttime) {
	if (timeleft < 0)
	    sprintf(buf, "(Best time: %d)", besttime);
	else
	    sprintf(buf, "Best time: %3d", besttime);
	color = sdlg.textcolor;
	if (state->game->replacebest)
	    sdlg.textcolor = clr_gray;
	puttext(&rect, buf, -1, PT_UPDATERECT);
	sdlg.textcolor = color;
    }
    putblank(&rect);

    for (n = 0 ; n < 4 ; ++n) {
	drawopaquetile(invloc.x + n * sdlg.wtile, invloc.y,
		gettileimage(state->keys[n] ? Key_Red + n : Empty, FALSE));
	drawopaquetile(invloc.x + n * sdlg.wtile, invloc.y + sdlg.htile,
		gettileimage(state->boots[n] ? Boots_Ice + n : Empty, FALSE));
    }

    if (state->statusflags & SF_INVALID)
	puttext(&hintloc, "This level cannot be played.", 0, PT_MULTILINE);
    else if (state->statusflags & SF_SHOWHINT)
	puttext(&hintloc, state->game->hinttext, 0, PT_MULTILINE | PT_RIGHT);
    else
	putblank(&hintloc);

    if (state->statusflags & SF_ONOMATOPOEIA)
	puttext(&onomatopoeialoc, getonomatopoeia(state->soundeffects), -1, 0);

    putblank(&endmsgloc);
}

/*
 * The exported functions.
 */

/* Display the current state of the game.
 */
int displaygame(void const *_state, int timeleft, int besttime)
{
    gamestate const    *state = _state;

    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);
    displaymapview(state);
    displayinfo(state, timeleft, besttime);
    if (SDL_MUSTLOCK(screen))
	SDL_UnlockSurface(screen);

    SDL_UpdateRect(screen, 0, 0, 0, 0);

    return TRUE;
}

/* Display a scrollable list, calling the callback function to manage
 * the selection until it returns FALSE.
 */
int displaylist(char const *title, char const *header,
		char const **items, int itemcount, int *index,
		int (*inputcallback)(int*))
{
    SDL_Rect	rect;
    scrollinfo	scroll;
    int		n;

    cleardisplay();

    rect = listloc;
    rect.y += listloc.h;
    rect.h = sdlg.font.h;
    puttext(&rect, title, -1, 0);
    rect = listloc;
    if (header)
	puttext(&rect, header, -1, PT_UPDATERECT);
    SDL_UpdateRect(screen, 0, 0, 0, 0);

    createscroll(&scroll, &rect, clr_yellow, itemcount, items);
    scrollmove(&scroll, *index);

    for (;;) {
	n = 0;
	if (!(*inputcallback)(&n))
	    break;
	scrollmove(&scroll, n);
    }
    if (n)
	*index = scroll.index;

    cleardisplay();
    return n;
}

/* Display a message appropriate to the end of game play in one corner.
 */
int displayendmessage(int completed)
{
    SDL_Rect	src;

    if (!endmsgicons)
	return FALSE;
    src.x = 0;
    src.y = (completed + 1) * ENDICONH;
    src.w = ENDICONW;
    src.h = ENDICONH;
    SDL_BlitSurface(endmsgicons, &src, screen, &endmsgloc);
    SDL_UpdateRect(screen, endmsgloc.x, endmsgloc.y, endmsgloc.w, endmsgloc.h);
    return TRUE;
}

/* Display some online help text, either arranged in columns or with
 * illustrations on the side.
 */
int displayhelp(int type, char const *title, void const *text, int textcount,
		int completed)
{
    SDL_Rect		left, right;
    objhelptext const  *objtext;
    char *const	       *tabbedtext;
    int			col, id, i, n;

    cleardisplay();
    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);

    left = listloc;
    left.y += listloc.h;
    left.h = sdlg.font.h;
    puttext(&left, title, -1, 0);
    left = listloc;

    if (type == HELP_TABTEXT) {
	tabbedtext = text;
	col = 0;
	for (i = 0 ; i < textcount ; ++i) {
	    n = strchr(tabbedtext[i], '\t') - tabbedtext[i];
	    if (col < n)
		col = n;
	}
	col = (col + 2) * sdlg.font.w;
	right = left;
	right.x += col;
	right.w -= col;

	for (i = 0 ; i < textcount ; ++i) {
	    n = strchr(tabbedtext[i], '\t') - tabbedtext[i];
	    puttext(&left, tabbedtext[i], n, PT_UPDATERECT);
	    puttext(&right, tabbedtext[i] + n + 1, 0,
			    PT_MULTILINE | PT_UPDATERECT);
	    if (left.y < right.y) {
		left.y = right.y;
		left.h = right.h;
	    } else {
		right.y = left.y;
		right.h = left.h;
	    }
	}
    } else if (type == HELP_OBJECTS) {
	right = left;
	col = sdlg.wtile * 2 + sdlg.font.w;
	right.x += col;
	right.w -= col;
	objtext = text;
	for (i = 0 ; i < textcount ; ++i) {
	    if (objtext[i].isfloor)
		id = objtext[i].item1;
	    else
		id = crtile(objtext[i].item1, EAST);
	    drawopaquetile(left.x + sdlg.wtile, left.y,
			   gettileimage(id, FALSE));
	    if (objtext[i].item2) {
		if (objtext[i].isfloor)
		    id = objtext[i].item2;
		else
		    id = crtile(objtext[i].item2, EAST);
		drawopaquetile(left.x, left.y,
			       gettileimage(id, FALSE));
	    }
	    left.y += sdlg.htile;
	    left.h -= sdlg.htile;
	    puttext(&right, objtext[i].desc, 0, PT_MULTILINE | PT_UPDATERECT);
	    if (left.y < right.y) {
		left.y = right.y;
		left.h = right.h;
	    } else {
		right.y = left.y;
		right.h = left.h;
	    }
	}
    }

    if (SDL_MUSTLOCK(screen))
	SDL_UnlockSurface(screen);
    displayendmessage(completed);

    SDL_UpdateRect(screen, 0, 0, 0, 0);

    return TRUE;
}

/* Create a display surface appropriate to the requirements of the
 * game.
 */
int creategamedisplay(void)
{
    if (!layoutscreen())
	return FALSE;
    if (!setdisplaysize())
	return FALSE;

    return TRUE;
}

/* Initialize a generic display surface capable of displaying text.
 */
int _sdloutputinitialize(void)
{
    if (screenw <= 0 || screenh <= 0) {
	screenw = 640;
	screenh = 480;
    }
    if (!setdisplaysize())
	return FALSE;

    clr_black = SDL_MapRGBA(screen->format, 0, 0, 0, 255);
    clr_white = SDL_MapRGBA(screen->format, 255, 255, 255, 255);
    clr_gray = SDL_MapRGBA(screen->format, 192, 192, 192, 255);
    clr_red = SDL_MapRGBA(screen->format, 255, 0, 0, 255);
    clr_yellow = SDL_MapRGBA(screen->format, 255, 255, 0, 255);

    sdlg.transpixel = SDL_MapRGBA(screen->format, 0, 0, 0, 0);
    if (sdlg.transpixel == clr_black) {
	sdlg.transpixel = 0xFFFFFFFF;
	sdlg.transpixel &= ~(screen->format->Rmask | screen->format->Gmask
						   | screen->format->Bmask);
    }

    sdlg.textcolor = clr_white;
    sdlg.bkgndcolor = clr_black;

    if (!createendmsgicons())
	return FALSE;

    return TRUE;
}
