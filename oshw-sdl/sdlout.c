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

/* The dimensions of the graphic output.
 */
#define	CXSCREEN	640
#define	CYSCREEN	480

/* Space to leave between graphic objects.
 */
#define	CXMARGIN	8
#define	CYMARGIN	8

/* The dimensions of the visible area of the map (in tiles).
 */
#define	CXDISPLAY	9
#define	CYDISPLAY	9

/* The pixel location of the information area's upper-left corner.
 */
#define	XINFO		(CXMARGIN + CXDISPLAY * CXTILE + CXMARGIN)
#define	YINFO		CYMARGIN

/* The pixel dimensions of the information area.
 */
#define	CXINFO		(CXSCREEN - CXMARGIN - XINFO)
#define	CYINFO		(YENDMSG - YINFO)

/* The pixel location of the messages displayed at the end of a game.
 */
#define	XENDMSG		(CXSCREEN - CXMARGIN - CXFONT * 16)
#define	YENDMSG		(CYSCREEN - CYMARGIN - CYFONT * 2)

/* The total number of tile images.
 */
#define	NTILES		(Count_Floors + 4 * Count_Entities)

/* cctiles[]: The pixels of all the tiles.
 * ccpalette[]: The tiles' color palette.
 * (This file is generated automatically.)
 */
#include	"cctiles.c"

/* ccfont: A nice font.
 * (This file is generated automatically.)
 */
#include	"ccfont.c"

/* Flags indicating which tiles have transparent pixels.
 */
static char		transparency[NTILES];

/* The graphic output buffer.
 */
static SDL_Surface     *screen = NULL;

/* Macros for locating a specific tile's pixels.
 */
#define	floortile(id)		(id)
#define	entitydirtile(id, dir)	(Count_Floors + (id) * 4 \
					      + ((0x30210 >> ((dir) * 2)) & 3))
#define	tileptr(tn)		(cctiles + (tn) * CXTILE * CYTILE)

/*
 * Tile initialization.
 */

static void initializetiles(void)
{
    unsigned char const	       *tile;
    int				n, i;

    memset(transparency, FALSE, sizeof transparency);
    for (n = 0 ; n < NTILES ; ++n) {
	tile = tileptr(n);
	for (i = 0 ; i < CXTILE * CYTILE ; ++i) {
	    if (tile[i] == IDX_TRANSPARENT) {
		transparency[n] = TRUE;
		break;
	    }
	}
    }
}

/*
 * Game display functions.
 */

/* Transfer the given tile to another tile-size buffer.
 */
static void copytile(unsigned char *dest, int tn)
{
    unsigned char const	       *src;
    int				n;

    src = tileptr(tn);
    if (transparency[tn]) {
	for (n = 0 ; n < CXTILE * CYTILE ; ++n)
	    if (src[n] != IDX_TRANSPARENT)
		dest[n] = src[n];
    } else {
	memcpy(dest, src, CXTILE * CYTILE);
    }
}

/* Render the given tile to a screen-sized buffer at (xpos, ypos).
 */
static void drawtile(unsigned char *scrbits, int xpos, int ypos, int tn)
{
    unsigned char const	       *tilebits;
    int				x, y;

    scrbits += ypos * CXSCREEN + xpos;
    tilebits = tileptr(tn);
    if (transparency[tn]) {
	for (y = 0 ; y < CYTILE ; ++y, scrbits += CXSCREEN)
	    for (x = 0 ; x < CXTILE ; ++x, ++tilebits)
		if (*tilebits != IDX_TRANSPARENT)
		    scrbits[x] = *tilebits;
    } else {
	for (y = 0 ; y < CYTILE ; ++y, scrbits += CXSCREEN, tilebits += CXTILE)
	    memcpy(scrbits, tilebits, CXTILE);
    }
}

/* Render the given tile to a screen-sized buffer at (xpos, ypos),
 * clipping any pixels that fall outside of the map display.
 */
