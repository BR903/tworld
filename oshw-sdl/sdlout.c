/* sdlout.c: Creating the program's displays.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
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

/* Erase a rectangle.
 */
#define	fillrect(r)		(puttext((r), NULL, 0, PT_MULTILINE))

/* Get a generic tile image.
 */
#define	gettileimage(id)	(getcellimage((id), Empty, 0))

/* Some prompting icons.
 */
static SDL_Surface     *prompticons = NULL;

/* Coordinates specifying the layout of the screen elements.
 */
static SDL_Rect		titleloc, infoloc, rinfoloc, invloc, hintloc;
static SDL_Rect		promptloc, displayloc, messageloc;
static int		screenw, screenh;

/* Setup a fontcolors structure, calculating the middle color from the
 * other two.
 */
static fontcolors makefontcolors(int rbkgnd, int gbkgnd, int bbkgnd,
				 int rtext, int gtext, int btext)
{
    fontcolors	colors;

    colors.c[0] = SDL_MapRGB(sdlg.screen->format, rbkgnd, gbkgnd, bbkgnd);
    colors.c[2] = SDL_MapRGB(sdlg.screen->format, rtext, gtext, btext);
    colors.c[1] = SDL_MapRGB(sdlg.screen->format, (rbkgnd + rtext) / 2,
						  (gbkgnd + gtext) / 2,
						  (bbkgnd + btext) / 2);
    return colors;
}

/* Create some simple icons used to prompt the user.
 */
