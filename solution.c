/* solution.c: Functions for reading and writing the solution files.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	"defs.h"
#include	"err.h"
#include	"fileio.h"
#include	"solution.h"

/* The solution file uses the following format:
 *
 * HEADER
 *  0-3   signature bytes
 *   4    ruleset (1=Lynx, 2=MS)
 *  5-7   other options (currently always zero)
 *
 * After the header are level solutions, in order:
 *
 * PER LEVEL
 *  0-3   offset to next solution (from the end of this field)
 *  4-5   level number
 *   6    initial random slide direction (only used in Lynx ruleset)
 *   7    unused, always zero
 *  8-11  initial random number generator value
 * 12-15  time of solution in ticks
 * 16-??  solution bytes
 *
 * If the offset field is zero, then none of the other fields are
 * present. (This permits the file to contain padding.) Otherwise, the
 * offset should never be less than 12.
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
 * the data file.  */

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
	xalloc(list->list, list->allocated * sizeof *list->list);
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
    xalloc(to->list, to->allocated * sizeof *to->list);
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
static int readsolutionheader(fileinfo *file, int *flags)
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
static int readsolution(fileinfo *file, gamesetup *game)
{
    unsigned long	size, val32;
    unsigned short	val16;
    unsigned char	val8;

    initmovelist(&game->savedsolution);
    if (!file->fp)
	return TRUE;

    if (!filereadint32(file, &size, NULL))
	return FALSE;
    if (!size)
	return TRUE;

    if (!filereadint16(file, &val16, "unexpected EOF"))
	return FALSE;
    game->number = val16;
    if (!filereadint8(file, &val8, "unexpected EOF"))
	return FALSE;
    game->savedrndslidedir = idxdir(val8);
    if (!filereadint8(file, &val8, "unexpected EOF"))
	return FALSE;
    if (!filereadint32(file, &val32, "unexpected EOF"))
	return FALSE;
    game->savedrndseed = val32;
    if (!filereadint32(file, &val32, "unexpected EOF"))
	return FALSE;
    game->besttime = val32;
    return readmovelist(file, &game->savedsolution, size - 12);
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

    if (!filewriteint16(file, game->number, "write error")
		|| !filewriteint8(file, diridx((int)game->savedrndslidedir),
					"write error")
		|| !filewriteint8(file, 0, "write error")
		|| !filewriteint32(file, game->savedrndseed, "write error")
		|| !filewriteint32(file, game->besttime, "write error")
		|| !writemovelist(file, &game->savedsolution, &size))
	return FALSE;

    if (!filegetpos(file, &end, "seek error")
		|| !filesetpos(file, &start, "seek error")
		|| !filewriteint32(file, size + 12, "write error")
		|| !filesetpos(file, &end, "seek error"))
	return FALSE;

    return TRUE;
}

/*
 * File I/O for solution files.
 */

/* Locate the solution file for the given data file and open it.
 */
static int opensolutionfile(fileinfo *file, char const *datname, int readonly)
{
    char       *buf = NULL;
    char const *filename;
    int		n;

    if (file->name) {
	filename = file->name;
    } else {
	n = strlen(datname);
	if (datname[n - 4] == '.' && tolower(datname[n - 3]) == 'd'
				  && tolower(datname[n - 2]) == 'a'
				  && tolower(datname[n - 1]) == 't')
	    n -= 4;
	xalloc(buf, n + 5);
	memcpy(buf, datname, n);
	memcpy(buf + n, ".tws", 5);
	filename = buf;
    }
    n = openfileindir(file, savedir, filename,
		      readonly ? "rb" : "wb",
		      readonly ? NULL : "can't access file");
    if (buf)
	free(buf);
    return n;
}

/* Read all the solutions for the given series.
 */
int readsolutions(gameseries *series)
{
    gamesetup	gametmp;
    int		i, j;

    if (!savedir || !*savedir
		 || !opensolutionfile(&series->solutionfile,
				      series->mapfile.name, TRUE)) {
	series->solutionflags = series->ruleset;
	return TRUE;
    }

    if (!readsolutionheader(&series->solutionfile,
			    &series->solutionflags))
	return FALSE;
    if (series->solutionflags != series->ruleset)
	return fileerr(&series->solutionfile,
		       "saved-game file is for a different"
		       " ruleset than the data file");
    savedirchecked = TRUE;

    j = 0;
    memset(&gametmp, 0, sizeof gametmp);
    for (;;) {
	if (!readsolution(&series->solutionfile, &gametmp))
	    break;
	for (i = j ; i < series->count ; ++i)
	    if (series->games[i].number == gametmp.number)
		break;
	if (i == series->count) {
	    fileerr(&series->solutionfile, "invalid level in solution file");
	    break;
	}
	series->games[i].savedrndslidedir = gametmp.savedrndslidedir;
	series->games[i].savedrndseed = gametmp.savedrndseed;
	series->games[i].besttime = gametmp.besttime;
	copymovelist(&series->games[i].savedsolution, &gametmp.savedsolution);
	if (i == j)
	    ++j;
    }
    destroymovelist(&gametmp.savedsolution);
    fileclose(&series->solutionfile, NULL);
    return TRUE;
}

/* Write out all the solutions for series.
 */
int savesolutions(gameseries *series)
{
    gamesetup  *game;
    int		i;

    if (!*savedir)
	return TRUE;

    if (!savedirchecked) {
	savedirchecked = TRUE;
	if (!finddir(savedir))
	    return fileerr(&series->solutionfile, "can't access directory");
    }
    if (series->solutionfile.fp)
	fileclose(&series->solutionfile, NULL);
    if (!opensolutionfile(&series->solutionfile,
			  series->mapfile.name, FALSE)) {
	*savedir = '\0';
	return FALSE;
    }

    if (!writesolutionheader(&series->solutionfile, series->solutionflags))
	return fileerr(&series->solutionfile,
		       "saved-game file has become corrupted!");
    for (i = 0, game = series->games ; i < series->count ; ++i, ++game) {
	if (!game->savedsolution.count)
	    continue;
	if (!writesolution(&series->solutionfile, game))
	    return fileerr(&series->solutionfile,
			   "saved-game file has become corrupted!");
    }

    fileclose(&series->solutionfile, NULL);
    return TRUE;
}
