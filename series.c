/* series.c: Functions for finding and reading the data files.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"defs.h"
#include	"err.h"
#include	"fileio.h"
#include	"solution.h"
#include	"series.h"

/* The signature bytes of the data files.
 */
#define	SIG_DATFILE_0		0xAC
#define	SIG_DATFILE_1		0xAA
#define	SIG_DATFILE_2		0x02

#define	CCSIG_RULESET_MS	0x00
#define	CCSIG_RULESET_LYNX	0x01

/* The "signature bytes" of the configuration files.
 */
#define	SIG_CFGFILE_0		0x66
#define	SIG_CFGFILE_1		0x69
#define	SIG_CFGFILE_2		0x6C

/* Mini-structure for passing data in and out of findfiles().
 */
typedef	struct seriesdata {
    gameseries *list;		/* the gameseries list */
    int		allocated;	/* number of gameseries currently allocated */
    int		count;		/* number of gameseries filled in */
    int		usedatdir;	/* TRUE if the file is in seriesdatdir. */
} seriesdata;

/* The directory containing the series files (data files and
 * configuration files).
 */
char	       *seriesdir = NULL;

/* The directory containing the configured data files.
 */
char	       *seriesdatdir = NULL;

/*
 * File I/O functions, with error-handling specific to the data files.
 */

/* Read an unsigned 8-bit number.
 */
static int datfilereadint8(fileinfo *file, int *val, int *size,
			    unsigned char min, unsigned char max)
{
    unsigned char	val8;

    if (*size < 1)
	return fileerr(file, "invalid metadata in data file");
    if (!filereadint8(file, &val8, "invalid data in data file"))
	return FALSE;
    *val = (int)val8;
    --*size;
    if (val8 < min || val8 > max)
	return fileerr(file, "invalid data in data file");
    return TRUE;
}

/* Read an unsigned 16-bit number.
 */
static int datfilereadint16(fileinfo *file, int *val, int *size,
			    unsigned short min, unsigned short max)
{
    unsigned short	val16;

    if (*size < 2)
	return fileerr(file, "invalid metadata in data file");
    if (!filereadint16(file, &val16, "invalid data in data file"))
	return FALSE;
    *val = (int)val16;
    *size -= 2;
    if (val16 < min || val16 > max)
	return fileerr(file, "invalid data in data file");
    return TRUE;
}

/* Read a section of the data file into an allocated buffer.
 */
static int datfilereadbuf(fileinfo *file, unsigned char **buf, int bufsize,
			  int *size)
{
    if (*size < bufsize)
	return fileerr(file, "invalid metadata in data file");
    if (!(*buf = filereadbuf(file, bufsize, "invalid data in data file")))
	return FALSE;
    *size -= bufsize;
    return TRUE;
}

/*
 * Reading the data file.
 */

/* Examine the top of a data file and identify its type. FALSE is
 * returned if any header bytes appear to be invalid.
 */
static int readseriesheader(gameseries *series)
{
    unsigned char	magic[4];
    unsigned short	total;
    int			ruleset;

    if (!fileread(&series->mapfile, magic, 4, "not a valid data file"))
	return FALSE;
    if (magic[0] != SIG_DATFILE_0 || magic[1] != SIG_DATFILE_1
				  || magic[2] != SIG_DATFILE_2)
	return fileerr(&series->mapfile, "not a valid data file");
    switch (magic[3]) {
      case CCSIG_RULESET_MS:	ruleset = Ruleset_MS;	break;
      case CCSIG_RULESET_LYNX:	ruleset = Ruleset_Lynx;	break;
      default:
	fileerr(&series->mapfile, "unrecognized header in data file");
	return FALSE;
    }
    if (series->ruleset == Ruleset_None)
	series->ruleset = ruleset;
    if (!filereadint16(&series->mapfile, &total, "not a valid data file"))
	return FALSE;
    series->total = total;
    if (!series->total) {
	fileerr(&series->mapfile, "file contains no maps");
	return FALSE;
    }

    return TRUE;
}

/* Read a single level out of the given data file. The lists are turned
 * into arrays and the password is translated. The maps are left in their
 * compressed format.
 */