static int createprompticons(void)
{
    static Uint8 iconpixels[] = {
	0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,
	0,0,0,0,1,2,2,2,2,1,0,0,0,0,0,0,
	0,0,1,2,2,2,2,2,2,1,0,0,0,0,0,0,
	1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	0,0,1,2,2,2,2,2,2,1,0,0,0,0,0,0,
	0,0,0,0,1,2,2,2,2,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
	0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0,
	0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0,
	0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,
	0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,
	0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,
	0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,
	0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,
	0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0,
	0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0,
	0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,2,2,2,2,1,0,0,0,0,
	0,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,
	0,0,0,0,0,0,1,2,2,2,2,2,2,1,0,0,
	0,0,0,0,0,0,1,2,2,2,2,1,0,0,0,0,
	0,0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,
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
	SDL_GetRGB(bkgndcolor(sdlg.dimtextclr), sdlg.screen->format,
		   &prompticons->format->palette->colors[0].r,
		   &prompticons->format->palette->colors[0].g,
		   &prompticons->format->palette->colors[0].b);
	SDL_GetRGB(halfcolor(sdlg.dimtextclr), sdlg.screen->format,
		   &prompticons->format->palette->colors[1].r,
		   &prompticons->format->palette->colors[1].g,
		   &prompticons->format->palette->colors[1].b);
	SDL_GetRGB(textcolor(sdlg.dimtextclr), sdlg.screen->format,
		   &prompticons->format->palette->colors[2].r,
		   &prompticons->format->palette->colors[2].g,
		   &prompticons->format->palette->colors[2].b);
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

    puttext(&rinfoloc, "Chips", 5, PT_CALCSIZE);
    rinfoloc.x = infoloc.x + rinfoloc.w + MARGINW;
    rinfoloc.y = infoloc.y;
    puttext(&rinfoloc, " 888", 4, PT_CALCSIZE);
    rinfoloc.h = 2 * texth;

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

    messageloc.x = infoloc.x;
    messageloc.y = titleloc.y;
    messageloc.w = promptloc.x - messageloc.x - MARGINW;
    messageloc.h = titleloc.h;

    hintloc.x = infoloc.x;
    hintloc.y = invloc.y + invloc.h + MARGINH;
    hintloc.w = screenw - MARGINW - hintloc.x;
    hintloc.h = messageloc.y - hintloc.y;
    if (hintloc.y + hintloc.h + MARGINH > promptloc.y)
	hintloc.h = promptloc.y - MARGINH - hintloc.y;

    return TRUE;
}

/* Create or change the program's display surface.
 */
static void createdisplay(void)
{
    if (sdlg.screen) {
	SDL_FreeSurface(sdlg.screen);
	sdlg.screen = NULL;
    }
    if (!(sdlg.screen = SDL_SetVideoMode(screenw, screenh, 32, SDL_HWSURFACE)))
	die("Cannot open %dx%d display: %s\n",
	    screenw, screenh, SDL_GetError());
    if (sdlg.screen->w != screenw || sdlg.screen->h != screenh)
	warn("Requested a %dx%d display, got %dx%d instead!",
	     sdlg.screen->w, sdlg.screen->h);
    if (sdlg.screen->format->BitsPerPixel != 32)
	warn("Requested a display with 32-bit depth, got %d-bit instead!",
	     sdlg.screen->format->BitsPerPixel);
}

/* Wipe the display.
 */
void cleardisplay(void)
{
    SDL_FillRect(sdlg.screen, NULL, bkgndcolor(sdlg.textclr));
}

/*
 * Tile display functions.
 */

static void drawopaquetile(int xpos, int ypos, Uint32 const *src)
{
    void const	       *endsrc;
    unsigned char      *dest;

    endsrc = src + sdlg.cptile;
    dest = (unsigned char*)sdlg.screen->pixels + ypos * sdlg.screen->pitch;
    dest = (unsigned char*)((Uint32*)dest + xpos);
    for ( ; src != endsrc ; src += sdlg.wtile, dest += sdlg.screen->pitch)
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
    line = (unsigned char*)sdlg.screen->pixels + ypos * sdlg.screen->pitch;
    line = (unsigned char*)((Uint32*)dest + xpos);
    for ( ; src != endsrc ; src += sdlg.wtile, line += sdlg.screen->pitch) {
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
    dest = (unsigned char*)sdlg.screen->pixels + tclip * sdlg.screen->pitch;
    dest = (unsigned char*)((Uint32*)dest + lclip);
    for (y = bclip - tclip ; y ; --y) {
	memcpy(dest, src, (rclip - lclip) * sizeof *src);
	dest += sdlg.screen->pitch;
	src += sdlg.wtile;
    }
}

static void drawtransptileclipped(SDL_Rect const *rect, Uint32 const *src)
{
    unsigned char      *line;
    Uint32	       *dest;
    int			lclip = displayloc.x;
    int			tclip = displayloc.y;
    int			rclip = displayloc.x + displayloc.w;
    int			bclip = displayloc.y + displayloc.h;
    int			x, y;

    if (rect->x > lclip)			lclip = rect->x;
    if (rect->y > tclip)			tclip = rect->y;
    if (rect->x + rect->w < rclip)		rclip = rect->x + rect->w;
    if (rect->y + rect->h < bclip)		bclip = rect->y + rect->h;
    if (lclip >= rclip || tclip >= bclip)
	return;
    src += (tclip - rect->y) * rect->w - rect->x;
    line = (unsigned char*)sdlg.screen->pixels + tclip * sdlg.screen->pitch;
    for (y = bclip - tclip ; y ; --y) {
	for (x = lclip, dest = (Uint32*)line ; x < rclip ; ++x)
	    if (src[x] != sdlg.transpixel)
		dest[x] = src[x];
	line += sdlg.screen->pitch;
	src += rect->w;
    }
}

/*
 * Message display function.
 */

static char		currentmsg[64];
static unsigned int	currentmsglen = 0;
static unsigned long	msguntil = 0;
static unsigned long	msgbolduntil = 0;

static void displaymsg(int update)
{
    int	f;

    if (currentmsg && msguntil < SDL_GetTicks()) {
	*currentmsg = '\0';
	currentmsglen = 0;
	f = 0;
    } else {
	if (!currentmsglen)
	    return;
	f = PT_CENTER;
	if (msgbolduntil < SDL_GetTicks())
	    f |= PT_DIM;
    }
    puttext(&messageloc, currentmsg, currentmsglen, f);
    if (update)
	SDL_UpdateRect(sdlg.screen, messageloc.x, messageloc.y,
				    messageloc.w, messageloc.h);
}

static int _setdisplaymsg(char const *msg, int msecs, int bold)
{
    if (!msg) {
	*currentmsg = '\0';
	currentmsglen = 0;
	msguntil = 0;
	msgbolduntil = 0;
    } else {
	currentmsglen = strlen(msg);
	if (currentmsglen >= sizeof currentmsg)
	    currentmsglen = sizeof currentmsg - 1;
	memcpy(currentmsg, msg, currentmsglen);
	currentmsg[currentmsglen] = '\0';
	msguntil = SDL_GetTicks() + msecs;
	msgbolduntil = SDL_GetTicks() + bold;
    }
    displaymsg(TRUE);
    return TRUE;
}

/*
 * Game display functions.
 */

/* Render the view of the visible area of the map to the output
 * buffer, including all visible creatures, with the view position
 * centered on the display as much as possible.
 */
static void displaymapview(gamestate const *state)
{
    SDL_Rect		rect;
    creature const     *cr;
    Uint32 const       *p;
    int			xdisppos, ydisppos;
    int			xorigin, yorigin;
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
    xorigin = displayloc.x - (xdisppos * sdlg.wtile / 4);
    yorigin = displayloc.y - (ydisppos * sdlg.htile / 4);

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
	    p = getcellimage(state->map[pos].top.id, state->map[pos].bot.id,
			     (state->statusflags & SF_NOANIMATION) ?
						-1 : state->currenttime);
	    drawopaquetileclipped(xorigin + x * sdlg.wtile,
				  yorigin + y * sdlg.htile, p);
	}
    }

    lmap -= 2;
    tmap -= 2;
    rmap += 2;
    bmap += 2;
    for (cr = state->creatures ; cr->id ; ++cr) {
	if (cr->hidden)
	    continue;
	x = cr->pos % CXGRID;
	y = cr->pos / CXGRID;
	if (x < lmap || x >= rmap || y < tmap || y >= bmap)
	    continue;
	rect.x = xorigin + x * sdlg.wtile;
	rect.y = yorigin + y * sdlg.htile;
	p = getcreatureimage(&rect, cr->id, cr->dir, cr->moving, cr->frame);
	drawtransptileclipped(&rect, p);
    }
}

