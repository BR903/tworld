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
#include	"sdlres.h"
#include	"sdltext.h"

/* Space to leave between graphic objects.
 */
#define	CXMARGIN	8
#define	CYMARGIN	8

/* Size of the end-message icons.
 */
#define	CXENDICON	16
#define	CYENDICON	10

/* The dimensions of the visible area of the map (in tiles).
 */
#define	NXTILES		9
#define	NYTILES		9

/* Macros for locating a specific tile's pixels.
 */
#define	floortile(id)		(id)
#define	entitydirtile(id, dir)	((id) + ((0x30210 >> ((dir) * 2)) & 3))

/* ccfont: A nice font.
 * (This file is generated automatically.)
 */
#include	"ccfont.c"

/* The graphic output buffer.
 */
static SDL_Surface     *screen = NULL;

/* Some prompting icons.
 */
static SDL_Surface     *endmsgicons = NULL;

/* Special pixel values.
 */
static Uint32		clr_black, clr_white, clr_gray, clr_red, clr_yellow;
static Uint32		clr_transparent;

/* Coordinates specifying the layout of the screen elements.
 */
static int		xdisplay, ydisplay, cxdisplay, cydisplay;
static int		xtitle, ytitle, cxtitle, cytitle;
static int		xinfo, yinfo, cxinfo, cyinfo;
static int		xinventory, yinventory, cxinventory, cyinventory;
static int		xonomatopoeia, yonomatopoeia,
			cxonomatopoeia, cyonomatopoeia;
static int		xhint, yhint, cxhint, cyhint;
static int		xendmsg, yendmsg, cxendmsg, cyendmsg;
static int		xlist, ylist, cxlist, cylist;
static int		cxscreen, cyscreen;

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
					       CXENDICON, 3 * CYENDICON,
					       8, CXENDICON, 0, 0, 0, 0);
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
    xlist = CXMARGIN;
    ylist = CYMARGIN;
    cxlist = (cxscreen - CXMARGIN) - xlist;
    cylist = (cyscreen - CYMARGIN - ccfont.h) - ylist;
}

/* Calculate the positions of all the elements of the game display.
 */
