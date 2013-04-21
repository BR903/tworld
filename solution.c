/* solution.c: Functions for reading and writing the solution files.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"defs.h"
#include	"err.h"
#include	"fileio.h"
#include	"series.h"
#include	"solution.h"

/*
 * The following is a description of the solution file format. Note that
 * numeric values are always stored in little-endian order, consistent
 * with the data file.
 *
 * The header is at least eight bytes long, and contains the following
 * values:
 *
 * HEADER
 *  0-3   signature bytes (35 33 9B 99)
 *   4    ruleset (1=Lynx, 2=MS)
 *   5    other options (currently always zero)
 *   6    other options (currently always zero)
 *   7    count of bytes in remainder of header (currently always zero)
 *
 * After the header are level solutions, usually but not necessarily
 * in numerical order. Each solution begins with the following values:
 *
 * PER LEVEL
 *  0-3   offset to next solution (from the end of this field)
 *  4-5   level number
 *  6-9   level password (four ASCII characters in "cleartext")
 *  10    other flags (currently always zero)
 *  11    initial random slide direction (only used in Lynx ruleset)
 * 12-15  initial random number generator value
 * 16-19  time of solution in ticks
 * 20-xx  solution bytes
 *
 * If the offset field is 0, then none of the other fields are
 * present. (This permits the file to contain padding.) If the offset
 * field is 6, then the level's number and password are present but
 * without a saved game. Otherwise, the offset should never be less
 * than 16.
 *
 * The solution bytes consist of a stream of values indicating the
 * moves of the solution. The values vary in size from one to five
 * bytes in length. The size of each value is always specified in the
 * first byte. There are five different formats for the values.
 *
 * The first two are similar in structure:
 *
 * #1: 01234567    #2: 01234567 89012345
 *     10DDDTTT        01DDDTTT TTTTTTTT
 *
 * The two lowest bits indicate which format is used. The next three
 * bits, marked with Ds, contain the direction of the move. The
 * remaining bits are marked with Ts, and these indicate the amount of
 * time, in ticks, between this move and the prior move, less one.
 * (Thus, a value of T=0 indicates a move that occurs on the tick
 * immediately following the previous move.) The very first move of a
 * solution is an exception: it is not decremented, as that would
 * sometimes require a negative value to be stored.
 *
 * The third format has the form:
 *
 * #3: 01234567
 *     00DDEEFF
 *
 * This value encodes three separate moves (DD, EE, and FF) packed
 * into a single byte. Each move has an implicit time value of four
 * ticks separating it from the prior move. Note that since only two
 * bytes are used to store each move, this format can only store
 * orthogonal moves (i.e. it cannot be used to store a Lynx diagonal
 * move).
 *
 * The fourth format is four bytes in length:
 *
 * #4: 01234567 89012345 67890123 45678901
 *     11DD0TTT TTTTTTTT TTTTTTTT TTTTTTTT
 *
 * Like the third format, there are only two bits available for
 * storing the direction.
 *
 * The fifth and final format is like the first two formats, in that
 * it can vary in length, depending on how many bits are required to
 * store the time interval. It is shown here in its largest form:
 *
 * #5: 01234567 89012345 67890123 45678901 23456789
 *     11NN1DDD DDDDTTTT TTTTTTTT TTTTTTTT TTTTTTTT
 *
 * The two bits marked NN indicate the size of the field in bytes,
 * less two (i.e., 00 for a two-byte value, 11 for a five-byte value).
 * Seven bits are used to indicate the move's direction, which allows
 * this field to store MS mouse moves. The time value is encoded
 * normally, and can be 4, 12, 20, or 28 bits long.
 *
 * Because Tile World does not currently support mouse moves, this
 * version of the code will never write out a solution value using
 * this format. It will read such values, but only for non-mouse
 * moves.
 *
 * (Note that it is possible for a solution to exist that requires
 * more than 28 bits to store one of its time intervals. However, 28
 * bits covers an interval of over five months, which is a long time
 * to leave a level running and then pick up again and solve
 * successfully. So this is not seen as a realistic concern.)
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
 * 7 = SOUTH | EAST = 1100 = 12
 */
static int const diridx8[16] = {
    -1,  0,  1,  4,  2, -1,  5, -1,  3,  6, -1, -1,  7, -1, -1, -1
};
static int const idxdir8[8] = {
    NORTH, WEST, SOUTH, EAST,
    NORTH | WEST, SOUTH | WEST, NORTH | EAST, SOUTH | EAST
};