/* Render all the various nuggets of data that comprise the
 * information display to the output buffer.
 */
static void displayinfo(gamestate const *state, int timeleft, int besttime)
{
    SDL_Rect	rect, rrect;
    char	buf[32];
    int		n;

    puttext(&titleloc, state->game->name, -1, PT_CENTER);

    rect = infoloc;

    if (state->game->number) {
	sprintf(buf, "Level %d", state->game->number);
	puttext(&rect, buf, -1, PT_UPDATERECT);
    } else
	puttext(&rect, "", 0, PT_UPDATERECT);

    if (state->game->passwd && *state->game->passwd) {
	sprintf(buf, "Password: %s", state->game->passwd);
	puttext(&rect, buf, -1, PT_UPDATERECT);
    } else
	puttext(&rect, "", 0, PT_UPDATERECT);

    puttext(&rect, "", 0, PT_UPDATERECT);

    rrect.x = rinfoloc.x;
    rrect.w = rinfoloc.w;
    rrect.y = rect.y;
    rrect.h = rect.h;
    puttext(&rect, "Chips", 5, PT_UPDATERECT);
    puttext(&rect, "Time", 4, PT_UPDATERECT);
    sprintf(buf, "%d", state->chipsneeded);
    puttext(&rrect, buf, -1, PT_RIGHT | PT_UPDATERECT);
    if (timeleft < 0)
	strcpy(buf, "---");
    else
	sprintf(buf, "%d", timeleft);
    puttext(&rrect, buf, -1, PT_RIGHT | PT_UPDATERECT);

    if (besttime) {
	if (timeleft < 0)
	    sprintf(buf, "(Best time: %d)", besttime);
	else
	    sprintf(buf, "Best time: %d", besttime);
	n = (state->game->sgflags & SGF_REPLACEABLE) ? PT_DIM : 0;
	puttext(&rect, buf, -1, PT_UPDATERECT | n);
    }
    fillrect(&rect);

    for (n = 0 ; n < 4 ; ++n) {
	drawopaquetile(invloc.x + n * sdlg.wtile, invloc.y,
		gettileimage(state->keys[n] ? Key_Red + n : Empty));
	drawopaquetile(invloc.x + n * sdlg.wtile, invloc.y + sdlg.htile,
		gettileimage(state->boots[n] ? Boots_Ice + n : Empty));
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
    SDL_BlitSurface(prompticons, &src, sdlg.screen, &promptloc);
    SDL_UpdateRect(sdlg.screen, promptloc.x, promptloc.y,
				promptloc.w, promptloc.h);
    return TRUE;
}

/*
 * The exported functions.
 */

/* Display the current state of the game.
 */
int displaygame(void const *state, int timeleft, int besttime)
{
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);
    displaymapview(state);
    displayinfo(state, timeleft, besttime);
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
    displaymsg(FALSE);
    SDL_UpdateRect(sdlg.screen, 0, 0, 0, 0);
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
	SDL_UpdateRect(sdlg.screen, hintloc.x, hintloc.y,
				    hintloc.w, hintloc.h);
    }
    return displayprompticon(completed);
}

