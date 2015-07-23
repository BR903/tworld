#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<stdarg.h>
#include	<string.h>
#include	<limits.h>
#include	<errno.h>

#define	TRUE	1
#define	FALSE	0

/* The default locations of everything.
 */
#ifdef _WIN32
#define	DIRSEP			'\\'
#define	DEFAULT_DATA_DIR	".\\data"
#define	DEFAULT_SETS_DIR	".\\sets"
#define	DEFAULT_SAVE_DIR	".\\save"
#else
#define	DIRSEP			'/'
#define	DEFAULT_DATA_DIR	"/usr/local/share/tworld/data"
#define	DEFAULT_SETS_DIR	"/usr/local/share/tworld/sets"
#define	DEFAULT_SAVE_DIR	"~/.tworld"
#endif

/* The various file signatures.
 */
static unsigned long const tws_sig = 0x999B3335;
static unsigned long const dat_sig = 0x0002AAAC;
static unsigned long const dac_sig = 0x656C6966;

/*
 * General-purpose functions
 */

#define	memerrexit()	(warn("out of memory"), exit(EXIT_FAILURE))
#define	xalloc(p, n)	(((p) = realloc((p), (n))) || (memerrexit(), NULL))

typedef	struct fileinfo {
    char const *name;
    FILE       *fp;
} fileinfo;

static int warn(char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    return FALSE;
}

static int warnfile(fileinfo const *file, char const *alt)
{
    if (alt) {
	if (errno)
	    warn("%s: %s", file->name, strerror(errno));
	else
	    warn("%s: %s", file->name, alt);
    }
    return FALSE;
}

static int fileopen(fileinfo *file, char const *name, char const *mode,
		    char const *msg)
{
    file->name = name;
    file->fp = fopen(name, mode);
    if (!file->fp)
	return warnfile(file, msg);
    return TRUE;
}

static int fileskip(fileinfo *file, int offset, char const *msg)
{
    return fseek(file->fp, offset, SEEK_CUR) ? warnfile(file, msg) : TRUE;
}

static int fileread(fileinfo *file, void *buf, unsigned long size,
		    char const *msg)
{
    return fread(buf, size, 1, file->fp) == 1 ? TRUE : warnfile(file, msg);
}

static int filereadint8(fileinfo *file, unsigned int *val, char const *msg)
{
    int	byte;

    if ((byte = fgetc(file->fp)) != EOF) {
	*val = (unsigned char)byte;
	return TRUE;
    }
    return warnfile(file, msg);
}

static int filereadint16(fileinfo *file, unsigned int *val, char const *msg)
{
    int	byte;

    if ((byte = fgetc(file->fp)) != EOF) {
	*val = byte & 0xFFU;
	if ((byte = fgetc(file->fp)) != EOF) {
	    *val |= (byte & 0xFFU) << 8;
	    return TRUE;
	}
    }
    return warnfile(file, msg);
}

static int filereadint32(fileinfo *file, unsigned long *val, char const *msg)
{
    int	byte;

    if ((byte = fgetc(file->fp)) != EOF) {
	*val = byte & 0xFFUL;
	if ((byte = fgetc(file->fp)) != EOF) {
	    *val |= (byte & 0xFFUL) << 8;
	    if ((byte = fgetc(file->fp)) != EOF) {
		*val |= (byte & 0xFFUL) << 16;
		if ((byte = fgetc(file->fp)) != EOF) {
		    *val |= (byte & 0xFFUL) << 24;
		    return TRUE;
		}
	    }
	}
    }
    return warnfile(file, msg);
}

static void fileclose(fileinfo *file, char const *msg)
{
    if (fclose(file->fp))
	warnfile(file, msg);
    file->fp = NULL;
}

/*
 * Functions for reading solution file data
 */

/* A single solution.
 */
typedef	struct solution {
    unsigned int	number;		/* level number */
    unsigned char	passwd[16];	/* level password */
    unsigned long	time;		/* solution time */
    unsigned long	score;		/* score for the level */
} solution;

/* A set of solutions.
 */