static int layoutscreen(void)
{
    int	cx;

    if (cxtile <= 0 || cytile <= 0)
	return FALSE;

    xdisplay = CXMARGIN;
    ydisplay = CYMARGIN;
    cxdisplay = NXTILES * cxtile;
    cydisplay = NYTILES * cytile;

    xtitle = xdisplay;
    ytitle = ydisplay + cydisplay + CYMARGIN;
    cxtitle = cxdisplay;
    cytitle = ccfont.h;

    cx = 4 * cxtile;
    if (cx < 18 * ccfont.w)
	cx = 18 * ccfont.w;

    xinfo = xdisplay + cxdisplay + CXMARGIN;
    yinfo = CYMARGIN;
    cxinfo = cx;
    cyinfo = 6 * ccfont.h;

    xinventory = xinfo;
    yinventory = yinfo + cyinfo + CYMARGIN;
    cxinventory = cx;
    cyinventory = 2 * cytile;

    cxendmsg = CXENDICON;
    cyendmsg = CYENDICON;
    xendmsg = xinfo + cxinfo - cxendmsg;
    yendmsg = ytitle + cytitle - cyendmsg;

    xonomatopoeia = xinfo;
    yonomatopoeia = ydisplay + cydisplay - ccfont.h;
    cxonomatopoeia = 6 * ccfont.w;
    cyonomatopoeia = ccfont.h;

    xhint = xinfo;
    yhint = yinventory + cyinventory + CYMARGIN;
    cxhint = cx;
    cyhint = yendmsg - CYMARGIN - yhint;

    cxscreen = xinfo + cxinfo + CXMARGIN;
    cyscreen = ytitle + cytitle + CYMARGIN;
    cx = 48 * ccfont.w + 2 * CXMARGIN;
    if (cxscreen < cx)
	cxscreen = cx;

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
    if (!(screen = SDL_SetVideoMode(cxscreen, cyscreen, 32, SDL_HWSURFACE)))
	die("Cannot open %dx%d display: %s\n",
	    cxscreen, cyscreen, SDL_GetError());
    _sdlsettextsurface(screen);
    _sdlsettileformat(screen->format);
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
 * Tile functions.
 */

/*
 * Game display functions.
 */

static Uint32 const *getcellimageptr(int top, int bot)
{
    Uint32     *dest;
    Uint32     *src;
    int		n;

    src = cctiles + top * cxtile * cytile;
    if (bot == Nothing || bot == Empty || !transparency[top])
	return src;
    dest = cctiles + Overlay_Buffer * cxtile * cytile;
    memcpy(dest, cctiles + bot * cxtile * cytile,
	   cxtile * cytile * sizeof *dest);
    src += NTILES * cxtile * cytile;
    for (n = 0 ; n < cxtile * cytile ; ++n)
	if (src[n] != clr_transparent)
	    dest[n] = src[n];
    return dest;
}

static Uint32 const *getcreatureimageptr(int id, int dir)
{
    int	tileid;

    tileid = entitydirtile(id, dir);
    if (transparency[tileid])
	tileid += NTILES;
    return cctiles + tileid * cxtile * cytile;
}

/* Render the given tile to a screen-sized buffer at (xpos, ypos).
 */
static void drawtile(Uint32 *scrbits, int xpos, int ypos, int tn)
{
    Uint32     *tilebits;
    int		x, y;

    scrbits += ypos * cxscreen + xpos;
    tilebits = cctiles + tn * cxtile * cytile;
    if (transparency[tn]) {
	for (y = 0 ; y < cytile ; ++y, scrbits += cxscreen)
	    for (x = 0 ; x < cxtile ; ++x, ++tilebits)
		if (*tilebits != clr_transparent)
		    scrbits[x] = *tilebits;
    } else {
	for (y = 0 ; y < cytile ; ++y, scrbits += cxscreen, tilebits += cxtile)
	    memcpy(scrbits, tilebits, cxtile * sizeof *scrbits);
    }
}

/* Render the given tile to a screen-sized buffer at (xpos, ypos),
 * clipping any pixels that fall outside of the map display.
 */
static void drawclippedtile(Uint32 *scrbits, int xpos, int ypos,
			    Uint32 const *tilebits)
{
    int	lclip = xdisplay;
    int	tclip = ydisplay;
    int	rclip = xdisplay + cxdisplay;
    int	bclip = ydisplay + cydisplay;
    int	x, y;

    lclip = xpos < lclip ? lclip : xpos;
    tclip = ypos < tclip ? tclip : ypos;
    rclip = xpos + cxtile >= rclip ? rclip : xpos + cxtile;
    bclip = ypos + cytile >= bclip ? bclip : ypos + cytile;
    if (lclip >= rclip || tclip >= bclip)
	return;

    scrbits += tclip * cxscreen;
    tilebits += (tclip - ypos) * cxtile - xpos;
    for (y = bclip - tclip ; y ; --y, scrbits += cxscreen, tilebits += cxtile)
	for (x = lclip ; x < rclip ; ++x)
	    if (tilebits[x] != clr_transparent)
		scrbits[x] = tilebits[x];
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
    int			pos, overlaid, x, y;

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
	    overlaid = FALSE;
#if 0
	    if (transparency[state->map[pos].top.id]) {
		if (transparency[state->map[pos].bot.id])
		    copytile(tilebuf, floortile(Empty));
		copytile(tilebuf, floortile(state->map[pos].bot.id));
		if (state->map[pos].bot.id != Empty)
		    overlaid = TRUE;
	    }
	    copytile(tilebuf, floortile(state->map[pos].top.id));
	    if (overlaid)
		memcpy(tileptr(floortile(Overlay_Buffer)),
		       tilebuf, sizeof tilebuf);
	    drawclippedtile((Uint32*)screen->pixels,
			    xdisplay + (x * cxtile) - (xdisppos * cxtile / 4),
			    ydisplay + (y * cytile) - (ydisppos * cytile / 4),
			    tilebuf);
#else
	    drawclippedtile((Uint32*)screen->pixels,
			    xdisplay + (x * cxtile) - (xdisppos * cxtile / 4),
			    ydisplay + (y * cytile) - (ydisppos * cytile / 4),
			    getcellimageptr(state->map[pos].top.id,
					    state->map[pos].bot.id));
#endif
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
#if 0
	drawclippedtile((Uint32*)screen->pixels,
			xdisplay + (x * cxtile / 4) - (xdisppos * cxtile / 4),
			ydisplay + (y * cytile / 4) - (ydisppos * cytile / 4),
			tileptr(entitydirtile(cr->id, cr->dir)));
#else
	drawclippedtile((Uint32*)screen->pixels,
			xdisplay + (x * cxtile / 4) - (xdisppos * cxtile / 4),
			ydisplay + (y * cytile / 4) - (ydisppos * cytile / 4),
			getcreatureimageptr(cr->id, cr->dir));
#endif
    }
}

