/* sdlres.h: Internal functions for accessing external resources.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_sdlres_h_
#define	_sdlres_h_

#include	"SDL.h"

/* The total number of tile images.
 */
#define	NTILES		128

#if 0

/* The SDL surface containing the tiles.
 */
extern Uint32	       *cctiles;

/* Flag array indicating which tiles have transparent pixels.
 */
extern char		transparency[NTILES];

extern void _sdlsettransparentcolor(Uint32 color);
extern void _sdlsettileformat(SDL_PixelFormat const *fmt);

#endif

#endif
