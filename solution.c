/* solution.c: Functions for reading and writing the solution files.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	"defs.h"
#include	"err.h"
#include	"fileio.h"
#include	"series.h"
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
 *  6-9   level password (four ASCII characters in "cleartext")
 *  10    unused, always zero
 *  11    initial random slide direction (only used in Lynx ruleset)
 * 12-15  initial random number generator value
 * 16-19  time of solution in ticks
 * 19-??  solution bytes
 *
 * If the offset field is 0, then none of the other fields are
 * present. (This permits the file to contain padding.) If the offset
 * field is 6, then the level's number and password are present but
 * without a saved game. Otherwise, the offset should never be less
 * than 16.
 *
 * The solution bytes consist of a stream of 1-, 2-, and 4-byte
 * values. There are four possible formats, with the first two bits of
 * the value providing unique identification:
 *
 * 01234567
 * 00DDddDD
 *
 * 01234567
 * 10DDDTTT
 *
 * 01234567 89012345
 * 01DDDTTT TTTTTTTT
 *
 * 01234567 89012345 67890123 45678901
 * 11DDDTTT TTTTTTTT TTTTTTTT TTTTTTTT
 *
 * Ignoring for the moment the first form, in each case the three bits
 * marked D contain the direction of the move, and the bits marked T
 * indicate the amount of time, in ticks, between this move and the
 * prior move, less one (i.e., a value of T=0 indicates a move that
 * occurs on the tick immediately following the previous move). The
 * exception to this is the very first move of a level: it is not
 * decremented, as that would sometimes require a negative value to be
 * stored.
 *
 * The first form, unlike the others, holds three separate moves in a
 * single byte. A value of T=3 (i.e., four ticks) is automatically
 * implied for all three moves.
 *
 * Numeric values are always stored in little-endian order, consistent
 * with the data file.
 */

/* The signature bytes of the solution files.
 */
#define	CSSIG		0x999B3335UL

/* Translate move directions between three-bit and four-bit
 * representations.
 *
 * 0 = NORTH	    = 0001 = 1
 * 1 = WEST	    = 0010 = 2
 * 2 = SOUTH	    = 0100 = 4
 * 3 = EAST	    = 1000 = 8
 * 4 = NORTH | WEST = 0011 = 3
 * 5 = SOUTH | WEST = 0110 = 6
 * 6 = NORTH | EAST = 1001 = 9
 * 7 = SOUTH | EAST = 1100 = 12 */
static int const diridx8[16] = {
    -1,  0,  1,  4,  2, -1,  5, -1,  3,  6, -1, -1,  7, -1, -1, -1
};
static int const idxdir8[8] = {
    NORTH, WEST, SOUTH, EAST,
    NORTH | WEST, SOUTH | WEST, NORTH | EAST, SOUTH | EAST
};

/* The path of the directory containing the user's solution files.
 */
char		       *savedir = NULL;

/* FALSE if savedir's existence is unverified.
 */
int			savedirchecked = FALSE;

/*
 * Functions for manipulating move lists.
 */

/* Initialize or reinitialize list as empty.
 */
