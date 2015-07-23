#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<limits.h>
#include	<errno.h>

#define	TRUE	1
#define	FALSE	0

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

static int filewriteint32(fileinfo *file, unsigned long val, char const *msg)
{
    if (fputc(val & 0xFF, file->fp) != EOF
			&& fputc((val >> 8) & 0xFF, file->fp) != EOF
			&& fputc((val >> 16) & 0xFF, file->fp) != EOF
			&& fputc((val >> 24) & 0xFF, file->fp) != EOF)
	return TRUE;
    return warnfile(file, msg);
}

static int filewrite(fileinfo *file, void *buf, unsigned long size,
		     char const *msg)
{
    return fwrite(buf, size, 1, file->fp) == 1 ? TRUE : warnfile(file, msg);
}

static void fileclose(fileinfo *file, char const *msg)
{
    if (fclose(file->fp))
	warnfile(file, msg);
    file->fp = NULL;
}

/*
 * Functions for reading and writing solution file data
 */

/* A single solution.
 */
typedef	struct solution {
    unsigned long	size;		/* size of solution data in bytes */
    unsigned short	number;		/* level number */
    unsigned char      *bytes;		/* the raw solution data */
} solution;

/* A set of solutions.
 */
typedef	struct solutions {
    unsigned char	header[8];	/* solution file header data */
    int			count;		/* total number of solutions */
    solution	       *solutions;	/* all the solutions */
    char		setname[256];	/* the name of the data file */
} solutions;

/* The .tws file signature.
 */
static unsigned char const tws_sig[] = { 0x35, 0x33, 0x9B, 0x99 };

/* Parse the given file and return a filled-in solutions struct.
 */
static int readsolutions(char const *infilename, solutions *ss)
{
    fileinfo		file;
    solution	       *s;
    unsigned long	size;

    ss->count = 0;
    ss->solutions = NULL;
    *ss->setname = '\0';

    if (!fileopen(&file, infilename, "rb", "couldn't open"))
	return FALSE;

    /* The first eight bytes of a .tws file contain the signature and
     * the header data.
     */
    if (!fileread(&file, ss->header, sizeof ss->header, "not a .tws file"))
	return FALSE;
    if (memcmp(ss->header, tws_sig, sizeof tws_sig))
	return warnfile(&file, "not a .tws file");

    /* The last byte of the header data indicates the size of the
     * remaining header, if any.
     */
    size = ss->header[7];
    if (size)
	if (!fileskip(&file, size, "not a .tws file"))
	    return FALSE;

    /* After that come the individual solutions. The first four bytes
     * of every solution is a number (little-endian) that gives the
     * size of the solution data, not including the four bytes just
     * read. Note that it is permitted for the size to be zero, in
     * which case no further data follows for that individual
     * solution. Otherwise, the first two bytes of the solution data
     * is the number (again, little-endian) of the level. (In the
     * particular case when the size is six, then the solution only
     * contains the level number and a password.)
     */
    for (;;) {
	if (!filereadint32(&file, &size, NULL))
	    break;
	if (size == 0)
	    continue;
	++ss->count;
	xalloc(ss->solutions, ss->count * sizeof *ss->solutions);
	s = ss->solutions + ss->count - 1;
	s->size = size;
	s->number = 0;
	s->bytes = NULL;
	xalloc(s->bytes, s->size);
	if (!fileread(&file, s->bytes, s->size, "invalid .tws file"))
	    return FALSE;
	s->number = s->bytes[0] | (s->bytes[1] << 8);
	if (!s->number && !s->bytes[2] && !s->bytes[3]
		       && !s->bytes[4] && !s->bytes[5]) {
	    if (size > 16 && size - 16 < 256)
		strcpy(ss->setname, (char*)s->bytes + 16);
	    free(s->bytes);
	    --ss->count;
	}
    }

    fileclose(&file, NULL);
    return TRUE;
}

/* Free all memory allocated by the solutions struct.
 */
static void freesolutions(solutions *ss)
{
    int	n;

    for (n = 0 ; n < ss->count ; ++n)
	free(ss->solutions[n].bytes);
    free(ss->solutions);
    ss->solutions = NULL;
    ss->count = 0;
}

/* Write the initial contents of a solution file.
 */
static int startsolutionfile(fileinfo *file, solutions *ss)
{
    if (!filewrite(file, ss->header, sizeof ss->header, "couldn't write"))
	return FALSE;
    if (ss->header[7])
	if (!filewrite(file, ss->setname, ss->header[7], "couldn't write"))
	    return FALSE;
    return TRUE;
}

/* Add the given solution to the output file.
 */
static int appendsolution(fileinfo *file, solution *s)
{
    if (!filewriteint32(file, s->size, "couldn't write"))
	return FALSE;
    if (s->size)
	if (!filewrite(file, s->bytes, s->size, "couldn't write"))
	    return FALSE;
    return TRUE;
}

/* Add the solution for the given level to the output file.
 */
static int appendlevel(fileinfo *file, solutions *ss, int number)
{
    solution   *s;

    for (s = ss->solutions ; s < ss->solutions + ss->count ; ++s)
	if (s->number == number)
	    return appendsolution(file, s);

    warn("no level numbered %d in %s", number, file->name);
    return FALSE;
}

/* Write the final contents of a solution file.
 */
static int endsolutionfile(fileinfo *file, solutions *ss)
{
    (void)file;
    (void)ss;
    return TRUE;
}

