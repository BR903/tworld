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

/* Produce an array of strings that break down the player's current
 * score into a pretty little list.
 */
int createscorelist(gameseries const *series,
		    char ***pptrs, int *pcount, int const **align)
{
    static int const	alignments[] = { +1, -1, +1, +1, +1 };
    gamesetup	       *game;
    char	      **ptrs;
    char	       *textheap;
    char	       *p;
    int			levelscore, timescore;
    unsigned int	totalscore;
    int			used, n;

    ptrs = malloc((series->count + 2) * sizeof *ptrs);
    textheap = malloc((series->count + 1) * 80);
    if (!ptrs || !textheap)
	memerrexit();
    totalscore = 0;

    if (align)
	strcpy(textheap, "Level\tName\tBase\tBonus\tScore");
    else
	strcpy(textheap, "Lvl  Name                                        "
			 "Base  Bonus   Score");
    ptrs[0] = textheap;
    used = strlen(textheap) + 1;
    for (n = 0, game = series->games ; n < series->count ; ++n, ++game) {
	if (n >= series->allocated)
	    break;
	p = ptrs[n + 1] = textheap + used;
	if (!hassolution(game)) {
	    p += sprintf(p, (align ? "%d\t%.50s\t\t\t" : "%3d  %-50s"),
			    game->number, game->name);
	} else {
	    p += sprintf(p, (align ? "%d\t%.50s\t" : "%3d  %-50.50s "),
			    game->number, game->name);
	    levelscore = game->number * 500;
	    p += commify(p, levelscore, 7);
	    if (align)
		*p++ = '\t';
	    else
		p += sprintf(p, "  ");
	    if (game->time) {
		timescore = 10 * (game->time
					- game->besttime / TICKS_PER_SECOND);
		p += commify(p, timescore, 5);
	    } else {
		timescore = 0;
		p += sprintf(p, (align ? "---" : "  ---"));
	    }
	    if (align)
		*p++ = '\t';
	    else
		p += sprintf(p, "  ");
	    p += commify(p, levelscore + timescore, 7);
	    totalscore += levelscore + timescore;
	}
	used += p - ptrs[n + 1] + 1;
    }
    if (n) {
	p = ptrs[n + 1] = textheap + used;
	if (align)
	    p += sprintf(p, "\tTotal Score\t\t\t");
	else
	    p += sprintf(p, "%3s  %-61.61s", "", "Total Score");
	commify(p, totalscore, 13);
    } else {
	ptrs[0] = textheap;
	strcpy(ptrs[0], "(No levels)");
    }

    *pptrs = ptrs;
    *pcount = n + 2;
    if (align)
	*align = alignments;
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