static void drawclippedtile(unsigned char *scrbits, int xpos, int ypos,
			    unsigned char const *tilebits)
{
    static int const	lclip = CXMARGIN;
    static int const	tclip = CYMARGIN;
    static int const	rclip = CXMARGIN + CXDISPLAY * CXTILE;
    static int const	bclip = CYMARGIN + CYDISPLAY * CYTILE;
    int			x, y, l, t, r, b;

    l = xpos < lclip ? lclip : xpos;
    t = ypos < tclip ? tclip : ypos;
    r = xpos + CXTILE >= rclip ? rclip : xpos + CXTILE;
    b = ypos + CYTILE >= bclip ? bclip : ypos + CYTILE;
    if (l >= r || t >= b)
	return;

    scrbits += t * CXSCREEN;
    tilebits += (t - ypos) * CXTILE - xpos;
    for (y = b - t ; y ; --y, scrbits += CXSCREEN, tilebits += CXTILE)
	for (x = l ; x < r ; ++x)
	    if (tilebits[x] != IDX_TRANSPARENT)
		scrbits[x] = tilebits[x];
}

/* Render the view of the visible area of the map to the output
 * buffer, including all visible creatures, with Chip centered on the
 * display as much as possible.
 */
static void displaymapview(gamestate const *state)
{
    unsigned char	tilebuf[CXTILE * CYTILE];
    creature const     *cr;
    creature const     *chip;
    int			xdisppos, ydisppos;
    int			lmap, tmap, rmap, bmap;
    int			pos, x, y;

    for (chip = state->creatures ; chip->id != Chip ; ++chip) ;
    xdisppos = (chip->pos % CXGRID) * 4;
    ydisppos = (chip->pos / CXGRID) * 4;
    if (chip->moving > 0) {
	switch (chip->dir) {
	  case NORTH:	ydisppos += chip->moving / 2;	break;
	  case WEST:	xdisppos += chip->moving / 2;	break;
	  case SOUTH:	ydisppos -= chip->moving / 2;	break;
	  case EAST:	xdisppos -= chip->moving / 2;	break;
	}
    }
    xdisppos -= 4 * 4;
    ydisppos -= 4 * 4;
    if (xdisppos < 0)
	xdisppos = 0;
    if (ydisppos < 0)
	ydisppos = 0;
    if (xdisppos > (CXGRID - CXDISPLAY) * 4)
	xdisppos = (CXGRID - CXDISPLAY) * 4;
    if (ydisppos > (CYGRID - CYDISPLAY) * 4)
	ydisppos = (CYGRID - CYDISPLAY) * 4;

    lmap = xdisppos / 4;
    tmap = ydisppos / 4;
    rmap = (xdisppos + 3) / 4 + CXDISPLAY;
    bmap = (ydisppos + 3) / 4 + CYDISPLAY;
    for (y = tmap ; y < bmap ; ++y) {
	if (y < 0 || y >= CXGRID)
	    continue;
	for (x = lmap ; x < rmap ; ++x) {
	    if (x < 0 || x >= CXGRID)
		continue;
	    pos = y * CXGRID + x;
	    if (transparency[state->map[pos].floor]) {
		if (transparency[state->map[pos].hfloor])
		    copytile(tilebuf, floortile(Empty));
		copytile(tilebuf, floortile(state->map[pos].hfloor));
	    }
	    copytile(tilebuf, floortile(state->map[pos].floor));
	    drawclippedtile((unsigned char*)screen->pixels,
			    CXMARGIN + (x * CXTILE) - (xdisppos * CXTILE / 4),
			    CYMARGIN + (y * CYTILE) - (ydisppos * CYTILE / 4),
			    tilebuf);
	}
    }

    --lmap;
    --tmap;
    ++rmap;
    ++bmap;
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
	if (x < lmap * 4 || x >= rmap * 4 || y < tmap * 4 || y >= bmap * 4)
	    continue;
	drawclippedtile((unsigned char*)screen->pixels,
			CXMARGIN + (x * CXTILE / 4) - (xdisppos * CXTILE / 4),
			CYMARGIN + (y * CYTILE / 4) - (ydisppos * CYTILE / 4),
			tileptr(entitydirtile(cr->id, cr->dir)));
    }
}

/* Render all the various nuggets of data that comprise the
 * information display to the output buffer.
 */