/* Render all the various nuggets of data that comprise the
 * information display to the output buffer.
 */
static void displayinfo(gamestate const *state, int timeleft, int besttime)
{
    SDL_Rect	text;
    char	buf[32];
    int		color;
    int		n, x, y;

    if (state->game->name && *state->game->name) {
	n = strlen(state->game->name);
	x = (cxtitle - n * ccfont.w) / 2;
	if (x < 0) {
	    x = 0;
	    n = cxtitle / ccfont.w;
	}
	_sdlputntext(xtitle + x, ytitle, n, state->game->name);
    }

    y = yinfo;

    sprintf(buf, "Level %d", state->game->number);
    _sdlputtext(xinfo, y, buf);
    y += ccfont.h;

    if (state->game->passwd && *state->game->passwd) {
	sprintf(buf, "Password: %s", state->game->passwd);
	_sdlputtext(xinfo, y, buf);
    }
    y += 2 * ccfont.h;

    sprintf(buf, "Chips %3d", state->chipsneeded);
    _sdlputtext(xinfo, y, buf);
    y += ccfont.h;

    if (timeleft < 0)
	strcpy(buf, "Time  ---");
    else
	sprintf(buf, "Time  %3d", timeleft);
    _sdlputtext(xinfo, y, buf);
    y += ccfont.h;

    if (besttime) {
	if (timeleft < 0)
	    sprintf(buf, "(Best time: %d)", besttime);
	else
	    sprintf(buf, "Best time: %3d", besttime);
	color = ccfont.color;
	if (state->game->replacebest)
	    ccfont.color = clr_gray;
	_sdlputtext(xinfo, y, buf);
	ccfont.color = color;
    }

    for (n = 0 ; n < 4 ; ++n) {
	drawtile((Uint32*)screen->pixels,
		 xinventory + n * cxtile, yinventory,
		 floortile(Empty));
	drawtile((Uint32*)screen->pixels,
		 xinventory + n * cxtile, yinventory + cytile,
		 floortile(Empty));
	if (state->keys[n])
	    drawtile((Uint32*)screen->pixels,
		     xinventory + n * cxtile, yinventory,
		     floortile(Key_Red + n));
	if (state->boots[n])
	    drawtile((Uint32*)screen->pixels,
		     xinventory + n * cxtile, yinventory + cytile,
		     floortile(Boots_Ice + n));
    }

    text.x = xhint;
    text.y = yhint;
    text.w = cxhint;
    text.h = cyhint;
    if (state->statusflags & SF_INVALID)
	_sdlputmltext(&text, "This level cannot be played.");
    else if (state->statusflags & SF_SHOWHINT)
	_sdlputmltext(&text, state->game->hinttext);

    if ((state->statusflags & SF_ONOMATOPOEIA) && state->soundeffects)
	_sdlputtext(xonomatopoeia, yonomatopoeia,
		    getonomatopoeia(state->soundeffects));
}

/*
 * The exported functions.
 */

/* Display the current state of the game.
 */
int displaygame(void const *_state, int timeleft, int besttime)
{
    gamestate const    *state = _state;

    SDL_FillRect(screen, NULL, clr_black);
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
    SDL_Rect	list;
    scrollinfo	scroll;
    int		n;

    list.x = xlist;
    list.y = ylist;
    list.w = cxlist;
    list.h = cylist;
    if (header) {
	list.y += ccfont.h;
	list.h -= ccfont.h;
    }

    _sdlcreatescroll(&scroll, &list, clr_yellow, itemcount, items);
    _sdlscrollsetindex(&scroll, *index);

    for (;;) {
	SDL_FillRect(screen, NULL, clr_black);
	_sdlputtext(xlist, ylist + cylist, title);
	if (header)
	    _sdlputtext(xlist, ylist, header);
	_sdlscrollredraw(&scroll);
	SDL_UpdateRect(screen, 0, 0, 0, 0);
	n = 0;
	if (!(*inputcallback)(&n))
	    break;
	_sdlscrollmove(&scroll, n);
    }
    if (n)
	*index = _sdlscrollindex(&scroll);
    return n;
}

