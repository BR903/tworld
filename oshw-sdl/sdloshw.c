/* sdloshw.c: Top-level SDL management functions.
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

/* Values global to this library.
 */
oshwglobals	sdlg;

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
    char	buf[256];

    if (subtitle && *subtitle) {
	sprintf(buf, "Tile World - %.242s", subtitle);
	SDL_WM_SetCaption(buf, "Tile World");
    } else
	SDL_WM_SetCaption("Tile World", "Tile World");
}

/* Shut down SDL.
 */
static void shutdown(void)
{
    if (SDL_WasInit(SDL_INIT_VIDEO))
	SDL_Quit();
}

/* Initialize SDL.
 */
int oshwinitialize(int silence, int showhistogram)
{
    sdlg.eventupdatefunc = _eventupdate;

    (void)silence;
    atexit(shutdown);
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
	die("Cannot initialize SDL system: %s\n", SDL_GetError());

    setsubtitle(NULL);

    return _sdltimerinitialize(showhistogram)
	&& _sdlresourceinitialize()
	&& _sdltextinitialize()
	&& _sdltileinitialize()
	&& _sdlinputinitialize()
	&& _sdloutputinitialize();
}
