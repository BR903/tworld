/* res.h: Functions for loading resources from external files.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_res_h_
#define _res_h_

/* The directory containing all the resource files.
 */
extern char	       *resdir;

extern int loadgameresources(int ruleset);

extern int initresources(void);

extern void freeallresources(void);

#endif
