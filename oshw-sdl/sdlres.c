/* sdlres.c: Functions for loading the program's external resources.
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

/* This is an automatically-generated file, which contains a
 * representation of the program's icon.
 */
#include	"ccicon.c"

/*
 *
 */

int _sdlresourceinitialize(void)
{
    SDL_Surface	       *icon;

    icon = SDL_CreateRGBSurfaceFrom(cciconimage, CXCCICON, CYCCICON,
				    32, 4 * CXCCICON,
				    0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (icon) {
	SDL_WM_SetIcon(icon, cciconmask);
	SDL_FreeSurface(icon);
    } else
	warn("couldn't create icon surface: %s", SDL_GetError());

    return TRUE;
}