/* Display some tabular information.
 */
int displaytable(char const *title, tablespec const *table, int completed)
{
    SDL_Rect	area;
    SDL_Rect   *cols;
    int		i, n;

    cleardisplay();

    area.x = MARGINW;
    area.y = screenh - MARGINH - sdlg.font.h;
    area.w = screenw - 2 * MARGINW;
    area.h = sdlg.font.h;
    puttext(&area, title, -1, 0);
    area.h = area.y - MARGINH;
    area.y = MARGINH;

    cols = measuretable(&area, table);
    for (i = table->rows, n = 0 ; i ; --i)
	drawtablerow(table, cols, &n, 0);
    free(cols);

    displayprompticon(completed);
    SDL_UpdateRect(sdlg.screen, 0, 0, 0, 0);
    return TRUE;
}


/* Display some text with illustrations on the side.
 */
int displaytiletable(char const *title,
		     tiletablerow const *rows, int count, int completed)
{
    SDL_Rect	left, right;
    int		col, id, i;

    cleardisplay();
    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_LockSurface(sdlg.screen);

    left.x = MARGINW;
    left.y = screenh - MARGINH - sdlg.font.h;
    left.w = screenw - 2 * MARGINW;
    left.h = sdlg.font.h;
    puttext(&left, title, -1, 0);
    left.h = left.y - MARGINH;
    left.y = MARGINH;

    right = left;
    col = sdlg.wtile * 2 + MARGINW;
    right.x += col;
    right.w -= col;
    for (i = 0 ; i < count ; ++i) {
	if (rows[i].isfloor)
	    id = rows[i].item1;
	else
	    id = crtile(rows[i].item1, EAST);
	drawopaquetile(left.x + sdlg.wtile, left.y, gettileimage(id));
	if (rows[i].item2) {
	    if (rows[i].isfloor)
		id = rows[i].item2;
	    else
		id = crtile(rows[i].item2, EAST);
	    drawopaquetile(left.x, left.y, gettileimage(id));
	}
	left.y += sdlg.htile;
	left.h -= sdlg.htile;
	puttext(&right, rows[i].desc, -1, PT_MULTILINE | PT_UPDATERECT);
	if (left.y < right.y) {
	    left.y = right.y;
	    left.h = right.h;
	} else {
	    right.y = left.y;
	    right.h = left.h;
	}
    }

    if (SDL_MUSTLOCK(sdlg.screen))
	SDL_UnlockSurface(sdlg.screen);
    displayprompticon(completed);

    SDL_UpdateRect(sdlg.screen, 0, 0, 0, 0);

    return TRUE;
}

/* Display a scrollable list, calling the callback function to manage
 * the selection until it returns FALSE.
 */
int displaylist(char const *title, void const *tab, int *idx,
		int (*inputcallback)(int*))
{
    tablespec const    *table = tab;
    SDL_Rect		area;
    SDL_Rect	       *cols;
    SDL_Rect	       *colstmp;
    int			linecount, itemcount, topitem, index;
    int			j, n;

    cleardisplay();
    area.x = MARGINW;
    area.y = screenh - MARGINH - sdlg.font.h;
    area.w = screenw - 2 * MARGINW;
    area.h = sdlg.font.h;
    puttext(&area, title, -1, 0);
    area.h = area.y - MARGINH;
    area.y = MARGINH;
    cols = measuretable(&area, table);
    if (!(colstmp = malloc(table->cols * sizeof *colstmp)))
	memerrexit();

    itemcount = table->rows - 1;
    topitem = 0;
    linecount = area.h / sdlg.font.h - 1;

    index = *idx;
    n = SCROLL_NOP;
    do {
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

	n = 0;
	SDL_FillRect(sdlg.screen, &area, bkgndcolor(sdlg.textclr));
	memcpy(colstmp, cols, table->cols * sizeof *colstmp);
	drawtablerow(table, colstmp, &n, 0);
	for (j = 0 ; j < topitem ; ++j)
	    drawtablerow(table, NULL, &n, 0);
	for ( ; j < topitem + linecount && j < itemcount ; ++j)
	    drawtablerow(table, colstmp, &n, j == index ? PT_HILIGHT : 0);
	SDL_UpdateRect(sdlg.screen, 0, 0, 0, 0);

	n = SCROLL_NOP;
    } while ((*inputcallback)(&n));
    if (n)
	*idx = index;

    free(cols);
    free(colstmp);
    cleardisplay();
    return n;
}

