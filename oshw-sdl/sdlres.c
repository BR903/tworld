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

/* This automatically-generated file defines a built-in font.
 */
#include	"ccfont.c"

/* Magic numbers identifying a file as a "PC Screen Font" file.
 */
#define	PSF_SIG_1	0x36
#define	PSF_SIG_2	0x04

/*
 * Reading PSF files.
 */

int loadfontfromfile(char const *filename)
{
    fontinfo		font;
    SDL_RWops	       *file;
    unsigned char	header[4];
    int			n;

    file = SDL_RWFromFile(filename, "rb");
    if (!file) {
	errmsg(filename, "cannot read font file: %s", SDL_GetError());
	return FALSE;
    }
    n = SDL_RWread(file, header, 1, 4);
    if (n < 0) {
	errmsg(filename, "cannot read font file: %s", SDL_GetError());
	return FALSE;
    }
    if (n < 4 || header[0] != PSF_SIG_1 || header[1] != PSF_SIG_2) {
	errmsg(filename, "not a valid font file");
	return FALSE;
    }
    font.h = header[3];
    if (font.h < 8 || font.h > 32) {
	errmsg(filename, "font has invalid height");
	return FALSE;
    }
    font.bits = malloc(256 * font.h);
    if (!font.bits)
	memerrexit();
    n = SDL_RWread(file, font.bits, font.h, 256);
    if (n < 0) {
	errmsg(filename, "couldn't read font file: %s", SDL_GetError());
	return FALSE;
    } else if (n < 256) {
	errmsg(filename, "invalid font file");
	return FALSE;
    }
    SDL_RWclose(file);

    sdlg.font.w = 9;
    sdlg.font.h = font.h;
    sdlg.font.bits = font.bits;

    return TRUE;
}

/*
 *
 */

int _sdlresourceinitialize(void)
{
    SDL_Surface	       *icon;

    sdlg.font = ccfont;

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