static int readlevelmap(fileinfo *file, gamesetup *game)
{
    unsigned char      *data;
    unsigned short	val16;
    int			levelsize, id, size, i;

    if (!filereadint16(file, &val16, NULL))
	return FALSE;
    levelsize = val16;
    if (!datfilereadint16(file, &game->number, &levelsize, 1, 999)
		|| !datfilereadint16(file, &game->time, &levelsize, 0, 999)
		|| !datfilereadint16(file, &game->chips, &levelsize, 0, 999))
	return FALSE;

    while (levelsize) {
	if (!datfilereadint8(file, &id, &levelsize, 1, 10))
	    return FALSE;
	if (id == 1) {
	    if (!datfilereadint8(file, &i, &levelsize, 0, 0)
			|| !datfilereadint16(file, &game->map1size,
						&levelsize, 0, 1024)
			|| !datfilereadbuf(file, &game->map1, game->map1size,
						&levelsize)
			|| !datfilereadint16(file, &game->map2size,
						&levelsize, 0, 1024)
			|| !datfilereadbuf(file, &game->map2, game->map2size,
						&levelsize)
			|| !datfilereadint16(file, &i, &levelsize, 0, 65535))
		return FALSE;
	} else {
	    if (!datfilereadint8(file, &size, &levelsize, 0, 255)
			|| !datfilereadbuf(file, &data, size, &levelsize))
		return FALSE;
	    switch (id) {
	      case 3:
		memcpy(game->name, data, size);
		game->name[size - 1] = '\0';
		break;
	      case 4:
		game->trapcount = size / 10;
		for (i = 0 ; i < game->trapcount ; ++i) {
		    game->traps[i].from = data[i * 10 + 0]
					+ data[i * 10 + 2] * CXGRID;
		    game->traps[i].to   = data[i * 10 + 4]
					+ data[i * 10 + 6] * CXGRID;
		}
		break;
	      case 5:
		game->clonercount = size / 8;
		for (i = 0 ; i < game->clonercount ; ++i) {
		    game->cloners[i].from = data[i * 8 + 0]
					  + data[i * 8 + 2] * CXGRID;
		    game->cloners[i].to   = data[i * 8 + 4]
					  + data[i * 8 + 6] * CXGRID;
		}
		break;
	      case 6:
		for (i = 0 ; i < size - 1 ; ++i)
		    game->passwd[i] = data[i] ^ 0x99;
		game->passwd[i] = '\0';
		break;
	      case 7:
		memcpy(game->hinttext, data, size);
		game->hinttext[size - 1] = '\0';
		break;
	      case 10:
		game->creaturecount = size / 2;
		for (i = 0 ; i < game->creaturecount ; ++i)
		    game->creatures[i] = data[i * 2 + 0]
				       + data[i * 2 + 1] * CXGRID;
		break;
	      default:
		return fileerr(file, "unrecognized field in data file");
	    }
	    free(data);
	    data = NULL;
	}
    }
    if (levelsize)
	warn("Level %d: %d bytes left over!", game->number, levelsize);
    return TRUE;
}

/* Assuming that the series passed in is in fact the original
 * chips.dat file, this function undoes the changes that MS introduced
 * to the original Lynx levels. A rather "ad hack" way to accomplish
 * this, but it permits this fixup to occur without requiring the user
 * to perform a special one-time task. Four passwords are repaired, a
 * (possibly) missing wall is restored, the beartrap wiring of level
 * 99 is fixed, and level 145 is removed.
 */
static int undomschanges(gameseries *series)
{
    if (series->total != 149)
	return FALSE;

    series->games[5].passwd[3] = 'P';
    series->games[9].passwd[0] = 'V';
    series->games[9].passwd[1] = 'U';
    series->games[27].passwd[3] = 'D';
    series->games[95].passwd[0] = 'W';
    series->games[95].passwd[1] = 'V';
    series->games[95].passwd[2] = 'H';
    series->games[95].passwd[3] = 'Y';

    series->games[98].traps[5].to = 14 * CXGRID + 8;
    series->games[98].traps[6].to = 14 * CXGRID + 23;
    series->games[98].traps[7].to = 16 * CXGRID + 8;
    series->games[98].traps[8].to = 16 * CXGRID + 23;
    series->games[98].traps[9].to = 18 * CXGRID + 16;
    series->games[98].traps[10].to = 20 * CXGRID + 6;
    series->games[98].traps[11].to = 20 * CXGRID + 16;
    series->games[98].traps[12].to = 23 * CXGRID + 23;
    series->games[98].traps[13].to = 25 * CXGRID + 23;
    series->games[110].traps[0].to = 11 * CXGRID + 22;
    series->games[110].traps[1].to = 6 * CXGRID + 15;

    series->games[87].map1[318] = 0x09;
    series->games[120].map1[395] = 0x00;
    series->games[126].map1[172] = 0x00;
    series->games[126].map1[184] = 0x01;
    series->games[126].map1[428] = 0x01;
    series->games[126].map1[440] = 0x00;

    free(series->games[144].map1);
    free(series->games[144].map2);
    memmove(series->games + 144, series->games + 145,
	    4 * sizeof *series->games);
    --series->total;
    --series->count;

    return TRUE;
}

