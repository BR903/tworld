/* sdlout.c: Creating the program's displays.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
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

/* Macro to erase a rectangle.
 */
#define	fillrect(r)		(puttext((r), NULL, 0, PT_MULTILINE))

/* The graphic output buffer.
 */
static SDL_Surface     *screen = NULL;

/* Some prompting icons.
 */
static SDL_Surface     *prompticons = NULL;

/* Special pixel values.
 */
static Uint32		clr_black, clr_white, clr_gray, clr_dkgray, clr_yellow;

/* Coordinates specifying the layout of the screen elements.
 */
static SDL_Rect		titleloc, infoloc, invloc, hintloc, promptloc;
static SDL_Rect		displayloc;
static int		screenw, screenh;

/* Display a row of tab-delimited columnar texts.
 */
static void drawtextrow(SDL_Rect *rects, char const *text, int const *flags,
			int count)
{
    char const *str;
    int		lowest;
    int		x, n;

    lowest = 0;
    for (x = 0 ; x < count ; ++x) {
	str = strchr(text, '\t');
	if (str)
	    n = str - text;
	else
	    n = strlen(text);
	puttext(rects + x, text, n, (flags ? flags[x] : 0) | PT_UPDATERECT);
	if (rects[lowest].y < rects[x].y)
	    lowest = x;
	if (!str)
	    break;
	text = str + 1;
    }
    for (x = 0 ; x < count ; ++x) {
	rects[x].y = rects[lowest].y;
	rects[x].h = rects[lowest].h;
    }
}

/* Create some simple icons used to prompt the user.
 */
static int createprompticons(void)
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

    if (!prompticons) {
	prompticons = SDL_CreateRGBSurfaceFrom(iconpixels,
					       ENDICONW, 3 * ENDICONH,
					       8, ENDICONW, 0, 0, 0, 0);
	if (!prompticons) {
	    warn("couldn't create SDL surface: %s", SDL_GetError());
	    return FALSE;
	}
	SDL_GetRGB(sdlg.bkgndcolor, screen->format,
		   &prompticons->format->palette->colors[0].r,
		   &prompticons->format->palette->colors[0].g,
		   &prompticons->format->palette->colors[0].b);
	SDL_GetRGB(sdlg.halfcolor, screen->format,
		   &prompticons->format->palette->colors[1].r,
		   &prompticons->format->palette->colors[1].g,
		   &prompticons->format->palette->colors[1].b);
    }
    return TRUE;
}

/*
 *
 */

/* Calculate the positions of all the elements of the game display.
 */
static int layoutscreen(void)
{
    static char const  *scoretext = "888  DRAWN AND QUARTERED"
				    "   8,888  888,888  888,888";
    static char const  *hinttext = "Total Score  8888888";
    int			fullw, infow, texth;

    if (sdlg.wtile <= 0 || sdlg.htile <= 0)
	return FALSE;

    puttext(&displayloc, scoretext, -1, PT_CALCSIZE);
    fullw = displayloc.w;
    texth = displayloc.h;
    puttext(&displayloc, hinttext, -1, PT_CALCSIZE);
    infow = displayloc.w;

    displayloc.x = MARGINW;
    displayloc.y = MARGINH;
    displayloc.w = NXTILES * sdlg.wtile;
    displayloc.h = NYTILES * sdlg.htile;

    titleloc.x = displayloc.x;
    titleloc.y = displayloc.y + displayloc.h + MARGINH;
    titleloc.w = displayloc.w;
    titleloc.h = texth;

    infoloc.x = displayloc.x + displayloc.w + MARGINW;
    infoloc.y = MARGINH;
    infoloc.w = 4 * sdlg.wtile;
    if (infoloc.w < infow)
	infoloc.w = infow;
    infoloc.h = 6 * texth;

    invloc.x = infoloc.x;
    invloc.y = infoloc.y + infoloc.h + MARGINH;
    invloc.w = 4 * sdlg.wtile;
    invloc.h = 2 * sdlg.htile;

    screenw = infoloc.x + infoloc.w + MARGINW;
    if (screenw < fullw)
	screenw = fullw;
    screenh = titleloc.y + titleloc.h + MARGINH;

    promptloc.x = screenw - MARGINW - ENDICONW;
    promptloc.y = screenh - MARGINH - ENDICONH;
    promptloc.w = ENDICONW;
    promptloc.h = ENDICONH;

    sdlg.textsfxrect.x = infoloc.x;
    sdlg.textsfxrect.y = titleloc.y;
    sdlg.textsfxrect.w = promptloc.x - sdlg.textsfxrect.x - MARGINW;
    sdlg.textsfxrect.h = titleloc.h;

    hintloc.x = infoloc.x;
    hintloc.y = invloc.y + invloc.h + MARGINH;
    hintloc.w = screenw - MARGINW - hintloc.x;
    hintloc.h = sdlg.textsfxrect.y - hintloc.y;
    if (hintloc.y + hintloc.h + MARGINH > promptloc.y)
	hintloc.h = promptloc.y - MARGINH - hintloc.y;

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

    sdlg.screen = screen;

    return TRUE;
}

