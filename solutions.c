#include	<stdio.h>
#include	<stdlib.h>
#include	"gen.h"
#include	"movelist.h"
#include	"dirio.h"
#include	"solutions.h"

/* The directory containing the user's solution files.
 */
char   *savedir = NULL;

/* FALSE if savedir's existence is unverified.
 */
int	savedirchecked = FALSE;

static int read32(FILE *fp, unsigned long *val32)
{
    int	byte;

    if ((byte = fgetc(fp)) == EOF)
	return FALSE;
    *val32 = (unsigned char)byte;
    if ((byte = fgetc(fp)) == EOF)
	return FALSE;
    *val32 |= (unsigned char)byte << 8;
    if ((byte = fgetc(fp)) == EOF)
	return FALSE;
    *val32 |= (unsigned char)byte << 16;
    if ((byte = fgetc(fp)) == EOF)
	return FALSE;
    *val32 |= (unsigned char)byte << 24;
    return TRUE;
}

int readsolution(FILE *fp, gamesetup *game)
{
    actlist	       *moves;
    action		act;
    unsigned long	val, count;
    int			n;

    moves = &game->savedsolution;
    initmovelist(moves);
    if (!fp)
	return TRUE;

    if (!read32(fp, &count))
	return FALSE;
    if (!count)
	return TRUE;

    for (n = 0 ; n < count ; ++n) {
	if (!read32(fp, &val))
	    return FALSE;
	act.when = val >> 2;
	act.dir = val & 3;
	addtomovelist(moves, act);
    }

    if (!read32(fp, &val))
	return FALSE;
    game->savedrndseed = val;
    if (!read32(fp, &val))
	return FALSE;
    game->besttime = val;

    return TRUE;
}

/*
 *
 */

static int write32(FILE *fp, unsigned long val32)
{
    return fputc(val32 & 255, fp) != EOF
			&& fputc((val32 >>  8) & 255, fp) != EOF
			&& fputc((val32 >> 16) & 255, fp) != EOF
			&& fputc((val32 >> 24) & 255, fp) != EOF;
}

static int writesolution(FILE *fp, gamesetup const *game)
{
    unsigned long	val;
    int			n;

    if (!write32(fp, game->savedsolution.count))
	return FALSE;
    if (!game->savedsolution.count)
	return TRUE;
    for (n = 0 ; n < game->savedsolution.count ; ++n) {
	val = game->savedsolution.list[n].when << 2;
	val |= game->savedsolution.list[n].dir & 3;
	if (!write32(fp, val))
	    return FALSE;
    }
    if (!write32(fp, game->savedrndseed))
	return FALSE;
    if (!write32(fp, game->besttime))
	return FALSE;
    return TRUE;
}

/* Write out all the solutions for series. Since each file contains
 * solutions for all the puzzles in one series, saving a new solution
 * requires that the function create the entire file's contents
 * afresh. In the case where only part of the original file has been
 * parsed, the unexamined contents are copied verbatim into a
 * temporary memory buffer, and then appended to the new file.
 */
int savesolutions(gameseries *series)
{
    gamesetup  *game;
    char       *tbuf = NULL;
    int		tbufsize = 0;
    fpos_t	pos;
    int		i, n;

    if (!*savedir)
	return TRUE;

    currentfilename = series->filename;

    if (series->solutionfp && !series->allsolutionsread) {
	n = 0;
	while (!feof(series->solutionfp)) {
	    if (tbufsize >= n) {
		n = n ? n * 2 : BUFSIZ;
		if (!(tbuf = realloc(tbuf, n)))
		    memerrexit();
		i = fread(tbuf + tbufsize, 1, n - tbufsize, series->solutionfp);
		if (i <= 0) {
		    if (i < 0)
			fileerr(NULL);
		    break;
		}
		tbufsize += i;
	    }
	}
    }

    if (!series->solutionfp || series->solutionsreadonly) {
	if (!savedirchecked) {
	    savedirchecked = TRUE;
	    if (!finddir(savedir)) {
		currentfilename = savedir;
		return fileerr(NULL);
	    }
	}
	if (series->solutionfp)
	    fclose(series->solutionfp);
	else
	    series->allsolutionsread = TRUE;
	series->solutionfp = openfileindir(savedir, series->filename, "w+b");
	if (!series->solutionfp) {
	    *savedir = '\0';
	    return fileerr(NULL);
	}
    }

    fseek(series->solutionfp, 0, SEEK_SET);
    for (i = 0, game = series->games ; i < series->count ; ++i, ++game)
	writesolution(series->solutionfp, game);

    if (tbufsize) {
	fgetpos(series->solutionfp, &pos);
	fwrite(tbuf, 1, tbufsize, series->solutionfp);
	fsetpos(series->solutionfp, &pos);
    }
    if (tbuf)
	free(tbuf);

    return TRUE;
}
