/* score.c: Calculating scores and formatting the display of same.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"defs.h"
#include	"err.h"
#include	"play.h"
#include	"score.h"

/* Translate a number into an ASCII string, complete with commas.
 */
static char const *commify(int number)
{
    static char	buf[32];
    char       *dest = buf + sizeof buf;
    int		n = 0;

    *--dest = '\0';
    do {
	++n;
	if (n % 4 == 0) {
	    *--dest = ',';
	    ++n;
	}
	*--dest = '0' + number % 10;
	number /= 10;
    } while (number);
    return dest;
}

/* Return the user's scores for a given level.
 */
int getscoresforlevel(gameseries const *series, int level,
		      int *base, int *bonus, int *total)
{
    gamesetup	       *game;
    int			levelscore, timescore;
    unsigned int	totalscore;
    int			n;

    totalscore = 0;
    for (n = 0, game = series->games ; n < series->count ; ++n, ++game) {
	if (n >= series->allocated)
	    break;
	levelscore = 0;
	timescore = 0;
	if (hassolution(game)) {
	    levelscore = game->number * 500;
	    if (game->time)
		timescore = 10 * (game->time
					- game->besttime / TICKS_PER_SECOND);
	}
	if (n == level) {
	    *base = levelscore;
	    *bonus = timescore;
	}
	totalscore += levelscore + timescore;
    }
    *total = totalscore;
    return TRUE;
}

/* Produce a table that displays the user's score, broken down by
 * levels with a grand total at the end. If usepasswds is FALSE, all
 * levels are displayed. Otherwise, levels after the last level for
 * which the user knows the password are left out. Other levels for
 * which the user doesn't know the password are in the table, but
 * without any information besides the level's number.
 */
int createscorelist(gameseries const *series, int usepasswds,
		    int **plevellist, int *pcount, tablespec *table)
{
    gamesetup	       *game;
    char	      **ptrs;
    char	       *textheap;
    char	       *blank;
    int		       *levellist = NULL;
    unsigned int	levelscore, timescore;
    unsigned int	totalscore;
    int			count;
    int			used, j, n;

    if (plevellist) {
	levellist = malloc((series->count + 2) * sizeof *levellist);
	if (!levellist)
	    memerrexit();
    }
    ptrs = malloc((series->count + 2) * 5 * sizeof *ptrs);
    textheap = malloc((series->count + 2) * 128);
    if (!ptrs || !textheap)
	memerrexit();
    totalscore = 0;

    n = 0;
    used = 0;
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1+Level");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1-Name");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1+Base");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1+Bonus");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1+Score");

    blank = textheap + used;
    used += 1 + sprintf(textheap + used, "4- ");

    count = 0;
    for (j = 0, game = series->games ; j < series->count ; ++j, ++game) {
	if (j >= series->allocated)
	    break;

	ptrs[n++] = textheap + used;
	used += 1 + sprintf(textheap + used, "1+%d", game->number);
	if (hassolution(game)) {
	    ptrs[n++] = textheap + used;
	    used += 1 + sprintf(textheap + used, "1-%.64s", game->name);
	    ptrs[n++] = textheap + used;
	    levelscore = 500 * game->number;
	    used += 1 + sprintf(textheap + used, "1+%s", commify(levelscore));
	    ptrs[n++] = textheap + used;
	    if (game->time) {
		timescore = 10 * (game->time
					- game->besttime / TICKS_PER_SECOND);
		used += 1 + sprintf(textheap + used, "1+%s",
				    commify(timescore));
	    } else {
		timescore = 0;
		strcpy(textheap + used, "1+---");
		used += 6;
	    }
	    ptrs[n++] = textheap + used;
	    used += 1 + sprintf(textheap + used, "1+%s",
				commify(levelscore + timescore));
	    totalscore += levelscore + timescore;
	    if (plevellist)
		levellist[count] = j;
	    ++count;
	} else {
	    if (!usepasswds || (game->sgflags & SGF_HASPASSWD)) {
		ptrs[n++] = textheap + used;
		used += 1 + sprintf(textheap + used, "4-%s", game->name);
		if (plevellist)
		    levellist[count] = j;
	    } else {
		ptrs[n++] = blank;
		if (plevellist)
		    levellist[count] = -1;
	    }
	    ++count;
	}
    }

    while (ptrs[n - 1] == blank) {
	n -= 2;
	--count;
    }

    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "2-Total Score");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "3+%s", commify(totalscore));
    if (plevellist)
	levellist[count] = -1;
    ++count;

    if (plevellist)
	*plevellist = levellist;
    if (pcount)
	*pcount = count;

    table->rows = count + 1;
    table->cols = 5;
    table->sep = 2;
    table->collapse = 1;
    table->items = ptrs;

    return TRUE;
}

