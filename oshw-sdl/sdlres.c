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

/* An automatically-generated file.
 */
#include	"ccicon.c"

#if 0

/*
 *
 */

static int setfontresource(int width, int height, unsigned char *fontdata)
{
    fontinfo	font;

    memset(fontdata, 0, 32 * height);
    memset(fontdata + 127 * height, 0, 33 * height);
    font.bits = fontdata;
    font.w = width;
    font.h = height;
    font.color = font.bkgnd = 0;
    _sdlsettextfont(&font);
}

#endif

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