/*
 * Top-level functions
 */

static void displaysolutioninfo(solutions *ss)
{
    int	first, last, n;
    int	comma = FALSE;

    if (!ss->count) {
	printf("empty solution file\n");
	return;
    }

    if (ss->setname[0])
	printf("%s: ", ss->setname);
    first = -1;
    last = -1;
    for (n = 0 ; n < ss->count ; ++n) {
	if (ss->solutions[n].number == last + 1) {
	    ++last;
	    continue;
	}
	if (last > 0) {
	    if (comma)
		putchar(',');
	    if (first == last)
		printf("%d", last);
	    else
		printf("%d-%d", first, last);
	    comma = TRUE;
	}
	first = last = ss->solutions[n].number;
    }
    if (comma)
	putchar(',');
    if (first == last)
	printf("%d\n", last);
    else
	printf("%d-%d\n", first, last);
}

static int writesolutionset(solutions *ss, char *string)
{
    static char	       *outfilename = NULL;
    fileinfo		file;
    char	       *setinfo;
    char	       *p;
    unsigned long	number;
    int			n;

    xalloc(outfilename, strlen(string) + 1);
    strcpy(outfilename, string);

    p = strrchr(outfilename, ':');
    if (!p) {
	warn("syntax error in \"%s\"", string);
	return FALSE;
    }
    setinfo = p + 1;
    *p = '\0';
    if (!fileopen(&file, outfilename, "wb", "couldn't open"))
	return FALSE;
    if (!startsolutionfile(&file, ss))
	return FALSE;

    for (;;) {
	number = strtoul(setinfo, &p, 10);
	if (number == 0 || number > USHRT_MAX) {
	    warn("invalid level number at \"... %s\"", setinfo);
	    return FALSE;
	}
	if (*p == '-') {
	    n = number;
	    number = strtoul(p + 1, &p, 10);
	    if (number == 0 || number > USHRT_MAX) {
		warn("invalid level number at \"... %s\"", setinfo);
		return FALSE;
	    }
	    if ((int)number < n) {
		warn("invalid range at \"... %s\"", setinfo);
		return FALSE;
	    }
	    while (n <= (int)number)
		if (!appendlevel(&file, ss, n++))
		    return FALSE;
	} else {
	    if (!appendlevel(&file, ss, (int)number))
		return FALSE;
	}
	if (*p == '\0')
	    break;
	if (*p != ',') {
	    warn("syntax error at \"... %s\"", setinfo);
	    return FALSE;
	}
	setinfo = p + 1;
    }

    if (!endsolutionfile(&file, ss))
	return FALSE;
    fileclose(&file, "write error");
    return TRUE;
}

static int explodesolutions(char const *infilename, solutions *ss)
{
    fileinfo	file;
    char       *outfilename = NULL;
    int		suffixed, len;
    int		n;

    len = strlen(infilename);
    xalloc(outfilename, len + 8);
    suffixed = len > 4 && !strcmp(infilename + len - 4, ".tws");
    if (suffixed)
	len -= 4;
    memcpy(outfilename, infilename, len);

    for (n = 0 ; n < ss->count ; ++n) {
	if (ss->solutions[n].size == 0 || ss->solutions[n].size == 6)
	    continue;
	if (suffixed)
	    sprintf(outfilename + len, "-%03d.tws", ss->solutions[n].number);
	else
	    sprintf(outfilename + len, "-%03d", ss->solutions[n].number);
	if (!fileopen(&file, outfilename, "wb", "couldn't open"))
	    continue;
	if (startsolutionfile(&file, ss))
	    if (appendsolution(&file, ss->solutions + n))
		endsolutionfile(&file, ss);
	fileclose(&file, "write error");
    }

    free(outfilename);
    return TRUE;
}

static void yowzitch(FILE *out)
{
    fputs("Usage: solex INPUT.tws OUTPUT.tws:NUMBERS [...]\n"
	  "       solex INPUT.tws all\n"
	  "       solex INPUT.tws\n"
	  "\n"
	  "NUMBERS selects the solutions to copy to OUTPUT.tws. Examples:\n"
	  "    OUTPUT.tws:144              copies the solution to one level\n"
	  "    OUTPUT.tws:99-102           copies 4 contiguous solutions\n"
	  "    OUTPUT.tws:4,7,33,66        copies 4 discontinguous solutions\n"
	  "    OUTPUT.tws:1-8,34,145-149   copies 14 solutions total\n"
	  "\n"
	  "\"all\" causes each solution to be copied to a separate file,\n"
	  "each named INPUT_NNN.tws, where NNN is the level number.\n",
	  out);
}

int main(int argc, char *argv[])
{
    solutions	ss;
    int		n;

    if (argc == 1 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
	yowzitch(stdout);
	return EXIT_SUCCESS;
    }

    if (!readsolutions(argv[1], &ss))
	return EXIT_FAILURE;

    if (argc == 2) {
	displaysolutioninfo(&ss);
    } else if (argc == 3 && !strcmp(argv[2], "all")) {
	if (!explodesolutions(argv[1], &ss))
	    return EXIT_FAILURE;
    } else {
	for (n = 2 ; n < argc ; ++n)
	    if (!writesolutionset(&ss, argv[n]))
		return EXIT_FAILURE;
    }

    freesolutions(&ss);
    return EXIT_SUCCESS;
}
