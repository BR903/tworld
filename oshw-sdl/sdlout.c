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
#include	"sdltext.h"
#include	"sdltile.h"

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

    if (sdlg.wtile <= 0 || sdlg.htile <= 0)
	return FALSE;

    xdisplay = CXMARGIN;
    ydisplay = CYMARGIN;
    cxdisplay = NXTILES * sdlg.wtile;
    cydisplay = NYTILES * sdlg.htile;

    xtitle = xdisplay;
    ytitle = ydisplay + cydisplay + CYMARGIN;
    cxtitle = cxdisplay;
    cytitle = ccfont.h;

    cx = 4 * sdlg.wtile;
    if (cx < 18 * ccfont.w)
	cx = 18 * ccfont.w;

    xinfo = xdisplay + cxdisplay + CXMARGIN;
    yinfo = CYMARGIN;
    cxinfo = cx;
    cyinfo = 6 * ccfont.h;

    xinventory = xinfo;
    yinventory = yinfo + cyinfo + CYMARGIN;
    cxinventory = cx;
    cyinventory = 2 * sdlg.htile;

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
    Uint32     *dest;
    void const *endsrc;

    dest = (Uint32*)screen->pixels + ypos * cxscreen + xpos;
    endsrc = src + sdlg.cptile;
    for ( ; src != endsrc ; src += sdlg.wtile, dest += cxscreen)
	memcpy(dest, src, sdlg.wtile * sizeof *dest);
}

#if 0
static void drawtransptile(int xpos, int ypos, Uint32 const *src)
{
    Uint32     *dest;
    void const *endsrc;
    int		x;

    dest = (Uint32*)screen->pixels + ypos * cxscreen + xpos;
    endsrc = src + sdlg.cptile;
    for ( ; src != endsrc ; src += sdlg.wtile, dest += cxscreen)
	for (x = 0 ; x < sdlg.wtile ; ++x)
	    if (src[x] != (Uint32)sdlg.transpixel)
		dest[x] = src[x];
}
#endif

static void drawopaquetileclipped(int xpos, int ypos, Uint32 const *src)
{
    Uint32     *dest;
    int		lclip = xdisplay;
    int		tclip = ydisplay;
    int		rclip = xdisplay + cxdisplay;
    int		bclip = ydisplay + cydisplay;
    int		y;

    if (xpos > lclip)			lclip = xpos;
    if (ypos > tclip)			tclip = ypos;
    if (xpos + sdlg.wtile < rclip)	rclip = xpos + sdlg.wtile;
    if (ypos + sdlg.htile < bclip)	bclip = ypos + sdlg.htile;
    if (lclip >= rclip || tclip >= bclip)
	return;
    dest = (Uint32*)screen->pixels + tclip * cxscreen + lclip;
    src += (tclip - ypos) * sdlg.wtile + lclip - xpos;
    for (y = bclip - tclip ; y ; --y, dest += cxscreen, src += sdlg.wtile)
	memcpy(dest, src, (rclip - lclip) * sizeof *dest);
}

static void drawtransptileclipped(int xpos, int ypos, Uint32 const *src)
{
    Uint32     *dest;
    int		lclip = xdisplay;
    int		tclip = ydisplay;
    int		rclip = xdisplay + cxdisplay;
    int		bclip = ydisplay + cydisplay;
    int		x, y;

    if (xpos > lclip)			lclip = xpos;
    if (ypos > tclip)			tclip = ypos;
    if (xpos + sdlg.wtile < rclip)	rclip = xpos + sdlg.wtile;
    if (ypos + sdlg.htile < bclip)	bclip = ypos + sdlg.htile;
    if (lclip >= rclip || tclip >= bclip)
	return;
    dest = (Uint32*)screen->pixels + tclip * cxscreen;
    src += (tclip - ypos) * sdlg.wtile - xpos;
    for (y = bclip - tclip ; y ; --y, dest += cxscreen, src += sdlg.wtile)
	for (x = lclip ; x < rclip ; ++x)
	    if (src[x] != (Uint32)sdlg.transpixel)
		dest[x] = src[x];
}

/*
 * Game display functions.
 */

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
	    drawopaquetileclipped(xdisplay + (x * sdlg.wtile)
					   - (xdisppos * sdlg.wtile / 4),
				  ydisplay + (y * sdlg.htile)
					   - (ydisppos * sdlg.htile / 4),
				  _sdlgetcellimage(state->map[pos].top.id,
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
	drawtransptileclipped(xdisplay + (x * sdlg.wtile / 4)
				       - (xdisppos * sdlg.wtile / 4),
			      ydisplay + (y * sdlg.htile / 4)
				       - (ydisppos * sdlg.htile / 4),
			      _sdlgetcreatureimage(cr->id, cr->dir, 0));
    }
    SDL_SetClipRect(screen, NULL);
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
	drawopaquetile(xinventory + n * sdlg.wtile, yinventory,
		_sdlgettileimage(state->keys[n] ? Key_Red + n : Empty, FALSE));
	drawopaquetile(xinventory + n * sdlg.wtile, yinventory + sdlg.htile,
		_sdlgettileimage(state->boots[n] ? Boots_Ice + n : Empty,
				 FALSE));
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
	help.x += sdlg.wtile * 2 + ccfont.w;
	help.w -= sdlg.wtile * 2 + ccfont.w;
	objtext = text;
	for (n = 0 ; n < textcount ; ++n) {
	    if (objtext[n].isfloor)
		id = floortile(objtext[n].item1);
	    else
		id = entitydirtile(objtext[n].item1, EAST);
	    drawopaquetile(xlist + sdlg.wtile, help.y,
			   _sdlgettileimage(id, FALSE));
	    if (objtext[n].item2) {
		if (objtext[n].isfloor)
		    id = floortile(objtext[n].item2);
		else
		    id = entitydirtile(objtext[n].item2, EAST);
		drawopaquetile(xlist, help.y, _sdlgettileimage(id, FALSE));
	    }
	    y = help.y;
	    _sdlputmltext(&help, objtext[n].desc);
	    y = sdlg.htile - (help.y - y);
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

    sdlg.transpixel = SDL_MapRGBA(screen->format, 0, 0, 0, 0);
    if (sdlg.transpixel == clr_black) {
	sdlg.transpixel = 0xFFFFFFFF;
	sdlg.transpixel &= ~(screen->format->Rmask | screen->format->Gmask
						   | screen->format->Bmask);
    }
    ccfont.color = clr_white;
    ccfont.bkgnd = clr_black;
    sdlg.font = &ccfont;

    if (!createendmsgicons())
	return FALSE;

    return TRUE;
}
