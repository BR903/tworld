/* solution.c: Functions for reading and writing the solution files.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	"defs.h"
#include	"err.h"
#include	"fileio.h"
#include	"solution.h"

/* The solution file uses the following format:
 *
 * HEADER
 *  0-3   signature bytes
 *  4     ruleset (1=Lynx, 2=MS)
 *  5-7   other options (currently always zero)
 *
 * After the header are level solutions, in order:
 *
 * PER LEVEL
 *  0-3   offset to next solution (from the end of this field)
 *  4-8   time of solution in ticks
 *  9-12  initial random number generator value
 *  13    initial random slide direction
 * 14-??  solution bytes
 *
 * If there is no solution for a level, the first field (offset to
 * next solution) is zero and none of the other fields are present.
 *
 * The solution bytes consist of a stream of 1-, 2-, and/or 4-byte
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
 *
 * Numbers are always stored in little-endian order, consistent with
 * the data file.
 */

/* The signature bytes of the solution files.
 */
#define	CSSIG		0xACAA0200UL

/* The path of the directory containing the user's solution files.
 */
char   *savedir = NULL;

/* FALSE if savedir's existence is unverified.
 */
int	savedirchecked = FALSE;

/*
 * Functions for manipulating move lists.
 */

/* Initialize or reinitialize list as empty.
 */
void initmovelist(actlist *list)
{
    if (!list->allocated || !list->list) {
	list->allocated = 16;
	if (!(list->list = malloc(list->allocated * sizeof *list->list)))
	    memerrexit();
    }
    list->count = 0;
}

/* Append move to the end of list.
 */
void addtomovelist(actlist *list, action move)
{
    if (list->count >= list->allocated) {
	list->allocated *= 2;
	if (!(list->list = realloc(list->list,
				   list->allocated * sizeof *list->list))) {
	    fprintf(stderr, "unable to allocate %lu bytes",
		    (unsigned long)(list->allocated * sizeof *list->list));
	    memerrexit();
	}
    }
    list->list[list->count++] = move;
}

/* Make to an independent copy of from.
 */
void copymovelist(actlist *to, actlist const *from)
{
    to->list = NULL;
    initmovelist(to);
    if (!to->allocated || !to->list)
	to->allocated = 16;
    while (to->allocated < from->count)
	to->allocated *= 2;
    if (!(to->list = realloc(to->list, to->allocated * sizeof *to->list)))
	memerrexit();
    to->count = from->count;
    memcpy(to->list, from->list, from->count * sizeof *from->list);
}

/* Deallocate list.
 */
void destroymovelist(actlist *list)
{
    if (list->list)
	free(list->list);
    list->allocated = 0;
    list->list = NULL;
}

/*
 * Functions for handling the solution file header.
 */

/* Read the header bytes of the given solution file. flags receives
 * the option bytes (bytes 4-7).
 */
int readsolutionheader(fileinfo *file, int *flags)
{
    unsigned long	sig, f;

    if (!filereadint32(file, &sig, "not a valid solution file"))
	return FALSE;
    if (sig != CSSIG)
	return fileerr(file, "not a valid solution file");
    if (!filereadint32(file, &f, "not a valid solution file"))
	return FALSE;
    *flags = (int)f;
    return TRUE;
}

/* Write the header bytes to the given solution file.
 */
static int writesolutionheader(fileinfo *file, int flags)
{
    return filewriteint32(file, CSSIG, NULL)
	&& filewriteint32(file, flags, NULL);
}

/*
 * File I/O for move lists.
 */

/* Read a move list from the given file into the given move list.
 * FALSE is returned if the move list in the file was less than size
 * bytes long, or is otherwise invalid.
 */
static int readmovelist(fileinfo *file, actlist *moves, unsigned long size)
{
    action		act;
    unsigned char	byte;
    unsigned short	word;
    int			n;

    initmovelist(moves);
    act.when = -1;
    n = size;
    while (n > 0) {
	if (!filereadint8(file, &byte, "unexpected EOF"))
	    return FALSE;
	act.dir = idxdir(byte & 3);
	act.when += byte >> 3;
	--n;
	if (byte & 4) {
	    if (!filereadint8(file, &byte, "unexpected EOF"))
		return FALSE;
	    act.when += (byte & ~1) << 4;
	    --n;
	    if (byte & 1) {
		if (!filereadint16(file, &word, "unexpected EOF"))
		    return FALSE;
		act.when += (unsigned long)word << 12;
		n -= 2;
	    }
	}
	++act.when;
	addtomovelist(moves, act);
    }
    if (n) {
	initmovelist(moves);
	return fileerr(file, "invalid data in solutions file");
    }

    return TRUE;
}

/* Writes the given move list to the given file. psize receives the
 * number of bytes written.
 */
static int writemovelist(fileinfo *file, actlist const *moves,
			 unsigned long *psize)
{
    unsigned char	byte;
    int			when, delta, size, n;

    when = -1;
    size = 0;
    for (n = 0 ; n < moves->count ; ++n) {
	delta = -when - 1;
	when = moves->list[n].when;
	delta += when;
	byte = (delta << 3) & 0xF8;
	byte |= diridx(moves->list[n].dir);
	if (delta >= (1 << 5)) {
	    byte |= 4;
	    if (!filewriteint8(file, byte, "write error"))
		return FALSE;
	    ++size;
	    byte = (delta >> 4) & 0xFE;
	    if (delta >= (1 << 12)) {
		byte |= 1;
		if (!filewriteint8(file, byte, "write error"))
		    return FALSE;
		byte = (delta >> 12) & 0xFF;
		if (!filewriteint8(file, byte, "write error"))
		    return FALSE;
		size += 2;
		byte = (delta >> 20) & 0xFF;
	    }
	}
	if (!filewriteint8(file, byte, "write error"))
	    return FALSE;
	++size;
    }

    *psize = size;
    return TRUE;
}

