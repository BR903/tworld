/* score.c: Calculating scores and formatting the display of same.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"defs.h"
#include	"err.h"
#include	"play.h"
#include	"score.h"

/* Translate number into an ASCII string, complete with commas. The
 * string produced is always at least len bytes long, padded on the
 * left with spaces if necessary.
 */
static int commify(char *dest, int number, int len)
{
    int	n = 0;

    dest[len] = '\0';
    do {
	++n;
	if (n % 4 == 0) {
	    dest[len - n] = ',';
	    ++n;
	}
	dest[len - n] = '0' + number % 10;
	number /= 10;
    } while (number);
    if (n < len)
	memset(dest, ' ', len - n);
    return len;
}

/* Produce an array of strings that break down the player's current
 * score into a pretty little list.
 */
int createscorelist(gameseries const *series,
		    char ***pptrs, int *pcount, char const **pheader)
{
    gamesetup	       *game;
    char	      **ptrs;
    char	       *textheap;
    char	       *p;
    int			levelscore, timescore;
    unsigned int	totalscore;
    int			count, used, n;

    ptrs = malloc((series->count + 1) * sizeof *ptrs);
    textheap = malloc((series->count + 1) * 72);
    used = 0;
    if (!ptrs || !textheap)
	memerrexit();
    count = 0;
    totalscore = 0;
    for (n = 0, game = series->games ; n < series->count ; ++n, ++game) {
	if (n >= series->allocated)
	    break;
	ptrs[count] = textheap + used;
	if (!hassolution(game)) {
	    used += sprintf(ptrs[count], "%3d  %-30s",
					 game->number, game->name);
	} else {
	    levelscore = game->number * 500;
	    if (game->time)
		timescore = 10 * (game->time
					- game->besttime / TICKS_PER_SECOND);
	    else
		timescore = 0;

	    p = ptrs[count];
	    p += sprintf(p, "%3d  %-30.30s", game->number, game->name);
	    p += commify(p, levelscore, 8);
	    p += commify(p, timescore, 13);
	    p += commify(p, levelscore + timescore, 13);
	    totalscore += levelscore + timescore;
	    used += p - ptrs[count];
	}
	++used;
	++count;
    }
    if (count) {
	ptrs[count] = textheap + used;
	n = sprintf(ptrs[count], "%3s  %-51.51s", "", "Total Score");
	commify(ptrs[count] + n, totalscore, 13);
    } else {
	ptrs[0] = textheap;
	strcpy(ptrs[0], "     (No levels completed)");
    }	
    ++count;

    *pptrs = ptrs;
    *pcount = count;
    if (pheader)
	*pheader = "Lvl  Name                       "
		   "Level bonus   Time bonus  Level score";
    return TRUE;
}

/* Free the memory allocated by createscorelist().
 */
void freescorelist(char **ptrs, int count)
{
    (void)count;
    free(ptrs[0]);
    free(ptrs);
}