/* TRUE if file modification is prohibited.
 */
int		readonly = FALSE;

/* The path of the directory containing the user's solution files.
 */
char	       *savedir = NULL;

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
 * the option bytes (bytes 5-6). extra receives any bytes in the
 * header that this code doesn't recognize.
 */
static int readsolutionheader(fileinfo *file, int ruleset, int *flags,
			      int *extrasize, unsigned char *extra)
{
    unsigned long	sig;
    unsigned short	f;
    unsigned char	n;

    if (!filereadint32(file, &sig, "not a valid solution file"))
	return FALSE;
    if (sig != CSSIG)
	return fileerr(file, "not a valid solution file");
    if (!filereadint8(file, &n, "not a valid solution file"))
	return FALSE;
    if (n != ruleset)
	return fileerr(file, "solution file is for a different ruleset"
			     " than the level set file");
    if (!filereadint16(file, &f, "not a valid solution file"))
	return FALSE;
    *flags = (int)f;

    if (!filereadint8(file, &n, "not a valid solution file"))
	return FALSE;
    *extrasize = n;
    if (n)
	if (!fileread(file, extra, *extrasize, "not a valid solution file"))
	    return FALSE;

    return TRUE;
}

/* Write the header bytes to the given solution file.
 */
static int writesolutionheader(fileinfo *file, int ruleset, int flags,
			       int extrasize, unsigned char const *extra)
{
    return filewriteint32(file, CSSIG, NULL)
	&& filewriteint8(file, ruleset, NULL)
	&& filewriteint16(file, flags, NULL)
	&& filewriteint8(file, extrasize, NULL)
	&& filewrite(file, extra, extrasize, NULL);
}

static int writesolutionsetname(fileinfo *file, char const *setname)
{
    char	zeroes[16] = "";
    int		n;

    n = strlen(setname) + 1;
    return filewriteint32(file, n + 16, NULL)
	&& filewrite(file, zeroes, 16, NULL)
	&& filewrite(file, setname, n, NULL);
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
	    if (byte & 16) {
		word = (byte >> 2) & 3;
		act.dir = idxdir8[(byte >> 5) & 7];
		if (!filereadint8(file, &byte, "unexpected EOF"))
		    return FALSE;
		--n;
		if (byte & 15)
		    return fileerr(file, "unrecognized move in solution");
		act.when += (byte >> 4) & 15;
		if (word--) {
		    if (!filereadint8(file, &byte, "unexpected EOF"))
			return FALSE;
		    --n;
		    act.when += (unsigned long)byte << 4;
		    if (word == 1) {
			if (!filereadint8(file, &byte, "unexpected EOF"))
			    return FALSE;
			--n;
			act.when += (unsigned long)byte << 12;
		    } else if (word == 2) {
			if (!filereadint16(file, &word, "unexpected EOF"))
			    return FALSE;
			n -= 2;
			act.when += (unsigned long)word << 12;
		    }
		}
	    } else {
		act.dir = idxdir8[(byte >> 2) & 3];
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
	    }
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
	    if (diridx8[moves->list[n].dir] >= 4)
		return fileerr(file, "!!! large delta before diagonal move");
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
    if (size < 16) {
	fileskip(file, size - 6, NULL);
	return fileerr(file, "truncated metadata in solution file");
    }

    if (!filereadint8(file, &val8, "unexpected EOF"))
	return FALSE;
    if (!filereadint8(file, &val8, "unexpected EOF"))
	return FALSE;
    game->savedrndslidedir = idxdir8[val8 & 7];
    game->savedstepping = (val8 >> 3) & 7;
    if (!filereadint32(file, &val32, "unexpected EOF"))
	return FALSE;
    game->savedrndseed = val32;
    if (!filereadint32(file, &val32, "unexpected EOF"))
	return FALSE;
    game->besttime = val32;

    size -= 16;
    if (!game->number && !*game->passwd) {
	game->sgflags |= SGF_SETNAME;
	if (size < 256) {
	    return fileread(file, game->name, size, "unexpected EOF");
	} else {
	    game->name[255] = '\0';
	    if (!fileread(file, game->name, 255, "unexpected EOF"))
		return FALSE;
	    return fileskip(file, size - 255, "unexpected EOF");
	}
    } else {
	return readmovelist(file, &game->savedsolution, size);
    }
}

/* Write the data of one complete solution from the appropriate fields
 * of game to the given file.
 */