/* Display a message appropriate to the end of game play in one corner.
 */
int displayendmessage(int completed)
{
    SDL_Rect	src, dest;

    if (!endmsgicons)
	return FALSE;
    src.x = 0;
    src.y = (completed + 1) * CYENDICON;
    src.w = CXENDICON;
    src.h = CYENDICON;
    dest.x = xendmsg;
    dest.y = yendmsg;
    dest.w = cxendmsg;
    dest.h = cyendmsg;
    SDL_BlitSurface(endmsgicons, &src, screen, &dest);
    SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);
    return TRUE;
}

/* Display some online help text, either arranged in columns or with
 * illustrations on the side.
 */
int displayhelp(int type, char const *title, void const *text, int textcount,
		int completed)
{
    SDL_Rect		help;
    objhelptext const  *objtext;
    char *const	       *tabbedtext;
    int			col, id;
    int			i, n, y;

    SDL_FillRect(screen, NULL, clr_black);
    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);

    help.x = xlist;
    help.y = ylist;
    help.w = cxlist;
    help.h = cylist;

    _sdlputtext(xlist, ylist + cylist, title);

    if (type == HELP_TABTEXT) {
	tabbedtext = text;
	col = 0;
	for (i = 0 ; i < textcount ; ++i) {
	    n = strchr(tabbedtext[i], '\t') - tabbedtext[i];
	    if (col < n)
		col = n;
	}
	help.x += (col + 2) * ccfont.w;
	help.w -= help.x;

	for (i = 0 ; i < textcount ; ++i) {
	    n = strchr(tabbedtext[i], '\t') - tabbedtext[i];
	    _sdlputntext(xlist, help.y, n, tabbedtext[i]);
	    _sdlputmltext(&help, tabbedtext[i] + n + 1);
	}
    } else if (type == HELP_OBJECTS) {
	help.x += cxtile * 2 + ccfont.w;
	help.w -= cxtile * 2 + ccfont.w;
	objtext = text;
	for (n = 0 ; n < textcount ; ++n) {
	    if (objtext[n].isfloor)
		id = floortile(objtext[n].item1);
	    else
		id = entitydirtile(objtext[n].item1, EAST);
	    if (transparency[id])
		drawtile((Uint32*)screen->pixels,
			 xlist + cxtile, help.y, floortile(Empty));
	    drawtile((Uint32*)screen->pixels, xlist + cxtile, help.y, id);
	    if (objtext[n].item2) {
		if (objtext[n].isfloor)
		    id = floortile(objtext[n].item2);
		else
		    id = entitydirtile(objtext[n].item2, EAST);
		if (transparency[id])
		    drawtile((Uint32*)screen->pixels,
			     xlist, help.y, floortile(Empty));
		drawtile((Uint32*)screen->pixels, xlist, help.y, id);
	    }
	    y = help.y;
	    _sdlputmltext(&help, objtext[n].desc);
	    y = cytile - (help.y - y);
	    if (y > 0) {
		help.y += y;
		help.h -= y;
	    }
	}
    }

    displayendmessage(completed);

    if (SDL_MUSTLOCK(screen))
	SDL_UnlockSurface(screen);

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
    if (cxscreen <= 0 || cyscreen <= 0) {
	cxscreen = 640;
	cyscreen = 480;
    }
    if (!setdisplaysize())
	return FALSE;

    clr_black = SDL_MapRGBA(screen->format, 0, 0, 0, 255);
    clr_white = SDL_MapRGBA(screen->format, 255, 255, 255, 255);
    clr_gray = SDL_MapRGBA(screen->format, 192, 192, 192, 255);
    clr_red = SDL_MapRGBA(screen->format, 255, 0, 0, 255);
    clr_yellow = SDL_MapRGBA(screen->format, 255, 255, 0, 255);
    clr_transparent = SDL_MapRGBA(screen->format, 0, 0, 0, 0);
    if (clr_transparent == clr_black) {
	clr_transparent = 0xFFFFFFFF;
	clr_transparent &= ~(screen->format->Rmask | screen->format->Gmask
						   | screen->format->Bmask);
    }
    ccfont.color = clr_white;
    ccfont.bkgnd = clr_black;
    _sdlsettextfont(&ccfont);
    _sdlsettransparentcolor(clr_transparent);

    if (!createendmsgicons())
	return FALSE;

    return TRUE;
}
