/* score.h: Calculating and formatting the scores.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_score_h_
#define	_score_h_

#include	"defs.h"

/* Produce a list of the player's scores for the given series,
 * formatted in columns. Each level in the series is listed in a
 * separate string, with an extra string at the end giving a grand
 * total. pptrs points to an array pointer that is filled in with the
 * location of an array of these strings. pcount points to a value
 * that is filled in with the size of the array. pheader, if it is not
 * NULL, is filled in with a pointer to a string providing headers for
 * the columns of the other strings.
 */
extern int createscorelist(gameseries const *series,
			   char ***pptrs, int *pcount, char const **pheader);

/* Free the memory allocated by createscorelist().
 */
extern void freescorelist(char **ptrs, int count);

#endif