/*
 * Functions to read the data files.
 */

/* Read the game file corresponding to series, until at least level
 * maps have been successfully loaded into memory or the end of the
 * data file is reached. The files are opened if they have not been
 * already.  Nothing is done if all requested levels are already
 * loaded. FALSE is returned if an error occurs.
 */
static int readlevelinseries(gameseries *series, int level)
{
    int	n;

    if (series->count > level)
	return TRUE;

    if (!(series->gsflags & GSF_ALLMAPSREAD)) {
	if (!series->mapfile.fp) {
	    if (!openfileindir(&series->mapfile, seriesdir,
			       series->mapfilename, "rb", "unknown error"))
		return FALSE;

	    if (!readseriesheader(series))
		return FALSE;
	}
	while (!(series->gsflags & GSF_ALLMAPSREAD)
						&& series->count <= level) {
	    while (series->count >= series->allocated) {
		n = series->allocated ? series->allocated * 2 : 16;
		xalloc(series->games, n * sizeof *series->games);
		memset(series->games + series->allocated, 0,
		       (n - series->allocated) * sizeof *series->games);
		series->allocated = n;
	    }
	    if (readlevelmap(&series->mapfile,
			     series->games + series->count))
		++series->count;
	    if (filetestend(&series->mapfile)) {
		fileclose(&series->mapfile, NULL);
		series->gsflags |= GSF_ALLMAPSREAD;
	    }
	}
    }
    return TRUE;
}

/* Load all levels from the given data file, and all of the user's
 * saved solutions.
 */
int readseriesfile(gameseries *series)
{
    if (series->gsflags & GSF_ALLMAPSREAD)
	return TRUE;
    if (series->total <= 0) {
	errmsg(series->filebase, "cannot read from empty level set");
	return FALSE;
    }
    xalloc(series->games, series->total * sizeof *series->games);
    memset(series->games + series->allocated, 0,
	   (series->total - series->allocated) * sizeof *series->games);
    series->allocated = series->total;
    if (!readlevelinseries(series, series->total - 1))
	return FALSE;
    if (series->gsflags & GSF_LYNXFIXES)
	undomschanges(series);
    readsolutions(series);
    return TRUE;
}

/* Free all memory allocated for the given gameseries.
 */
void freeseriesdata(gameseries *series)
{
    gamesetup  *game;
    int		n;

    fileclose(&series->solutionfile, NULL);
    fileclose(&series->mapfile, NULL);
    clearfileinfo(&series->solutionfile);
    clearfileinfo(&series->mapfile);
    free(series->mapfilename);
    series->mapfilename = NULL;
    series->gsflags = 0;
    series->solutionflags = 0;

    for (n = 0, game = series->games ; n < series->count ; ++n, ++game) {
	free(game->map1);
	free(game->map2);
	destroymovelist(&game->savedsolution);
    }
    free(series->games);
    series->games = NULL;
    series->allocated = 0;
    series->count = 0;
    series->total = 0;

    series->ruleset = Ruleset_None;
    series->gsflags = 0;
    *series->filebase = '\0';
    *series->name = '\0';
}

/*
 * Reading the configuration file.
 */

/* Parse the lines of the given configuration file. The return value
 * is the name of the corresponding data file, or NULL if the
 * configuration file could not be read or contained a syntax error.
 */