void initmovelist(actlist *list)
{
    if (!list->allocated || !list->list) {
	list->allocated = 16;
	xalloc(list->list, list->allocated * sizeof *list->list);
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
    if (!to->allocated || !to->list)
	to->allocated = 16;
    while (to->allocated < from->count)
	to->allocated *= 2;
    xalloc(to->list, to->allocated * sizeof *to->list);
    to->count = from->count;
    if (from->count)
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
 * size tells the function the length of the list as stored in the
 * file. FALSE is returned if the move list in the file was less than
 * size bytes long, or is otherwise invalid.
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
	--n;
	switch (byte & 3) {
	  case 0:
	    act.dir = idxdir8[(byte >> 2) & 3];
	    act.when += 4;
	    addtomovelist(moves, act);
	    act.dir = idxdir8[(byte >> 4) & 3];
	    act.when += 4;
	    addtomovelist(moves, act);
	    act.dir = idxdir8[(byte >> 6) & 3];
	    act.when += 4;
	    addtomovelist(moves, act);
	    break;
	  case 1:
	    act.dir = idxdir8[(byte >> 2) & 7];
	    act.when += (byte >> 5) & 7;
	    ++act.when;
	    addtomovelist(moves, act);
	    break;
	  case 2:
	    act.dir = idxdir8[(byte >> 2) & 7];
	    act.when += (byte >> 5) & 7;
	    if (!filereadint8(file, &byte, "unexpected EOF"))
		return FALSE;
	    --n;
	    act.when += (unsigned long)byte << 3;
	    ++act.when;
	    addtomovelist(moves, act);
	    break;
	  case 3:
	    act.dir = idxdir8[(byte >> 2) & 7];
	    act.when += (byte >> 5) & 7;
	    if (!filereadint8(file, &byte, "unexpected EOF"))
		return FALSE;
	    --n;
	    act.when += (unsigned long)byte << 3;
	    if (!filereadint16(file, &word, "unexpected EOF"))
		return FALSE;
	    n -= 2;
	    act.when += (unsigned long)word << 11;
	    ++act.when;
	    addtomovelist(moves, act);
	    break;
	}
    }
    if (n) {
	initmovelist(moves);
	return fileerr(file, "invalid data in solutions file");
    }

    return TRUE;
}

/* Write the given move list to the given file. psize receives the
 * number of bytes written. FALSE is returned if an error occurs.
 */
static int writemovelist(fileinfo *file, actlist const *moves,
			 unsigned long *psize)
{
    unsigned long	size;
    unsigned char	byte;
    unsigned short	word;
    unsigned long	dwrd;
    int			when, delta, n;

    when = -1;
    size = 0;
    for (n = 0 ; n < moves->count ; ++n) {
	delta = -when - 1;
	when = moves->list[n].when;
	delta += when;
	if (delta == 3 && n + 2 < moves->count
	        && diridx8[moves->list[n].dir] < 4
		&& moves->list[n + 1].when - moves->list[n].when == 4
	        && diridx8[moves->list[n + 1].dir] < 4
		&& moves->list[n + 2].when - moves->list[n + 1].when == 4
	        && diridx8[moves->list[n + 2].dir] < 4) {
	    byte = 0x00 | (diridx8[moves->list[n].dir] << 2)
			| (diridx8[moves->list[n + 1].dir] << 4)
			| (diridx8[moves->list[n + 2].dir] << 6);
	    if (!filewriteint8(file, byte, "write error"))
		return FALSE;
	    when = moves->list[n + 2].when;
	    ++size;
	    n += 2;
	} else if (delta < (1 << 3)) {
	    byte = 0x01 | (diridx8[moves->list[n].dir] << 2)
			| ((delta << 5) & 0xE0);
	    if (!filewriteint8(file, byte, "write error"))
		return FALSE;
	    ++size;
	} else if (delta < (1 << 11)) {
	    word = 0x02 | (diridx8[moves->list[n].dir] << 2)
			| ((delta << 5) & 0xFFE0);
	    if (!filewriteint16(file, word, "write error"))
		return FALSE;
	    size += 2;
	} else if (delta < (1 << 27)) {
	    dwrd = 0x03 | (diridx8[moves->list[n].dir] << 2)
			| ((delta << 5) & 0xFFFFFFE0);
	    if (!filewriteint32(file, dwrd, "write error"))
		return FALSE;
	    size += 4;
	} else {
	    return fileerr(file, "!!! huge delta in solution file");
	    return FALSE;
	}
    }

    *psize = size;
    return TRUE;
}

/*
 * File I/O for level solutions.
 */

/* Read the data of a one complete solution from the given file into
 * the appropriate fields of game.
 */