typedef	struct solutions {
    int			count;		/* total number of solutions */
    solution	       *solutions;	/* all the solutions */
    int			currentlevel;	/* most recently visited level */
    char		setname[256];	/* the name of the data file */
} solutions;

/* Parse the given file and return a filled-in solutions struct.
 */
static int readsolutions(char const *infilename, solutions *ss)
{
    fileinfo		file;
    solution	       *s;
    unsigned long	dword;
    unsigned int	word, byte;

    ss->count = 0;
    ss->solutions = NULL;
    ss->currentlevel = 0;
    *ss->setname = '\0';

    if (!fileopen(&file, infilename, "rb", "couldn't open"))
	return FALSE;

    /* The first four bytes of a .tws file contain the signature.
     */
    if (!filereadint32(&file, &dword, "not a solution file"))
	return FALSE;
    if (dword != tws_sig)
	return warnfile(&file, "not a .tws file");

    /* The next byte contains the ruleset.
     */
    if (!filereadint8(&file, &byte, "not a solution file"))
	return FALSE;
    if (byte != 2)
	warnfile(&file, "does not use the MS ruleset");

    /* The next two bytes contain the current level number.
     */
    if (!filereadint16(&file, &word, "not a solution file"))
	return FALSE;
    ss->currentlevel = word;

    /* The next byte contains the size of the remaining header.
     */
    if (!filereadint8(&file, &byte, "not a solution file"))
	return FALSE;
    if (byte)
	if (!fileskip(&file, byte, "not a solution file"))
	    return FALSE;

    /* After that come the individual solutions. The first four bytes
     * of every solution is a number (little-endian) that gives the
     * size of the solution data, not including the four bytes just
     * read. Note that it is permitted for the size to be zero, in
     * which case no further data follows for that individual
     * solution. Otherwise, the first two bytes of the solution data
     * is the number (again, little-endian) of the level, and the next
     * four bytes are the level password. If the size is greater than
     * six, then the solution time is stored as an int32 starting at
     * the sixteenth byte.
     */
    for (;;) {
	if (!filereadint32(&file, &dword, NULL))
	    break;
	if (dword == 0)
	    continue;
	if (dword < 6)
	    return warnfile(&file, "invalid solution data");
	++ss->count;
	xalloc(ss->solutions, ss->count * sizeof *ss->solutions);
	s = ss->solutions + ss->count - 1;
	if (!filereadint16(&file, &s->number, "invalid solution file"))
	    return FALSE;
	if (!fileread(&file, s->passwd, 4, "invalid solution file"))
	    return FALSE;
	dword -= 6;
	s->passwd[4] = '\0';
	s->time = 0;
	s->score = 0;
	if (dword >= 10) {
	    if (!fileskip(&file, 6, "invalid solution file"))
		return FALSE;
	    if (!filereadint32(&file, &s->time, "invalid solution file"))
		return FALSE;
	    dword -= 10;
	    if (!s->number && !*s->passwd) {
		--ss->count;
		if (dword > 0 && dword < 256)
		    if (!fileread(&file, ss->setname, dword,
				  "invalid solution file"))
			return FALSE;
		ss->setname[dword] = '\0';
		dword = 0;
	    }
	    if (!fileskip(&file, dword, "invalid solution file"))
		return FALSE;
	}
    }

    fileclose(&file, NULL);
    return TRUE;
}

/* Free all memory allocated by the solutions struct.
 */
static void freesolutions(solutions *ss)
{
    free(ss->solutions);
    ss->solutions = NULL;
    ss->count = 0;
}

/*
 * Functions for reading level set files
 */

/* A single level.
 */
typedef	struct level {
    unsigned int	number;		/* level number */
    unsigned int	time;		/* level time */
} level;

/* A set of levels.
 */
typedef	struct levels {
    int			count;		/* total number of levels */
    unsigned int	lastnumber;	/* number of the last level */
    level	       *levels;		/* all the levels */
} levels;

/* Parse the given file and return a filled-in levels struct.
 */