static char *readconfigfile(fileinfo *file, gameseries *series)
{
    static char	datfilename[256];
    char	buf[256];
    char	name[256];
    char	value[256];
    char       *p;
    int		lineno, n;

    n = sizeof buf - 1;
    if (!filegetline(file, buf, &n, "invalid configuration file"))
	return NULL;
    if (sscanf(buf, "file = %s", datfilename) != 1) {
	fileerr(file, "bad filename in configuration file");
	return NULL;
    }
    for (lineno = 2 ; ; ++lineno) {
	n = sizeof buf - 1;
	if (!filegetline(file, buf, &n, NULL))
	    break;
	for (p = buf ; isspace(*p) ; ++p) ;
	if (!*p || *p == '#')
	    continue;
	if (sscanf(buf, "%[^= \t] = %s", name, value) != 2) {
	    fileerr(file, "invalid configuration file syntax");
	    return NULL;
	}
	for (p = name ; (*p = tolower(*p)) != '\0' ; ++p) ;
	if (!strcmp(name, "name")) {
	    strcpy(series->name, value);
	} else if (!strcmp(name, "lastlevel")) {
	    n = (int)strtol(value, &p, 10);
	    if (*p || n <= 0) {
		fileerr(file, "invalid lastlevel in configuration file");
		return NULL;
	    }
	    series->final = n;
	} else if (!strcmp(name, "ruleset")) {
	    for (p = value ; (*p = tolower(*p)) != '\0' ; ++p) ;
	    if (strcmp(value, "ms") && strcmp(value, "lynx")) {
		fileerr(file, "invalid ruleset in configuration file");
		return NULL;
	    }
	    series->ruleset = *value == 'm' ? Ruleset_MS : Ruleset_Lynx;
	} else if (!strcmp(name, "usepasswords")) {
	    if (tolower(*value) == 'n')
		series->gsflags |= GSF_IGNOREPASSWDS;
	    else
		series->gsflags &= ~GSF_IGNOREPASSWDS;
	} else if (!strcmp(name, "fixlynx")) {
	    if (tolower(*value) == 'n')
		series->gsflags &= ~GSF_LYNXFIXES;
	    else
		series->gsflags |= GSF_LYNXFIXES;
	} else {
	    warn("line %d: directive \"%s\" unknown", lineno, name);
	    fileerr(file, "unrecognized setting in configuration file");
	    return NULL;
	}
    }

    return datfilename;
}

/*
 * Functions to locate the series files.
 */

/* Open the given file and read the information in the file header (or
 * the entire file if it is a configuration file), then allocate and
 * initialize a gameseries structure for the file and add it to the
 * list stored under the second argument. This function is used as a
 * findfiles() callback.
 */
static int getseriesfile(char *filename, void *data)
{
    fileinfo		file;
    unsigned char	magic[3];
    seriesdata	       *sdata = (seriesdata*)data;
    gameseries	       *series;
    char	       *datfilename;
    int			config, f;

    clearfileinfo(&file);
    if (!openfileindir(&file, seriesdir, filename, "rb", "unknown error"))
	return 0;
    if (!fileread(&file, &magic, 3, "unexpected EOF")) {
	fileclose(&file, NULL);
	return 0;
    }
    filerewind(&file, NULL);
    if (magic[0] == SIG_CFGFILE_0 && magic[1] == SIG_CFGFILE_1
				  && magic[2] == SIG_CFGFILE_2) {
	config = TRUE;
    } else if (magic[0] == SIG_DATFILE_0 && magic[1] == SIG_DATFILE_1
					 && magic[2] == SIG_DATFILE_2) {
	config = FALSE;
    } else {
	fileerr(&file, "not a valid data file or configuration file");
	fileclose(&file, NULL);
	return 0;
    }

    if (sdata->count >= sdata->allocated) {
	sdata->allocated = sdata->count + 1;
	xalloc(sdata->list, sdata->allocated * sizeof *sdata->list);
    }
    series = sdata->list + sdata->count;
    series->mapfilename = NULL;
    clearfileinfo(&series->solutionfile);
    series->gsflags = 0;
    series->solutionflags = 0;
    series->allocated = 0;
    series->count = 0;
    series->final = 0;
    series->ruleset = Ruleset_None;
    series->games = NULL;
    strncpy(series->filebase, filename, sizeof series->filebase - 1);
    series->filebase[sizeof series->filebase - 1] = '\0';
    strcpy(series->name, series->filebase);

    f = FALSE;
    if (config) {
	fileclose(&file, NULL);
	if (!openfileindir(&file, seriesdir, filename, "r", "unknown error"))
	    return 0;
	clearfileinfo(&series->mapfile);
	free(series->mapfilename);
	series->mapfilename = NULL;
	datfilename = readconfigfile(&file, series);
	fileclose(&file, NULL);
	if (datfilename) {
	    if (openfileindir(&series->mapfile, seriesdatdir,
			      datfilename, "rb", NULL))
		f = readseriesheader(series);
	    else
		warn("cannot use %s: %s unavailable", filename, datfilename);
	    fileclose(&series->mapfile, NULL);
	    clearfileinfo(&series->mapfile);
	    if (f)
		series->mapfilename = getpathforfileindir(seriesdatdir,
							  datfilename);
	}
    } else {
	series->mapfile = file;
	f = readseriesheader(series);
	fileclose(&series->mapfile, NULL);
	clearfileinfo(&series->mapfile);
	if (f)
	    series->mapfilename = getpathforfileindir(seriesdir, filename);
    }
    if (f)
	++sdata->count;
    return 0;
}

