/* fileread.c: Functions for reading the game files.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"gen.h"
#include	"movelist.h"
#include	"dirio.h"
#include	"solutions.h"
#include	"parse.h"
#include	"fileread.h"

#define	CCSIG_0		0xAC
#define	CCSIG_1		0xAA
#define	CCSIG_2		0x02
#define	CCSIG_3		0x00

/* Mini-structure for our findfiles() callback.
 */
typedef	struct seriesdata {
    gameseries *list;
    int		count;
} seriesdata;

/* The path of the directory containing the level files.
 */
char   *datadir = NULL;

/* Examine the top of the game file. FALSE is returned if the file
 * appears to be invalid.
 */
static int readseriesheader(gameseries *series)
{
    int	n;

    if (fgetc(series->mapfp) != CCSIG_0 || fgetc(series->mapfp) != CCSIG_1
					|| fgetc(series->mapfp) != CCSIG_2
					|| fgetc(series->mapfp) != CCSIG_3)
	goto badfile;

    if ((n = fgetc(series->mapfp)) == EOF)
	goto badfile;
    series->total = n;
    if ((n = fgetc(series->mapfp)) == EOF)
	goto badfile;
    series->total |= n << 8;

    if (series->total)
	return TRUE;

  badfile:
    fclose(series->mapfp);
    series->mapfp = NULL;
    series->allmapsread = TRUE;
    return FALSE;
}

/* A callback function that initializes a gameseries structure for
 * filename and adds it to the list stored under the second argument.
 */
static int getseriesfile(char *filename, void *data)
{
    static int	allocated = 0;
    seriesdata *sdata = (seriesdata*)data;
    gameseries *series;

    while (sdata->count >= allocated) {
	++allocated;
	if (!(sdata->list = realloc(sdata->list,
				    allocated * sizeof *sdata->list)))
	    memerrexit();
    }
    series = sdata->list + sdata->count++;
    series->filename = filename;
    series->mapfp = NULL;
    series->solutionfp = NULL;
    series->allmapsread = FALSE;
    series->allsolutionsread = FALSE;
    series->allocated = 0;
    series->count = 0;
    series->games = NULL;
    *series->name = '\0';
    return 1;
}

/* A callback function to compare two gameseries structures by
 * comparing their filenames.
 */
static int gameseriescmp(void const *a, void const *b)
{
    return strcmp(((gameseries*)a)->filename, ((gameseries*)b)->filename);
}

/* Search the game file directory and generate an array of gameseries
 * structures corresponding to the level files found there.
 */
int getseriesfiles(char *filename, gameseries **list, int *count)
{
    seriesdata	s;

    s.list = NULL;
    s.count = 0;
    if (*filename && isfilename(filename)) {
	if (getseriesfile(filename, &s) <= 0 || !s.count)
	    die("Couldn't access \"%s\"", filename);
	*datadir = '\0';
	*savedir = '\0';
    } else {
	if (!findfiles(datadir, &s, getseriesfile) || !s.count)
	    die("Couldn't find any data files in \"%s\".", datadir);
	if (s.count > 1)
	    qsort(s.list, s.count, sizeof *s.list, gameseriescmp);
    }
    *list = s.list;
    *count = s.count;
    return TRUE;
}

/* Read the game file corresponding to series, and the corresponding
 * solutions in the solution file, until at least level maps have been
 * successfully parsed, or the end is reached. The files are opened if
 * they have not been already. No files are opened if the requested
 * level is already in memory.
 */
int readlevelinseries(gameseries *series, int level)
{
    int		n;

    if (level < 0)
	return FALSE;
    if (series->count > level)
	return TRUE;

    if (!series->allmapsread) {
	if (!series->mapfp) {
	    currentfilename = series->filename;
	    series->mapfp = openfileindir(datadir, series->filename, "rb");
	    if (!series->mapfp)
		return fileerr(NULL);
	    if (!readseriesheader(series))
		return fileerr("file contains no maps");
	    if (*savedir) {
		series->solutionfp = openfileindir(savedir, 
						   series->filename, "rb");
		if (series->solutionfp) {
		    savedirchecked = TRUE;
		    series->solutionsreadonly = TRUE;
		}
	    } else
		series->solutionfp = NULL;
	}
	while (!series->allmapsread && series->count <= level) {
	    while (series->count >= series->allocated) {
		n = series->allocated ? series->allocated * 2 : 16;
		if (!(series->games = realloc(series->games,
					      n * sizeof *series->games)))
		    memerrexit();
		memset(series->games + series->allocated, 0,
		       (n - series->allocated) * sizeof *series->games);
		series->allocated = n;
	    }
	    if (readlevelmap(series->mapfp, series->games + series->count)) {
		if (!series->allsolutionsread)
		    readsolution(series->solutionfp,
				 series->games + series->count);
		++series->count;
	    }
	    if (feof(series->mapfp)) {
		fclose(series->mapfp);
		series->mapfp = NULL;
		series->allmapsread = TRUE;
	    }
	    if (series->solutionfp && feof(series->solutionfp))
		series->allsolutionsread = TRUE;
	}
    }
    return series->count > level;
}
