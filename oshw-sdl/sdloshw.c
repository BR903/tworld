/* sdloshw.c: Top-level SDL management functions.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../err.h"

/* Values global to this library.
 */
oshwglobals	sdlg;

/* This is an automatically-generated file, which contains a
 * representation of the program's icon.
 */
#include	"ccicon.c"

/* Dispatch all events sitting in the SDL event queue. 
 */
static void _eventupdate(int wait)
{
    SDL_Event	event;

    if (wait)
	SDL_WaitEvent(NULL);
    SDL_PumpEvents();
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_ALLEVENTS)) {
	switch (event.type) {
	  case SDL_KEYDOWN:
	    keyeventcallback(event.key.keysym.sym, TRUE);
	    break;
	  case SDL_KEYUP:
	    keyeventcallback(event.key.keysym.sym, FALSE);
	    break;
	  case SDL_QUIT:
	    exit(EXIT_SUCCESS);
	}
    }
}

/* Window decoration.
 */
void setsubtitle(char const *subtitle)
{
    char	buf[270];

    if (subtitle && *subtitle) {
	sprintf(buf, "Tile World - %.255s", subtitle);
	SDL_WM_SetCaption(buf, "Tile World");
    } else {
	SDL_WM_SetCaption("Tile World", "Tile World");
    }
}

/* Shut down SDL.
 */
static void shutdown(void)
{
    SDL_Quit();
}

/* Initialize SDL.
 */
int oshwinitialize(int silence, int showhistogram)
{
    SDL_Surface	       *icon;

    sdlg.eventupdatefunc = _eventupdate;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
	die("Cannot initialize SDL system: %s\n", SDL_GetError());
    atexit(shutdown);

    icon = SDL_CreateRGBSurfaceFrom(cciconimage, CXCCICON, CYCCICON,
				    32, 4 * CXCCICON,
				    0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (icon) {
	SDL_WM_SetIcon(icon, cciconmask);
	SDL_FreeSurface(icon);
    } else
	warn("couldn't create icon surface: %s", SDL_GetError());

    setsubtitle(NULL);

    return _sdltimerinitialize(showhistogram)
	&& _sdltextinitialize()
	&& _sdltileinitialize()
	&& _sdlinputinitialize()
	&& _sdloutputinitialize()
	&& _sdlsfxinitialize(silence);
}