/* A callback function to compare two gameseries structures by
 * comparing their filenames.
 */
static int gameseriescmp(void const *a, void const *b)
{
    return strcmp(((gameseries*)a)->name, ((gameseries*)b)->name);
}

/* Search the series directory and generate an array of gameseries
 * structures corresponding to the data files found there. The array
 * is returned through list, and the size of the array is returned
 * through count. If preferred is not NULL, then the array returned
 * will only contain the series with that string as its filename
 * (presuming it can be found). The program will be aborted if a
 * serious error occurs or if no series can be found.
 */
static int getseriesfiles(char const *preferred, gameseries **list, int *count)
{
    seriesdata	s;
    int		n;

    s.list = NULL;
    s.allocated = 0;
    s.count = 0;
    s.usedatdir = FALSE;
    if (preferred && *preferred && haspathname(preferred)) {
	if (getseriesfile((char*)preferred, &s) < 0)
	    return FALSE;
	if (!s.count) {
	    errmsg(preferred, "couldn't read data file");
	    return FALSE;
	}
	*seriesdir = '\0';
	*savedir = '\0';
    } else {
	if (!*seriesdir)
	    return FALSE;
	if (!findfiles(seriesdir, &s, getseriesfile) || !s.count) {
	    errmsg(seriesdir, "directory contains no data files");
	    return FALSE;
	}
	if (preferred && *preferred) {
	    for (n = 0 ; n < s.count ; ++n) {
		if (!strcmp(s.list[n].name, preferred)) {
		    s.list[0] = s.list[n];
		    s.count = 1;
		    n = 0;
		    break;
		}
	    }
	    if (n == s.count) {
		errmsg(preferred, "no such data file");
		return FALSE;
	    }
	}
	if (s.count > 1)
	    qsort(s.list, s.count, sizeof *s.list, gameseriescmp);
    }
    *list = s.list;
    *count = s.count;
    return TRUE;
}

/* Produce a list of the series that are available for play. An array
 * of gameseries structures is returned through pserieslist, the size
 * of the array is returned through pcount, and a table of the the
 * filenames is returned through table. preferredfile, if not NULL,
 * limits the results to just the series with that filename.
 */
int createserieslist(char const *preferredfile, gameseries **pserieslist,
		     int *pcount, tablespec *table)
{
    static char const  *rulesetname[Ruleset_Count];
    gameseries	       *serieslist;
    char	      **ptrs;
    char	       *textheap;
    int			listsize;
    int			used, col, n, y;

    if (!getseriesfiles(preferredfile, &serieslist, &listsize))
	return FALSE;
    if (!table) {
	*pserieslist = serieslist;
	*pcount = listsize;
	return TRUE;
    }

    col = 8;
    for (n = 0 ; n < listsize ; ++n)
	if (col < (int)strlen(serieslist[n].name))
	    col = strlen(serieslist[n].name);
    if (col > 48)
	col = 48;

    rulesetname[Ruleset_Lynx] = "Lynx";
    rulesetname[Ruleset_MS] = "MS";
    ptrs = malloc((listsize + 1) * 2 * sizeof *ptrs);
    textheap = malloc((listsize + 1) * (col + 32));
    if (!ptrs || !textheap)
	memerrexit();

    n = 0;
    used = 0;
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1-Filename");
    ptrs[n++] = textheap + used;
    used += 1 + sprintf(textheap + used, "1.Ruleset");
    for (y = 0 ; y < listsize ; ++y) {
	ptrs[n++] = textheap + used;
	used += 1 + sprintf(textheap + used,
			    "1-%-*s", col, serieslist[y].name);
	ptrs[n++] = textheap + used;
	used += 1 + sprintf(textheap + used,
			    "1.%s", rulesetname[serieslist[y].ruleset]);
    }

    *pserieslist = serieslist;
    *pcount = listsize;
    table->rows = listsize + 1;
    table->cols = 2;
    table->sep = 2;
    table->collapse = 0;
    table->items = ptrs;
    return TRUE;
}

