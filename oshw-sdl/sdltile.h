/* sdltile.h: Tile image handling functions.
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

/* Return a pointer to a specific tile image.
 */
extern Uint32 const *_sdlgettileimage(int id, int transp);

/* Return a pointer to a tile image for a creature.
 */
extern Uint32 const *_sdlgetcreatureimage(int id, int dir, int moving);

/* Return a pointer to a tile image for a cell. If the top image is
 * transparent, the appropriate image is created in the overlay
 * buffer.
 */
extern Uint32 const *_sdlgetcellimage(int top, int bot);

#endif