/* Wipe the display.
 */
void cleardisplay(void)
{
    SDL_FillRect(sdlg.screen, NULL, sdlg.bkgndcolor);
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
    int		color, hcolor;
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
	hcolor = sdlg.halfcolor;
	if (state->game->replacebest) {
	    sdlg.textcolor = clr_gray;
	    sdlg.halfcolor = clr_dkgray;
	}
	puttext(&rect, buf, -1, PT_UPDATERECT);
	sdlg.textcolor = color;
	sdlg.halfcolor = hcolor;
    }
    fillrect(&rect);

    for (n = 0 ; n < 4 ; ++n) {
	drawopaquetile(invloc.x + n * sdlg.wtile, invloc.y,
		gettileimage(state->keys[n] ? Key_Red + n : Empty, FALSE));
	drawopaquetile(invloc.x + n * sdlg.wtile, invloc.y + sdlg.htile,
		gettileimage(state->boots[n] ? Boots_Ice + n : Empty, FALSE));
    }

    if (state->statusflags & SF_INVALID)
	puttext(&hintloc, "This level cannot be played.", -1, PT_MULTILINE);
    else if (state->statusflags & SF_SHOWHINT)
	puttext(&hintloc, state->game->hinttext, -1, PT_MULTILINE | PT_CENTER);
    else
	fillrect(&hintloc);

    fillrect(&promptloc);
}

/* Display an appropriate prompt in one corner.
 */