static void displayinfo(gamestate const *state, int timeleft, int besttime)
{
    SDL_Rect	info;
    char	buf[32];
    int		n, x;

    if (state->game->name && *state->game->name) {
	n = strlen(state->game->name);
	x = (CXDISPLAY * CXTILE - n * CXFONT) / 2;
	if (x < CXMARGIN) {
	    x = CXMARGIN;
	    n = CXGRID * CXTILE / CXFONT;
	}
	_sdlputntext(x, CYMARGIN + CYDISPLAY * CYTILE + CYMARGIN, n,
		     state->game->name);
    }

    info.x = XINFO;
    info.y = YINFO;
    info.w = CXINFO;
    info.h = CYINFO;

    sprintf(buf, "Level %d", state->game->number);
    _sdlputtext(info.x, info.y, buf);
    info.y += CYFONT;

    if (state->game->passwd && *state->game->passwd) {
	sprintf(buf, "Password: %s", state->game->passwd);
	_sdlputtext(info.x, info.y, buf);
    }
    info.y += 2 * CYFONT;

    sprintf(buf, "Chips %3d", state->chipsneeded);
    _sdlputtext(info.x, info.y, buf);
    info.y += CYFONT;

    if (timeleft < 0)
	strcpy(buf, "Time  ---");
    else
	sprintf(buf, "Time  %3d", timeleft);
    _sdlputtext(info.x, info.y, buf);
    info.y += CYFONT;

    if (besttime) {
	if (timeleft < 0)
	    sprintf(buf, "(Best time: %d)", besttime);
	else
	    sprintf(buf, "Best time: %3d", besttime);
	_sdlputtext(info.x, info.y, buf);
    }
    info.y += 2 * CYFONT;

    for (n = 0 ; n < 4 ; ++n) {
	drawtile((unsigned char*)screen->pixels,
		 info.x + n * CXTILE, info.y, floortile(Empty));
	drawtile((unsigned char*)screen->pixels,
		 info.x + n * CXTILE, info.y + CYTILE, floortile(Empty));
	if (state->keys[n])
	    drawtile((unsigned char*)screen->pixels,
		     info.x + n * CXTILE, info.y, floortile(Key_Red + n));
	if (state->boots[n])
	    drawtile((unsigned char*)screen->pixels,
		     info.x + n * CXTILE, info.y + CYTILE,
		     floortile(Boots_Ice + n));
    }
    info.y += 2 * CYTILE + CYFONT;

    if (state->displayflags & DF_SHOWHINT)
	_sdlputmltext(&info, state->game->hinttext);
    else if (state->soundeffect && *state->soundeffect)
	_sdlputtext(info.x, info.y, state->soundeffect);
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

    memset(screen->pixels, IDX_BLACK, CXSCREEN * CYSCREEN);
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

    list.x = CXMARGIN;
    list.y = CYMARGIN + CYFONT + CYMARGIN;
    list.w = CXSCREEN - CXMARGIN - list.x;
    list.h = CYSCREEN - 2 * CYMARGIN - CYFONT - list.y;

    _sdlcreatescroll(&scroll, &list, IDX_BLACK, IDX_LTYELLOW,
		     itemcount, items);
    _sdlscrollsetindex(&scroll, *index);

    for (;;) {
	SDL_FillRect(screen, NULL, IDX_BLACK);
	_sdlputtext(CXMARGIN, CYSCREEN - CYMARGIN - CYFONT, title);
	if (header)
	    _sdlputtext(CXMARGIN, CYMARGIN, header);
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

/* Display some online help text, either arranged in columns or with
 * illustrations on the side.
 */
int displayhelp(int type, char const *title, void const *text, int textcount)
{
    SDL_Rect		help;
    objhelptext const  *objtext;
    char *const	       *tabbedtext;
    int			col;
    int			i, n, y;

    SDL_FillRect(screen, NULL, IDX_BLACK);
    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);

    help.x = CXMARGIN;
    help.y = CYMARGIN;
    help.w = CXSCREEN - 2 * CXMARGIN;
    help.h = CYSCREEN - 3 * CYMARGIN - CYFONT;

    _sdlputtext(CXMARGIN, CYSCREEN - CYMARGIN - CYFONT, title);

    if (type == HELP_TABTEXT) {
	tabbedtext = text;
	col = 0;
	for (i = 0 ; i < textcount ; ++i) {
	    n = strchr(tabbedtext[i], '\t') - tabbedtext[i];
	    if (col < n)
		col = n;
	}
	help.x = CXMARGIN + (col + 4) * CXFONT;
	help.w = CXSCREEN - CXMARGIN - help.x;

	for (i = 0 ; i < textcount ; ++i) {
	    n = strchr(tabbedtext[i], '\t') - tabbedtext[i];
	    _sdlputntext(CXMARGIN, help.y, n, tabbedtext[i]);
	    _sdlputmltext(&help, tabbedtext[i] + n + 1);
	}
    } else if (type == HELP_OBJECTS) {
	help.x += CXTILE * 2 + CXMARGIN;
	help.w -= CXTILE * 2 + CXMARGIN;
	objtext = text;
	for (n = 0 ; n < textcount ; ++n) {
	    if (objtext[n].isfloor) {
		drawtile((unsigned char*)screen->pixels,
			 CXMARGIN + CXTILE, help.y,
			 floortile(objtext[n].item1));
		if (objtext[n].item2)
		    drawtile((unsigned char*)screen->pixels,
			     CXMARGIN, help.y,
			     floortile(objtext[n].item2));
	    } else {
		drawtile((unsigned char*)screen->pixels,
			 CXMARGIN + CXTILE, help.y,
			 floortile(Empty));
		drawtile((unsigned char*)screen->pixels,
			 CXMARGIN + CXTILE, help.y,
			 entitydirtile(objtext[n].item1, EAST));
		if (objtext[n].item2) {
		    drawtile((unsigned char*)screen->pixels,
			     CXMARGIN, help.y,
			     floortile(Empty));
		    drawtile((unsigned char*)screen->pixels,
			     CXMARGIN, help.y,
			     entitydirtile(objtext[n].item2, EAST));
		}
	    }
	    y = help.y;
	    _sdlputmltext(&help, objtext[n].desc);
	    y = CYTILE - (help.y - y);
	    if (y > 0) {
		help.y += y;
		help.h -= y;
	    }
	}
    }

    _sdlputtext(CXSCREEN - CXMARGIN - 25 * CXFONT,
		CYSCREEN - CYMARGIN - CYFONT,
		"Press any key to continue");

    if (SDL_MUSTLOCK(screen))
	SDL_UnlockSurface(screen);

    SDL_UpdateRect(screen, 0, 0, 0, 0);

    return TRUE;
}

/* Display a message appropriate to the end of game play in one corner.
 */
int displayendmessage(int completed)
{
    if (completed > 0) {
	_sdlputtext(XENDMSG, YENDMSG, "Press ENTER");
	_sdlputtext(XENDMSG, YENDMSG + CYFONT, "to continue");
    } else {
	_sdlputtext(XENDMSG, YENDMSG, " \"Bummer\"");
	_sdlputtext(XENDMSG, YENDMSG + CYFONT, "Press ENTER");
    }

    SDL_UpdateRect(screen, 0, 0, 0, 0);

    return TRUE;
}

/* Prepare for graphic output.
 */
int _sdloutputinitialize(void)
{
    if (sizeof cctiles < NTILES * CXTILE * CYTILE)
	die("Tile array has size %u, expected at least %u",
	    sizeof cctiles, NTILES * CXTILE * CYTILE);

    if (!(screen = SDL_SetVideoMode(CXSCREEN, CYSCREEN, 8, SDL_HWSURFACE)))
	die("Cannot open 640x480x8 display: %s\n", SDL_GetError());

    SDL_SetPalette(screen, SDL_LOGPAL, ccpalette, 0,
		   sizeof ccpalette / sizeof *ccpalette);

    ccfont.color = IDX_WHITE;
    _sdlsettextsurface(screen);
    _sdlsettextfont(&ccfont);

    initializetiles();

    return TRUE;
}
