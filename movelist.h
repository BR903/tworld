/* movelist.h: Functions for manipulating lists of moves.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_movelist_h_
#define	_movelist_h_

#include	"gen.h"

/* A move is stored as a direction and a time.
 */
typedef	struct action { int when:29, dir:3; } action;

/* A list of moves.
 */
typedef struct actlist {
    int		allocated;	/* number of elements allocated */
    int		count;		/* size of the actual array */
    action     *list;		/* the array */
} actlist;

/* Initialize or reinitialize list as empty.
 */
extern void initmovelist(actlist *list);

/* Initialize list as having size elements.
 */
extern void setmovelist(actlist *list, int size);

/* Append move to the end of list.
 */
extern void addtomovelist(actlist *list, action move);

/* Make to an independent copy of from.
 */
extern void copymovelist(actlist *to, actlist const *from);

/* Deallocate list.
 */
extern void destroymovelist(actlist *list);

#endif
