/* series.h: Functions for finding and reading the data files.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_series_h_
#define	_series_h_

#include	"defs.h"

/* The directory containing the data files.
 */
extern char    *seriesdir;

/* Load all levels of the given series.
 */
extern int readseriesfile(gameseries *series);

/* Produce a list all available data files. pserieslist receives the
 * location of an array of gameseries structures, one per data file
 * successfully found. pcount points to a value that is filled in with
 * the number of the data files. pptrs receives the location of an
 * array of strings describing the data files. The strings show the
 * names of the files, as well as how many level each contains and
 * which ruleset each uses. pheader, if it is not NULL, is filled in
 * with a pointer to a string providing headers for the columns of
 * strings of the array. preferredfile optionally provides the
 * filename or pathname of a single data file. If the preferred data
 * file is found, it will be the only one returned.
 */
extern int createserieslist(char const *preferredfile,
			    gameseries **pserieslist,
			    char ***pptrs, int *pcount, int const **align);

/* Free all memory allocated by createserieslist().
 */
extern void freeserieslist(char **ptrs, int count);

#endif