static int readlevels(char const *infilename, levels *ls)
{
    fileinfo		file;
    level	       *l;
    unsigned long	dword;
    unsigned int	word;

    ls->count = 0;
    ls->lastnumber = 0;
    ls->levels = NULL;

    if (!fileopen(&file, infilename, "rb", "couldn't open"))
	return FALSE;

    /* The first four bytes of a level set file contain the signature.
     */
    if (!filereadint32(&file, &dword, "not a level set file"))
	return FALSE;
    if ((dword & 0x00FFFFFF) != dat_sig)
	return warnfile(&file, "not a level set file");

    /* The next two bytes contain the number of the last level.
     */
    if (!filereadint16(&file, &ls->lastnumber, "not a solution file"))
	return FALSE;

    /* After that come the individual levels. The first two bytes of
     * every level is a number (little-endian) that gives the size of
     * the level data, not including the four bytes just read. The
     * next two bytes provide the level's number, and the next two
     * bytes provide the level's time limit, in seconds (or zero if
     * the level has no time limit).
     */
    for (;;) {
	if (!filereadint16(&file, &word, NULL))
	    break;
	if (word < 4)
	    return warnfile(&file, "invalid level set file");
	++ls->count;
	xalloc(ls->levels, ls->count * sizeof *ls->levels);
	l = ls->levels + ls->count - 1;
	if (!filereadint16(&file, &l->number, "invalid level set file"))
	    return FALSE;
	if (!filereadint16(&file, &l->time, "invalid level set file"))
	    return FALSE;
	if (!fileskip(&file, word - 4, "invalid level set file"))
	    return FALSE;
    }

    fileclose(&file, NULL);
    return TRUE;
}

/* Free all memory allocated by the levels struct.
 */
static void freelevels(levels *ss)
{
    free(ss->levels);
    ss->levels = NULL;
    ss->count = 0;
}

/*
 * Top-level functions
 */

static char datadir[4096] = DEFAULT_DATA_DIR;
static char setsdir[4096] = DEFAULT_SETS_DIR;
static char savedir[4096] = DEFAULT_SAVE_DIR;

static int findlevelsandsolutions(char const *filename,
				  levels *ls, solutions *ss)
{
    fileinfo		file;
    char		buf[8192];
    char const	       *homedir;
    unsigned long	dword;
    int			n;

    homedir = getenv("HOME");
    if (!homedir || !*homedir)
	homedir = ".";
    if (*datadir == '~') {
	snprintf(buf, sizeof buf, "%s", datadir + 1);
	snprintf(datadir, sizeof datadir, "%s%s", homedir, buf);
    }
    if (*setsdir == '~') {
	snprintf(buf, sizeof buf, "%s", setsdir + 1);
	snprintf(setsdir, sizeof setsdir, "%s%s", homedir, buf);
    }
    if (*savedir == '~') {
	snprintf(buf, sizeof buf, "%s", savedir + 1);
	snprintf(savedir, sizeof savedir, "%s%s", homedir, buf);
    }

    if (strchr(filename, DIRSEP))
	snprintf(buf, sizeof buf, "%s", filename);
    else
	snprintf(buf, sizeof buf, "%s%c%s", setsdir, DIRSEP, filename);
    if (!fileopen(&file, buf, "rb", NULL)) {
	if (!strchr(filename, DIRSEP)) {
	    snprintf(buf, sizeof buf, "%s", filename);
	    if (fileopen(&file, filename, "rb", NULL))
		goto okay;
	}
	return warnfile(&file, "couldn't open");
    }
  okay:
    if (!filereadint32(&file, &dword, "invalid level set file"))
	return FALSE;
    if (dword == dac_sig) {
	n = snprintf(buf, sizeof buf, "%s%c", datadir, DIRSEP);
	if (fscanf(file.fp, "=%255s", buf + n) != 1)
	    return warnfile(&file, "invalid configuration file");
	if (!fileopen(&file, buf, "rb", "couldn't open"))
	    return FALSE;
    }
    fileclose(&file, NULL);

    if (!readlevels(buf, ls))
	return FALSE;

    n = strlen(filename);
    if (filename[n - 4] == '.' && tolower(filename[n - 3]) == 'd'
			       && tolower(filename[n - 2]) == 'a'
			       && tolower(filename[n - 1]) == 't')
	n -= 4;

    if (strchr(filename, DIRSEP))
	snprintf(buf, sizeof buf, "%.*s.tws", n, filename);
    else
	snprintf(buf, sizeof buf, "%s%c%.*s.tws", savedir, DIRSEP,
						  n, filename);

    if (!readsolutions(buf, ss))
	return FALSE;

    return TRUE;
}

