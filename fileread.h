/* fileread.h: Functions for reading the data files.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_fileread_h_
#define	_fileread_h_

#include	<stdio.h>
#include	"cc.h"
#include	"movelist.h"

/* The collection of data maintained for each game.
 */
typedef	struct gamesetup {
    int		number;
    int		time;
    int		chips;
    int		map1size;
    int		map2size;
    int		creaturecount;
    int		trapcount;
    int		clonercount;
    int		besttime;
    unsigned long savedrndseed;
    char	savedrndslidedir;
    actlist	savedsolution;
    ushrt	creatures[256];
    xyconn	traps[256];
    xyconn	cloners[256];
    char	name[256];
    char	passwd[256];
    char	hinttext[256];
    uchar      *map1;
    uchar      *map2;
} gamesetup;

/* The collection of data maintained for each game file.
 */
typedef	struct gameseries {
    int		total;			/* the number of levels in the file */
    int		allocated;		/* number of elements allocated */
    int		count;			/* actual size of array */
    gamesetup  *games;			/* the list of levels */
    char       *filename;		/* the name of the files */
    FILE       *mapfp;			/* the file containing the levels */
    FILE       *solutionfp;		/* the file of the user's solutions */
    int		solutionflags;		/* settings for the saved solutions */
    int		allmapsread;		/* TRUE if levels are at EOF */
    int		allsolutionsread;	/* TRUE if solutions are at EOF */
    int		solutionsreadonly;	/* TRUE if solutions are readonly */
    char	name[256];		/* the series's name */
} gameseries;

/* The path of the directory containing the game files.
 */
extern char    *datadir;

/* Find the game files and allocate an array of gameseries structures
 * for them.
 */
extern int getseriesfiles(char *filename, gameseries **list, int *count);

/* Read the given game file up to the given level.
 */
extern int readlevelinseries(gameseries *series, int level);

#endif