int displayinputprompt(char const *prompt, char *input, int maxlen,
		       int (*inputcallback)(void))
{
    SDL_Rect	area, promptrect, inputrect;
    int		len, ch;

    puttext(&inputrect, "W", 1, PT_CALCSIZE);
    inputrect.w *= maxlen + 1;
    puttext(&promptrect, prompt, -1, PT_CALCSIZE);
    area.h = inputrect.h + promptrect.h + 2 * MARGINH;
    if (inputrect.w > promptrect.w)
	area.w = inputrect.w;
    else
	area.w = promptrect.w;
    area.w += 2 * MARGINW;
    area.x = (screenw - area.w) / 2;
    area.y = (screenh - area.h) / 2;
    promptrect.x = area.x + MARGINW;
    promptrect.y = area.y + MARGINH;
    promptrect.w = area.w - 2 * MARGINW;
    inputrect.x = promptrect.x;
    inputrect.y = promptrect.y + promptrect.h;
    inputrect.w = promptrect.w;

    len = strlen(input);
    if (len > maxlen)
	len = maxlen;
    for (;;) {
	SDL_FillRect(sdlg.screen, &area, textcolor(sdlg.textclr));
	++area.x;
	++area.y;
	area.w -= 2;
	area.h -= 2;
	SDL_FillRect(sdlg.screen, &area, bkgndcolor(sdlg.textclr));
	--area.x;
	--area.y;
	area.w += 2;
	area.h += 2;
	puttext(&promptrect, prompt, -1, PT_CENTER);
	input[len] = '_';
	puttext(&inputrect, input, len + 1, PT_CENTER);
	input[len] = '\0';
	SDL_UpdateRect(sdlg.screen, area.x, area.y, area.w, area.h);
	ch = (*inputcallback)();
	if (ch == '\n' || ch < 0)
	    break;
	if (isprint(ch)) {
	    input[len] = ch;
	    if (len < maxlen)
		++len;
	    input[len] = '\0';
	} else if (ch == '\b') {
	    if (len)
		--len;
	    input[len] = '\0';
	} else if (ch == '\f') {
	    len = 0;
	    input[0] = '\0';
	} else {
	    /* no op */
	}
    }
    cleardisplay();
    return ch == '\n';
}

/*
 *
 */

/* Create a display surface appropriate to the requirements of the
 * game.
 */
int creategamedisplay(void)
{
    if (!layoutscreen())
	return FALSE;
    createdisplay();
    cleardisplay();
    return TRUE;
}

/* Initialize with a generic display surface capable of displaying text.
 */
int _sdloutputinitialize(void)
{
    sdlg.setdisplaymsgfunc = _setdisplaymsg;

    screenw = 640;
    screenh = 480;
    promptloc.x = screenw - MARGINW - ENDICONW;
    promptloc.y = screenh - MARGINH - ENDICONH;
    promptloc.w = ENDICONW;
    promptloc.h = ENDICONH;
    createdisplay();
    cleardisplay();

    sdlg.transpixel = SDL_MapRGBA(sdlg.screen->format, 0, 0, 0, 0);
    if (sdlg.transpixel == SDL_MapRGBA(sdlg.screen->format, 0, 0, 0, 255)) {
	sdlg.transpixel = 0xFFFFFFFF;
	sdlg.transpixel &= ~(sdlg.screen->format->Rmask
					| sdlg.screen->format->Gmask
					| sdlg.screen->format->Bmask);
    }

    sdlg.textclr = makefontcolors(0, 0, 0, 255, 255, 255);
    sdlg.dimtextclr = makefontcolors(0, 0, 0, 192, 192, 192);
    sdlg.hilightclr = makefontcolors(0, 0, 0, 255, 255, 0);

    cleardisplay();

    if (!createprompticons())
	return FALSE;

    return TRUE;
}