static void calculatetimesandscores(solutions *ss, levels const *ls)
{
    int const	tickspersec = 20;
    solution   *s;
    int		m, n;

    for (m = 0, s = ss->solutions ; m < ss->count ; ++m, ++s) {
	if (!s->time) {
	    s->score = 0;
	    continue;
	}
	for (n = 0 ; n < ls->count ; ++n)
	    if (ls->levels[n].number == s->number)
		break;
	if (n == ls->count) {
	    warn("found solution for nonexistent level %d", s->number);
	    continue;
	}
	s->score = s->number * 500;
	if (ls->levels[n].time) {
	    s->time = (ls->levels[n].time * tickspersec - s->time);
	    s->time = (s->time + tickspersec - 1) / tickspersec;
	    s->score += 10 * s->time;
	} else {
	    s->time = 0;
	}
    }
}

static void displaysolutioninfo(solutions *ss, char const *infilename)
{
    int	total, n;

    if (!ss->count) {
	printf("; (empty)\n");
	return;
    }

    if (infilename)
	printf("; %s\n\n", infilename);
    printf("[Chip's Challenge]\n");
    if (ss->currentlevel > 0)
	printf("Current Level=%d\n", ss->currentlevel);
    total = 0;
    for (n = 0 ; n < ss->count ; ++n) {
	printf("Level%u=%s",
	       ss->solutions[n].number, ss->solutions[n].passwd);
	if (ss->solutions[n].score)
	    printf(",%lu,%lu", ss->solutions[n].time, ss->solutions[n].score);
	putchar('\n');
	total += ss->solutions[n].score;
    }
    printf("Highest Level=%d\n", n);
    printf("Current Score=%d\n", total);
}

static void yowzitch(FILE *out)
{
    fprintf(out, "Usage: twstoini [-DLS DIR] SETNAME\n"
		 "  -L DIR   Read level sets from DIR [default=%s]\n"
		 "  -D DIR   Read data files from DIR [default=%s]\n"
		 "  -S DIR   Read solution files from DIR [default=%s]\n",
		 DEFAULT_SETS_DIR, DEFAULT_DATA_DIR, DEFAULT_SAVE_DIR);
}

int main(int argc, char *argv[])
{
    char const *setname = NULL;
    levels	ls;
    solutions	ss;
    int		n;

    if (argc == 1 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
	yowzitch(stdout);
	return EXIT_SUCCESS;
    }

    for (n = 1 ; n < argc ; ++n) {
	if (argv[n][0] == '-') {
	    switch (argv[n][1]) {
	      case 'D':
		strcpy(datadir, argv[n][2] ? &argv[n][2] : argv[++n]);
		break;
	      case 'L':
		strcpy(setsdir, argv[n][2] ? &argv[n][2] : argv[++n]);
		break;
	      case 'S':
		strcpy(savedir, argv[n][2] ? &argv[n][2] : argv[++n]);
		break;
	      default:
		yowzitch(stderr);
		return EXIT_FAILURE;
	    }
	} else {
	    if (setname) {
		yowzitch(stderr);
		return EXIT_FAILURE;
	    }
	    setname = argv[n];
	}
    }

    if (!findlevelsandsolutions(setname, &ls, &ss))
	return EXIT_FAILURE;
    calculatetimesandscores(&ss, &ls);
    displaysolutioninfo(&ss, setname);

    freelevels(&ls);
    freesolutions(&ss);
    return EXIT_SUCCESS;
}