static int writesolution(fileinfo *file, gamesetup const *game)
{
    unsigned long	size = 0;
    fpos_t		start, end;
    unsigned char	val8;

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

    val8 = diridx8[(int)game->savedrndslidedir];
    val8 |= game->savedstepping << 3;
    if (!filewriteint16(file, game->number, "write error")
		|| !filewrite(file, game->passwd, 4, "write error")
		|| !filewriteint8(file, 0, "write error")
		|| !filewriteint8(file, val8, "write error")
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
static int opensolutionfile(fileinfo *file, char const *datname, int writable)
{
    static int	savedirchecked = FALSE;
    char       *buf = NULL;
    char const *filename;
    int		n;

    if (writable && readonly)
	return FALSE;

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

    if (!savedirchecked && savedir && *savedir && !haspathname(filename)) {
	savedirchecked = TRUE;
	if (readonly) {
	    *savedir = '\0';
	} else {
	    if (!finddir(savedir)) {
		*savedir = '\0';
		fileerr(file, "can't access directory");
	    }
	}
    }

    n = openfileindir(file, savedir, filename,
		      writable ? "wb" : "rb",
		      writable ? "can't access file" : NULL);
    if (buf)
	free(buf);
    return n;
}

/* Read any and all saved solutions for the given series.
 */
int readsolutions(gameseries *series)
{
    gamesetup	gametmp;
    int		i;

    if (!series->savefile.name)
	series->savefile.name = series->savefilename;
    if ((!series->savefile.name && (series->gsflags & GSF_NODEFAULTSAVE))
		|| !opensolutionfile(&series->savefile,
				     series->filebase, FALSE)) {
	series->solheaderflags = 0;
	series->solheadersize = 0;
	return TRUE;
    }

    if (!readsolutionheader(&series->savefile, series->ruleset,
			    &series->solheaderflags,
			    &series->solheadersize, series->solheader))
	return FALSE;

    memset(&gametmp, 0, sizeof gametmp);
    for (;;) {
	if (!readsolution(&series->savefile, &gametmp))
	    break;
	if (gametmp.sgflags & SGF_SETNAME) {
	    if (strcmp(gametmp.name, series->name)) {
		errmsg(series->name, "ignoring solution file %s as it was"
				     " recorded for a different level set: %s",
		       series->savefile.name, gametmp.name);
		series->gsflags |= GSF_NOSAVING;
		return FALSE;
	    }
	    series->gsflags |= GSF_SAVESETNAME;
	    continue;
	}
	i = findlevelinseries(series, gametmp.number, gametmp.passwd);
	if (i < 0) {
	    i = findlevelinseries(series, 0, gametmp.passwd);
	    if (i < 0) {
		fileerr(&series->savefile,
			"unmatched password in solution file");
		continue;
	    }
	    warn("level %d has been moved to level %d",
		 gametmp.number, series->games[i].number);
	}
	series->games[i].besttime = gametmp.besttime;
	series->games[i].sgflags = gametmp.sgflags;
	series->games[i].savedrndslidedir = gametmp.savedrndslidedir;
	series->games[i].savedstepping = gametmp.savedstepping;
	series->games[i].savedrndseed = gametmp.savedrndseed;
	copymovelist(&series->games[i].savedsolution, &gametmp.savedsolution);
    }
    destroymovelist(&gametmp.savedsolution);
    fileclose(&series->savefile, NULL);
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

    if (readonly || (series->gsflags & GSF_NOSAVING))
	return TRUE;

    if (series->savefile.fp)
	fileclose(&series->savefile, NULL);
    if (!series->savefile.name)
	series->savefile.name = series->savefilename;
    if (!series->savefile.name && (series->gsflags & GSF_NODEFAULTSAVE))
	return TRUE;
    if (!opensolutionfile(&series->savefile, series->filebase, TRUE))
	return FALSE;

    if (!writesolutionheader(&series->savefile, series->ruleset,
			     series->solheaderflags,
			     series->solheadersize, series->solheader))
	return fileerr(&series->savefile,
		       "saved-game file has become corrupted!");
    if (series->gsflags & GSF_SAVESETNAME) {
	if (!writesolutionsetname(&series->savefile, series->name))
	    return fileerr(&series->savefile,
			   "saved-game file has become corrupted!");
    }
    for (i = 0, game = series->games ; i < series->count ; ++i, ++game) {
	if (!writesolution(&series->savefile, game))
	    return fileerr(&series->savefile,
			   "saved-game file has become corrupted!");
    }

    fileclose(&series->savefile, NULL);
    return TRUE;
}
