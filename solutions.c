#include	<stdio.h>
#include	<stdlib.h>
#include	"gen.h"
#include	"movelist.h"
#include	"dirio.h"
#include	"solutions.h"

/* The solution file uses the following format:
 *
 * HEADER
 *  0-3   signature
 *  4-8   options (currently always zero)
 *
 * After the header are level solutions, in order:
 *
 * PER LEVEL
 *  0-3   offset to next solution (if no solution this value is zero)
 *  4-8   time of solution in ticks
 *  9-12  initial PRNG value
 *  13    initial random slide value
 * 14-??  solution bytes
 *
 * The solution bytes consist of a series of 1-, 2-, and 4-byte
 * values. The three different possibilities take the following forms:
 *
 * 01234567
 * DD0TTTTT
 * and:
 * 01234567 01234567
 * DD1TTTTT 0TTTTTTT
 * and:
 * 01234567 01234567 01234567 01234567
 * DD1TTTTT 1TTTTTTT TTTTTTTT TTTTTTTT
 *
 * In each form, D contains the direction of movement, and T contains
 * the amount of time, in ticks, between this move and the previous
 * move, less one. (The exception to this is the very first move of
 * the solution, for which the time interval is not decremented, as
 * that would require T to include a negative value.)
 */

#define	CSSIG_0		0x00
#define	CSSIG_1		0x02
#define	CSSIG_2		0xAA
#define	CSSIG_3		0xAC

/* The directory containing the user's solution files.
 */
char   *savedir = NULL;

/* FALSE if savedir's existence is unverified.
 */
int	savedirchecked = FALSE;

/*
 * File I/O for 32-bit integers.
 */

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

static int write32(FILE *fp, unsigned long val32)
{
    return fputc(val32 & 255, fp) != EOF
			&& fputc((val32 >>  8) & 255, fp) != EOF
			&& fputc((val32 >> 16) & 255, fp) != EOF
			&& fputc((val32 >> 24) & 255, fp) != EOF;
}

/*
 * Functions for handling the solution file header.
 */

int readsolutionheader(FILE *fp, int *flags)
{
    unsigned long	f;

    if (fgetc(fp) != CSSIG_0 || fgetc(fp) != CSSIG_1
			     || fgetc(fp) != CSSIG_2
			     || fgetc(fp) != CSSIG_3)
	return FALSE;
    if (!read32(fp, &f))
	return FALSE;

    *flags = (int)f;
    return TRUE;
}

static int writesolutionheader(FILE *fp, int flags)
{
    if (fputc(CSSIG_0, fp) == EOF || fputc(CSSIG_1, fp) == EOF
				  || fputc(CSSIG_2, fp) == EOF
				  || fputc(CSSIG_3, fp) == EOF)
	return FALSE;
    if (!write32(fp, flags))
	return FALSE;
    return TRUE;
}

/*
 * File I/O for move lists.
 */

static int readmovelist(FILE *fp, actlist *moves, unsigned long size)
{
    action	act;
    int		byte;
    int		n;

    initmovelist(moves);
    act.when = -1;
    n = size;
    while (n > 0) {
	if ((byte = fgetc(fp)) == EOF)
	    return FALSE;
	act.dir = (int)(uchar)byte & 3;
	act.when += (int)(uchar)byte >> 3;
	--n;
	if (byte & 4) {
	    if ((byte = fgetc(fp)) == EOF)
		return FALSE;
	    act.when += ((int)(uchar)byte & ~1) << 4;
	    --n;
	    if (byte & 1) {
		if ((byte = fgetc(fp)) == EOF)
		    return FALSE;
		act.when += (int)(uchar)byte << 12;
		if ((byte = fgetc(fp)) == EOF)
		    return FALSE;
		act.when += (int)(uchar)byte << 20;
		n -= 2;
	    }
	}
	++act.when;
	addtomovelist(moves, act);
    }

    if (n) {
	fileerr("invalid data in solutions file");
	initmovelist(moves);
	return FALSE;
    }
    return TRUE;
}

static int writemovelist(FILE *fp, actlist const *moves,
			 unsigned long *psize)
{
    uchar	byte;
    int		when, delta, size, n;

    when = -1;
    size = 0;
    for (n = 0 ; n < moves->count ; ++n) {
	delta = -when - 1;
	when = moves->list[n].when;
	delta += when;
	byte = (delta << 3) & 0xF8;
	byte |= moves->list[n].dir & 3;
	if (delta >= (1 << 5)) {
	    byte |= 4;
	    if (fputc(byte, fp) == EOF)
		return FALSE;
	    ++size;
	    byte = (delta >> 4) & 0xFE;
	    if (delta >= (1 << 12)) {
		byte |= 1;
		if (fputc(byte, fp) == EOF)
		    return FALSE;
		byte = (delta >> 12) & 0xFF;
		if (fputc(byte, fp) == EOF)
		    return FALSE;
		size += 2;
		byte = (delta >> 20) & 0xFF;
	    }
	}
	if (fputc(byte, fp) == EOF)
	    return FALSE;
	++size;
    }

    *psize = size;
    return TRUE;
}

/*
 * File I/O for level solutions.
 */

int readsolution(FILE *fp, gamesetup *game)
{
    unsigned long	size, val;

    initmovelist(&game->savedsolution);
    if (!fp)
	return TRUE;

    if (!read32(fp, &size))
	return FALSE;
    if (!size)
	return TRUE;

    if (!read32(fp, &val))
	return FALSE;
    game->besttime = val;
    if (!read32(fp, &val))
	return FALSE;
    game->savedrndseed = val;
    game->savedrndslidedir = fgetc(fp);
    return readmovelist(fp, &game->savedsolution, size - 9);
}

static int writesolution(FILE *fp, gamesetup const *game)
{
    unsigned long	size;
    fpos_t		start, end;

    fgetpos(fp, &start);

    if (!write32(fp, 0))
	return FALSE;
    if (!game->savedsolution.count)
	return TRUE;

    if (!write32(fp, game->besttime))
	return FALSE;
    if (!write32(fp, game->savedrndseed))
	return FALSE;
    if (fputc((char)game->savedrndslidedir, fp) == EOF)
	return FALSE;
    if (!writemovelist(fp, &game->savedsolution, &size))
	return FALSE;

    fgetpos(fp, &end);
    fsetpos(fp, &start);
    if (!write32(fp, size + 9))
	return FALSE;
    fsetpos(fp, &end);

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
		i = fread(tbuf + tbufsize, 1, n - tbufsize,
			  series->solutionfp);
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

    rewind(series->solutionfp);
    writesolutionheader(series->solutionfp, series->solutionflags);
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