/* Make an independent copy of a single gameseries structure from list.
 */
void getseriesfromlist(gameseries *dest, gameseries const *list, int index)
{
    int	n;

    *dest = list[index];
    n = strlen(list[index].mapfilename) + 1;
    if (!(dest->mapfilename = malloc(n)))
	memerrexit();
    memcpy(dest->mapfilename, list[index].mapfilename, n);
}

/* Free all memory allocated by the createserieslist() table.
 */
void freeserieslist(gameseries *list, int count, tablespec *table)
{
    int	n;

    if (list) {
	for (n = 0 ; n < count ; ++n)
	    free(list[n].mapfilename);
	free(list);
    }
    if (table) {
	free((void*)table->items[0]);
	free(table->items);
    }
}

/*
 * Miscellaneous functions
 */

/* A function for looking up a specific level in a series by number
 * and/or password.
 */
int findlevelinseries(gameseries const *series, int number, char const *passwd)
{
    int	i, n;

    n = -1;
    if (number) {
	for (i = 0 ; i < series->total ; ++i) {
	    if (series->games[i].number == number) {
		if (!passwd || !strcmp(series->games[i].passwd, passwd)) {
		    if (n >= 0)
			return -1;
		    n = i;
		}
	    }
	}
    } else if (passwd) {
	for (i = 0 ; i < series->total ; ++i) {
	    if (!strcmp(series->games[i].passwd, passwd)) {
		if (n >= 0)
		    return -1;
		n = i;
	    }
	}
    } else {
	return -1;
    }
    return n;
}

/* Construct a small level for displaying at the very end of a series.
 */
gamesetup *enddisplaylevel(void)
{
    static unsigned char endmap1[] = {
	0x15, 0xFF, 0x03, 0x39, 0x15, 0x39, 0x15, 0x15, 0x39, 0xFF, 0x17, 0x00,
	0x39, 0xFF, 0x04, 0x15, 0xFF, 0x04, 0x39, 0xFF, 0x17, 0x00,
	0x39, 0xFF, 0x04, 0x15, 0xFF, 0x04, 0x39, 0xFF, 0x17, 0x00,
	0x15, 0xFF, 0x03, 0x39, 0x15, 0x39, 0x15, 0x15, 0x39, 0xFF, 0x17, 0x00,
	0xFF, 0x04, 0x15, 0x6E, 0xFF, 0x04, 0x15, 0xFF, 0x17, 0x00,
	0xFF, 0x04, 0x39, 0x15, 0xFF, 0x03, 0x39, 0x15, 0xFF, 0x17, 0x00,
	0x15, 0x39, 0x39, 0x15, 0x15, 0x39, 0x15, 0x15, 0x39, 0xFF, 0x17, 0x00,
	0x15, 0x39, 0x39, 0x15, 0x15, 0xFF, 0x03, 0x39, 0x15, 0xFF, 0x17, 0x00,
	0xFF, 0x04, 0x39, 0x15, 0x39, 0xFF, 0x03, 0x15, 0xFF, 0xFF, 0x00,
	0xFF, 0xFF, 0x00, 0xFF, 0xF9, 0x00
    };
    static unsigned char endmap2[] = {
	0xFF, 0x84, 0x00, 0x15, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0xFF, 0xFF, 0x00, 0xFF, 0x7E, 0x00
    };

    static gamesetup	ending;

    ending.map1size = sizeof endmap1;
    ending.map1 = endmap1;
    ending.map2size = sizeof endmap2;
    ending.map2 = endmap2;
    strcpy(ending.name, "CONGRATULATIONS!");
    return &ending;
}
