/* solution.c: Functions for reading and writing the solution files.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_solution_h_
#define	_solution_h_

#include	"defs.h"
#include	"fileio.h"

/* The path of the directory containing the user's solution files.
 */
extern char    *savedir;

/* Initialize or reinitialize list as empty.
 */
extern void initmovelist(actlist *list);

/* Append move to the end of list.
 */
extern void addtomovelist(actlist *list, action move);

/* Make to an independent copy of from.
 */
extern void copymovelist(actlist *to, actlist const *from);

/* Deallocate list.
 */
extern void destroymovelist(actlist *list);

/* Read any and all saved solutions for the given series.
 */
extern int readsolutions(gameseries *series);

/* Write out all the solutions for series.
 */
extern int savesolutions(gameseries *series);

#endif