/* Produce a table that displays the user's best times for each level
 * that has a solution. If showfractions is FALSE, times are rounded
 * down to second precision.
 */
int createtimelist(gameseries const *series, int showfractions,
		   int **plevellist, int *pcount, tablespec *table)
{
    gamesetup	       *game;
    char	      **ptrs;
    char	       *textheap;
    char	       *untimed;
    int		       *levellist = NULL;
    long		leveltime;
    int			count;
    int			used, j, n;

    if (plevellist) {
	levellist = malloc((series->count + 1) * sizeof *levellist);
	if (!levellist)
	    memerrexit();
    }
    ptrs = malloc((series->count + 1) * 4 * sizeof *ptrs);
    textheap = malloc((series->count + 1) * 128);
    if (!ptrs || !textheap)
	memerrexit();

    n = 0;
    used = 0;
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1+Level");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1-Name");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1+Time");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1+Solution");

    untimed = textheap + used;
    used += 1 + sprintf(textheap + used, "1+---");

    count = 0;
    for (j = 0, game = series->games ; j < series->count ; ++j, ++game) {
	if (j >= series->allocated)
	    break;
	if (!hassolution(game))
	    continue;
	ptrs[n++] = textheap + used;
	used += 1 + sprintf(textheap + used, "1+%d", game->number);
	ptrs[n++] = textheap + used;
	used += 1 + sprintf(textheap + used, "1-%.64s", game->name);
	if (game->time) {
	    leveltime = (game->time + 1) * TICKS_PER_SECOND;
	    leveltime -= game->besttime + 1;
	    ptrs[n++] = textheap + used;
	    used += 1 + sprintf(textheap + used, "1+%d", game->time);
	} else {
	    leveltime = 1000 * TICKS_PER_SECOND - game->besttime - 1;
	    ptrs[n++] = untimed;
	}
	ptrs[n++] = textheap + used;
	switch (showfractions) {
	  case 0:
	    used += 1 + sprintf(textheap + used, "1+%ld",
				leveltime / TICKS_PER_SECOND);
	    break;
	  case 1:
	    if (leveltime < TICKS_PER_SECOND - 1) {
		leveltime = TICKS_PER_SECOND - leveltime;
		used += 1 + sprintf(textheap + used, "1+-%ld - .%01ld",
			      leveltime / TICKS_PER_SECOND,
			      ((10 * leveltime) / TICKS_PER_SECOND) % 10);
	    } else {
		used += 1 + sprintf(textheap + used, "1+%ld - .%01ld",
			      leveltime / TICKS_PER_SECOND,
			      9 - ((10 * leveltime) / TICKS_PER_SECOND) % 10);
	    }
	    break;
	  default:
	    used += 1 + sprintf(textheap + used, "1+%.*f", showfractions,
				(double)leveltime / TICKS_PER_SECOND);
	    break;
	}
	if (plevellist)
	    levellist[count] = j;
	++count;
    }

    if (plevellist)
	*plevellist = levellist;
    if (pcount)
	*pcount = count;

    table->rows = count + 1;
    table->cols = 4;
    table->sep = 2;
    table->collapse = 1;
    table->items = ptrs;

    return TRUE;
}

/* Free the memory allocated by createscorelist() or createtimelist().
 */
void freescorelist(int *levellist, tablespec *table)
{
    free(levellist);
    if (table) {
	free(table->items[0]);
	free(table->items);
    }
}