/*
 * File I/O for level solutions.
 */

/* Read the data of a solution from the given file into the
 * appropriate fields of game.
 */
int readsolution(fileinfo *file, gamesetup *game)
{
    unsigned long	size, val;
    unsigned char	byte;

    initmovelist(&game->savedsolution);
    if (!file->fp)
	return TRUE;

    if (!filereadint32(file, &size, NULL))
	return FALSE;
    if (!size)
	return TRUE;

    if (!filereadint32(file, &val, "unexpected EOF"))
	return FALSE;
    game->besttime = val;
    if (!filereadint32(file, &val, "unexpected EOF"))
	return FALSE;
    game->savedrndseed = val;
    if (!filereadint8(file, &byte, "unexpected EOF"))
	return FALSE;
    game->savedrndslidedir = idxdir(byte);
    return readmovelist(file, &game->savedsolution, size - 9);
}

/* Write the data of a solution from the appropriate fields of game to
 * the given file.
 */
static int writesolution(fileinfo *file, gamesetup const *game)
{
    unsigned long	size;
    fpos_t		start, end;

    filegetpos(file, &start, "seek error");

    if (!filewriteint32(file, 0, "write error"))
	return FALSE;
    if (!game->savedsolution.count)
	return TRUE;

    if (!filewriteint32(file, game->besttime, "write error")
		|| !filewriteint32(file, game->savedrndseed, "write error")
		|| !filewriteint8(file, diridx((int)game->savedrndslidedir),
					"write error")
		|| !writemovelist(file, &game->savedsolution, &size))
	return FALSE;

    if (!filegetpos(file, &end, "seek error")
		|| !filesetpos(file, &start, "seek error")
		|| !filewriteint32(file, size + 9, "write error")
		|| !filesetpos(file, &end, "seek error"))
	return FALSE;

    return TRUE;
}

/* Write out all the solutions for series. Since each file contains
 * solutions for all the puzzles in one series, saving a new solution
 * requires that the function create the entire file's contents
 * afresh. In the case where only part of the original file has been
 * parsed so far, the unexamined contents are copied verbatim into a
 * temporary memory buffer, and then appended back onto the new file.
 */
int savesolutions(gameseries *series)
{
    gamesetup  *game;
    char       *tbuf = NULL;
    int		tbufsize = 0;
    int		i;

    if (!*savedir)
	return TRUE;

    if (series->solutionfile.fp && !series->allsolutionsread) {
	warn("Cannot save solution; save file incompletely parsed!");
	return FALSE;
    }
#if 0
    if (series->solutionfile.fp && !series->allsolutionsread) {
	int n = 0;
	warn("entering file loop; %d bytes in first read", BUFSIZ);
	while (!filetestend(&series->solutionfile)) {
	    if (tbufsize >= n) {
		n = n ? n * 2 : BUFSIZ;
		if (!(tbuf = realloc(tbuf, n)))
		    memerrexit();
		i = fread(tbuf + tbufsize, 1, n - tbufsize,
			  series->solutionfile.fp);
		warn("asked for %d - %d = %d bytes, got %d ...",
		     n, tbufsize, n - tbufsize, i);
		if (i <= 0) {
		    if (i < 0)
			fileerr(&series->solutionfile, "can't read");
		    break;
		}
		tbufsize += i;
	    }
	}
	warn("done with file loop");
    }
#endif
    if (!series->solutionfile.fp || series->solutionsreadonly) {
	if (!savedirchecked) {
	    savedirchecked = TRUE;
	    if (!finddir(savedir))
		return fileerr(&series->solutionfile,
			       "can't access directory");
	}
	if (series->solutionfile.fp)
	    fileclose(&series->solutionfile, NULL);
	else
	    series->allsolutionsread = TRUE;
	if (!openfileindir(&series->solutionfile, savedir,
			   series->mapfile.name, "w+b", "can't access file")) {
	    *savedir = '\0';
	    return FALSE;
	}
    }

    if (!filerewind(&series->solutionfile, "seek error"))
	return fileerr(&series->solutionfile, "cannot save solution");
    if (!writesolutionheader(&series->solutionfile, series->solutionflags))
	return fileerr(&series->solutionfile,
		       "saved-game file may have been corrupted!");
    for (i = 0, game = series->games ; i < series->count ; ++i, ++game)
	if (!writesolution(&series->solutionfile, game))
	    return fileerr(&series->solutionfile,
			   "saved-game file has become corrupted!");

    if (tbufsize) {
	fpos_t	pos;
	if (!filegetpos(&series->solutionfile, &pos, "seek error")
			|| !filewrite(&series->solutionfile, tbuf, tbufsize,
				      "write error")
			|| !filesetpos(&series->solutionfile, &pos,
				       "seek error"))
	    return fileerr(&series->solutionfile,
			   "saved-game file may have been corrupted!");
    }
    if (tbuf)
	free(tbuf);

    return TRUE;
}
