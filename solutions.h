/* solutions.h: Functions for reading and saving puzzle solutions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_solutions_h_
#define	_solutions_h_

#include	"fileread.h"

/* The directory containing the user's solution files.
 */
extern char    *savedir;

/* FALSE if savedir's existence is unverified.
 */
extern int	savedirchecked;

/* Read the solution header data for the given file.
 */
extern int readsolutionheader(FILE *fp, int *flags);

/* Read the solution for game, if any, from the current file position.
 */
extern int readsolution(FILE *fp, gamesetup *game);

/* Write out all the solutions for series.
 */
extern int savesolutions(gameseries *series);

#endif