static int displayprompticon(int completed)
{
    SDL_Rect	src;

    if (!prompticons)
	return FALSE;
    src.x = 0;
    src.y = (completed + 1) * ENDICONH;
    src.w = ENDICONW;
    src.h = ENDICONH;
    SDL_BlitSurface(prompticons, &src, screen, &promptloc);
    SDL_UpdateRect(screen, promptloc.x, promptloc.y, promptloc.w, promptloc.h);
    return TRUE;
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

int displayendmessage(int basescore, int timescore, int totalscore,
		      int completed)
{
    char	buf[32];
    SDL_Rect	rect;
    int		n;

    if (completed >= 0) {
	rect = hintloc;
	puttext(&rect, "Level Completed", -1, PT_CENTER | PT_UPDATERECT);
	puttext(&rect, "", 0, PT_CENTER | PT_UPDATERECT);
	n = sprintf(buf, "Time Bonus %04d", timescore);
	puttext(&rect, buf, n, PT_CENTER | PT_UPDATERECT);
	n = sprintf(buf, "Level Bonus %05d", basescore);
	puttext(&rect, buf, n, PT_CENTER | PT_UPDATERECT);
	n = sprintf(buf, "Level Score %05d", timescore + basescore);
	puttext(&rect, buf, n, PT_CENTER | PT_UPDATERECT);
	n = sprintf(buf, "Total Score %07d", totalscore);
	puttext(&rect, buf, n, PT_CENTER | PT_UPDATERECT);
	fillrect(&rect);
	SDL_UpdateRect(screen, hintloc.x, hintloc.y, hintloc.w, hintloc.h);
    }
    return displayprompticon(completed);
}

/* Display some online help text, either arranged in columns, or with
 * illustrations on the side.
 */
int displayhelp(int type, char const *title, void const *text, int textcount,
		int completed)
{
    objhelptext const  *objtext;
    char *const	       *tabbedtext;
    SDL_Rect		left, right;
    SDL_Rect		cols[8];
    int			flags[8];
    int			colcount;
    int			col, id, i;

    cleardisplay();
    if (SDL_MUSTLOCK(screen))
	SDL_LockSurface(screen);

    left.x = MARGINW;
    left.y = screenh - MARGINH - sdlg.font.h;
    left.w = screenw - 2 * MARGINW;
    left.h = sdlg.font.h;
    puttext(&left, title, -1, 0);
    left.h = left.y - MARGINH;
    left.y = MARGINH;

    if (type == HELP_TABTEXT) {
	tabbedtext = text;
	colcount = measurecolumns(tabbedtext, cols, textcount);
	col = MARGINW;
	for (i = 0 ; i < colcount ; ++i) {
	    cols[i].x = col;
	    cols[i].y = left.y;
	    cols[i].h = left.h;
	    col += cols[i].w + MARGINW;
	    flags[i] = 0;
	}
	if (col > screenw) {
	    while (cols[colcount - 1].x >= screenw - MARGINW)
		--colcount;
	    cols[colcount - 1].w = screenw - MARGINW - cols[colcount - 1].x;
	}
	flags[colcount - 1] = PT_MULTILINE;
	for (i = 0 ; i < textcount ; ++i)
	    drawtextrow(cols, tabbedtext[i], flags, colcount);
    } else if (type == HELP_OBJECTS) {
	right = left;
	col = sdlg.wtile * 2 + MARGINW;
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
	    puttext(&right, objtext[i].desc, -1,
			    PT_MULTILINE | PT_UPDATERECT);
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
    displayprompticon(completed);

    SDL_UpdateRect(screen, 0, 0, 0, 0);

    return TRUE;
}

/* Display a scrollable list, calling the callback function to manage
 * the selection until it returns FALSE.
 */
int displaylist(char const *title, char const **items, int itemcount, int *idx,
		int columncount, int const *just, int (*inputcallback)(int*))
{
    SDL_Rect	area;
    SDL_Rect   *columns;
    SDL_Rect   *colstmp;
    Uint32	textcolor;
    int	       *flags;
    int		linecount, topitem, index;
    int		widest;
    int		n, x;

    cleardisplay();
    area.x = MARGINW;
    area.y = screenh - MARGINH - sdlg.font.h;
    area.w = screenw - 2 * MARGINW;
    area.h = sdlg.font.h;
    puttext(&area, title, -1, 0);
    area.h = area.y - MARGINH;
    area.y = MARGINH;

    linecount = area.h / sdlg.font.h - 1;
    topitem = 0;
    n = columncount * sizeof(SDL_Rect);
    columns = malloc(n);
    colstmp = malloc(n);
    if (!columns || !colstmp)
	memerrexit();
    measurecolumns(items, columns, itemcount);
    --itemcount;
    x = area.x;
    widest = 0;
    for (n = 0 ; n < columncount ; ++n) {
	columns[n].y = area.y;
	columns[n].h = area.h;
	columns[n].x = x;
	x += columns[n].w + MARGINW;
	if (columns[n].w > columns[widest].w)
	    widest = n;
    }
    x -= area.x;
    if (x > area.w) {
	x -= area.w;
	if (columns[widest].w < x)
	    die("Can't fit this fucking table thing in the window");
	columns[widest].w -= x;
	for (n = widest + 1 ; n < columncount ; ++n)
	    columns[n].x -= x;
    }

    textcolor = sdlg.textcolor;

    flags = NULL;
    if (just) {
	flags = malloc(columncount * sizeof *flags);
	for (n = 0 ; n < columncount ; ++n)
	    flags[n] = just[n] ? just[n] > 0 ? PT_RIGHT : 0 : PT_CENTER;
    }

    n = *idx;
    for (;;) {
	switch (n) {
	  case SCROLL_NOP:						break;
	  case SCROLL_UP:		--index;			break;
	  case SCROLL_DN:		++index;			break;
	  case SCROLL_HALFPAGE_UP:	index -= (linecount + 1) / 2;	break;
	  case SCROLL_HALFPAGE_DN:	index += (linecount + 1) / 2;	break;
	  case SCROLL_PAGE_UP:		index -= linecount;		break;
	  case SCROLL_PAGE_DN:		index += linecount;		break;
	  case SCROLL_ALLTHEWAY_UP:	index = 0;			break;
	  case SCROLL_ALLTHEWAY_DN:	index = itemcount - 1;		break;
	  default:			index = n;			break;
	}
	if (index < 0)
	    index = 0;
	else if (index >= itemcount)
	    index = itemcount - 1;
	if (linecount < itemcount) {
	    n = linecount / 2;
	    if (index < n)
		topitem = 0;
	    else if (index >= itemcount - n)
		topitem = itemcount - linecount;
	    else
		topitem = index - n;
	}

	memcpy(colstmp, columns, columncount * sizeof *colstmp);
	drawtextrow(colstmp, items[0], flags, columncount);
	for (n = topitem ; n < itemcount ; ++n) {
	    if (colstmp[0].h < sdlg.font.h)
		break;
	    if (n == index)
		sdlg.textcolor = clr_yellow;
	    drawtextrow(colstmp, items[n + 1], flags, columncount);
	    if (n == index)
		sdlg.textcolor = textcolor;
	}
	SDL_UpdateRect(screen, 0, 0, 0, 0);

	n = SCROLL_NOP;
	if (!(*inputcallback)(&n))
	    break;
    }
    if (n)
	*idx = index;

    free(columns);
    free(colstmp);
    free(flags);
    cleardisplay();
    return n;
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
    cleardisplay();
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
    clr_dkgray = SDL_MapRGBA(screen->format, 128, 128, 128, 255);
    clr_yellow = SDL_MapRGBA(screen->format, 255, 255, 0, 255);

    sdlg.transpixel = SDL_MapRGBA(screen->format, 0, 0, 0, 0);
    if (sdlg.transpixel == clr_black) {
	sdlg.transpixel = 0xFFFFFFFF;
	sdlg.transpixel &= ~(screen->format->Rmask | screen->format->Gmask
						   | screen->format->Bmask);
    }

    sdlg.textcolor = clr_white;
    sdlg.halfcolor = clr_gray;
    sdlg.bkgndcolor = clr_black;
    cleardisplay();

    if (!createprompticons())
	return FALSE;

    return TRUE;
}