static int readsolution(fileinfo *file, gamesetup *game)
{
    unsigned long	size, val32;
    unsigned short	val16;
    unsigned char	val8;

    game->number = 0;
    game->sgflags = 0;
    game->besttime = 0;
    initmovelist(&game->savedsolution);
    if (!file->fp)
	return TRUE;

    if (!filereadint32(file, &size, NULL))
	return FALSE;
    if (!size)
	return TRUE;
    if (size < 16 && size != 6)
	return fileerr(file, "invalid data in solution file");

    if (!filereadint16(file, &val16, "unexpected EOF"))
	return FALSE;
    game->number = val16;
    if (!fileread(file, game->passwd, 4, "unexpected EOF"))
	return FALSE;
    game->passwd[5] = '\0';
    game->sgflags |= SGF_HASPASSWD;

    if (size == 6)
	return TRUE;

    if (!filereadint8(file, &val8, "unexpected EOF"))
	return FALSE;
    if (!filereadint8(file, &val8, "unexpected EOF"))
	return FALSE;
    game->savedrndslidedir = idxdir8[val8];
    if (!filereadint32(file, &val32, "unexpected EOF"))
	return FALSE;
    game->savedrndseed = val32;
    if (!filereadint32(file, &val32, "unexpected EOF"))
	return FALSE;
    game->besttime = val32;
    return readmovelist(file, &game->savedsolution, size - 16);
}

/* Write the data of one complete solution from the appropriate fields
 * of game to the given file.
 */
static int writesolution(fileinfo *file, gamesetup const *game)
{
    unsigned long	size;
    fpos_t		start, end;

    if (!game->savedsolution.count) {
	if (!(game->sgflags & SGF_HASPASSWD))
	    return TRUE;
	if (!filewriteint32(file, 6, "write error")
			|| !filewriteint16(file, game->number, "write error")
			|| !filewrite(file, game->passwd, 4, "write error"))
	    return FALSE;
	return TRUE;
    }

    filegetpos(file, &start, "seek error");

    if (!filewriteint32(file, 0, "write error"))
	return FALSE;

    if (!filewriteint16(file, game->number, "write error")
		|| !filewrite(file, game->passwd, 4, "write error")
		|| !filewriteint8(file, 0, "write error")
		|| !filewriteint8(file, diridx8[(int)game->savedrndslidedir],
					"write error")
		|| !filewriteint32(file, game->savedrndseed, "write error")
		|| !filewriteint32(file, game->besttime, "write error")
		|| !writemovelist(file, &game->savedsolution, &size))
	return FALSE;

    if (!filegetpos(file, &end, "seek error")
		|| !filesetpos(file, &start, "seek error")
		|| !filewriteint32(file, size + 16, "write error")
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

/* Read all the solutions for the given series. FALSE is returned if
 * an error occurs. Note that it is not an error for the solution file
 * to not exist.
 */
int readsolutions(gameseries *series)
{
    gamesetup	gametmp;
    int		i, j;

    if (!savedir || !*savedir
		 || !opensolutionfile(&series->solutionfile,
				      series->filebase, TRUE)) {
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
	if (!gametmp.number || !*gametmp.passwd)
	    continue;
	i = findlevelinseries(series, gametmp.number, gametmp.passwd);
	if (i < 0) {
	    i = findlevelinseries(series, 0, gametmp.passwd);
	    if (i < 0) {
		fileerr(&series->solutionfile,
			"unmatched password in solution file");
		continue;
	    }
	    warn("level %d has been moved to level %d",
		 gametmp.number, series->games[i].number);
	}
	series->games[i].besttime = gametmp.besttime;
	series->games[i].sgflags = gametmp.sgflags;
	series->games[i].savedrndslidedir = gametmp.savedrndslidedir;
	series->games[i].savedrndseed = gametmp.savedrndseed;
	copymovelist(&series->games[i].savedsolution, &gametmp.savedsolution);
	if (i == j)
	    ++j;
    }
    destroymovelist(&gametmp.savedsolution);
    fileclose(&series->solutionfile, NULL);
    return TRUE;
}

/* Write out all the solutions for series. The solution file is
 * created if it does not currently exist. The solution file's
 * directory is also created if it does not currently exist. (Nothing
 * is done if no directory has been set, however.) FALSE is returned
 * if an error occurs.
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
			  series->filebase, FALSE)) {
	*savedir = '\0';
	return FALSE;
    }

    if (!writesolutionheader(&series->solutionfile, series->solutionflags))
	return fileerr(&series->solutionfile,
		       "saved-game file has become corrupted!");
    for (i = 0, game = series->games ; i < series->count ; ++i, ++game) {
	if (!writesolution(&series->solutionfile, game))
	    return fileerr(&series->solutionfile,
			   "saved-game file has become corrupted!");
    }

    fileclose(&series->solutionfile, NULL);
    return TRUE;
}
