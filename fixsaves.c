/*
 * Compile: gcc -Wall -o fixsaves fixsaves.c
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<dirent.h>

typedef	unsigned char	u8;
typedef	unsigned short	u16;
typedef	unsigned long	u32;

/***/

static char		infilename[512];
static char		outfilename[512];
static FILE	       *infp = NULL;
static FILE	       *outfp = NULL;

static int see(char const *dir, char const *file)
{
    sprintf(infilename, "%s/%s", dir, file);
    infp = fopen(infilename, "rb");
    if (infp)
	return 1;
    perror(infilename);
    *infilename = '\0';
    infp = NULL;
    return 0;
}

static int tell(char const *dir, char const *file)
{
    sprintf(outfilename, "%s/%s", dir, file);
    outfp = fopen(outfilename, "wb");
    if (outfp)
	return 1;
    perror(outfilename);
    *outfilename = '\0';
    outfp = NULL;
    return 0;
}

static void seen(void)
{
    *infilename = '\0';
    if (infp)
	fclose(infp);
}

static void told(void)
{
    *outfilename = '\0';
    if (outfp)
	fclose(outfp);
}

static int seekit(long offset, int whence)
{
    if (fseek(infp, offset, whence)) {
	perror(infilename);
	return 0;
    }
    return 1;
}

static int readit(void *ptr, int size)
{
    int	n;

    if (!size)
	return 1;
    n = fread(ptr, size, 1, infp);
    if (n == 0) {
	fprintf(stderr, "%s: unexpected EOF", infilename);
	return 0;
    } else if (n < 0) {
	perror(infilename);
	return 0;
    }
    return 1;
}

static int writeit(void const *ptr, int size)
{
    int	n;

    if (!size)
	return 1;
    n = fwrite(ptr, size, 1, outfp);
    if (n == 0) {
	fprintf(stderr, "%s: write error", outfilename);
	return 0;
    } else if (n < 0) {
	perror(outfilename);
	return 0;
    }
    return 1;
}

static int removeit(char const *dir, char const *file)
{
    int	n;

    sprintf(infilename, "%s/%s", dir, file);
    n = remove(infilename);
    if (n)
	perror(infilename);
    *infilename = '\0';
    return n == 0;
}

static int renameit(char const *dir, char const *old, char const *new)
{
    int	n;

    sprintf(infilename, "%s/%s", dir, old);
    sprintf(outfilename, "%s/%s", dir, new);
    n = rename(infilename, outfilename);
    if (n) {
	fprintf(stderr, "%s => ", infilename);
	perror(outfilename);
    }
    *infilename = '\0';
    *outfilename = '\0';
    return n == 0;
}

/***/

typedef	struct filenamestash {
    struct filenamestash       *next;
    char			filename[252];
} filenamestash;

static filenamestash	stash = { NULL, "" };

static int		unsuccessful = 0;
static int		successful = 0;


static void stashfilename(char const *filename)
{
    filenamestash      *s;

    for (s = &stash ; s->next ; s = s->next) ;
    strcpy(s->filename, filename);
    if ((s->next = malloc(sizeof *s)) == NULL) {
	fputs("OUT OF MEMORY\n", stderr);
	exit(1);
    }
    s->next->next = NULL;
    s->next->filename[0] = '\0';
}

static char const *getoldfilename(char const *filename)
{
    static char	buf[256];
    int		n;

    n = strlen(filename);
    if (filename[n - 4] == '.' && tolower(filename[n - 3]) == 't'
			       && tolower(filename[n - 2]) == 'w'
			       && tolower(filename[n - 1]) == 's')
	n -= 4;
    memcpy(buf, filename, n);
    memcpy(buf + n, ".old", 5);
    return buf;
}

static char const *getnewfilename(char const *filename)
{
    static char	buf[256];
    int		n;

    n = strlen(filename);
    if (filename[n - 4] == '.' && tolower(filename[n - 3]) == 't'
			       && tolower(filename[n - 2]) == 'w'
			       && tolower(filename[n - 1]) == 's')
	n -= 4;
    memcpy(buf, filename, n);
    memcpy(buf + n, ".new", 5);
    return buf;
}

/***/

typedef	struct leveldata {
    u16		number;
    u8		passwd[8];
} leveldata;

static int		levelcount = 0;
static leveldata       *levels = NULL;

static int getpasswd(u16 number, u8 *passwd)
{
    int	i;

    for (i = 0 ; i < levelcount ; ++i) {
	if (levels[i].number == number) {
	    memcpy(passwd, levels[i].passwd, 5);
	    return 1;
	}
    }
    return 0;
}

static char const *getdatfilename(char const *filename)
{
    static char	buf[256];
    int		n;

    n = strlen(filename);
    if (filename[n - 4] == '.' && tolower(filename[n - 3]) == 't'
			       && tolower(filename[n - 2]) == 'w'
			       && tolower(filename[n - 1]) == 's')
	n -= 4;
    memcpy(buf, filename, n);
    memcpy(buf + n, ".dat", 5);
    return buf;
}

static int readleveldata(void)
{
    u32		sig;
    u16		left, word;
    u8		id, byte;
    int		i, n;

    n = fread(&sig, 4, 1, infp);
    if (n == 0) {
	fprintf(stderr, "%s: file is empty\n", infilename);
	return 0;
    } else if (n < 0) {
	perror(infilename);
	return 0;
    } else if (sig != 0x0002AAACL && sig != 0x0102AAACL) {
	fprintf(stderr, "%s: file is not a CC .dat file\n", infilename);
	return 0;
    }
    if (!readit(&word, 2))
	return 0;
    if (word == 0 || word > 999) {
	fprintf(stderr, "%s: file has invalid header"
			" (claims to contain %u levels)\n", infilename, word);
	return 0;
    }
    if ((levels = realloc(levels, word * sizeof *levels)) == NULL) {
	fputs("OUT OF MEMORY\n", stderr);
	exit(1);
    }
    levelcount = (int)word;
    for (i = 0 ; i < levelcount ; ++i) {
	n = fread(&word, 2, 1, infp);
	if (n == 0)
	    break;
	if (n < 0) {
	    perror(infilename);
	    return 0;
	}
	if (!readit(&word, 2))
	    return 0;
	levels[i].number = word;
	if (!seekit(6, SEEK_CUR))
	    return 0;
	if (!readit(&word, 2) || !seekit(word, SEEK_CUR))
	    return 0;
	if (!readit(&word, 2) || !seekit(word, SEEK_CUR))
	    return 0;
	if (!readit(&word, 2))
	    return 0;
	left = word;
	while (left > 2) {
	    if (!readit(&id, 1) || !readit(&byte, 1))
		return 0;
	    left -= 2;
	    if (id == 6) {
		if (byte != 5) {
		    fprintf(stderr, "%s: file is corrupted (level %u"
				    " has password of length %d)\n",
			    infilename, levels[i].number, byte);
		    return 0;
		}
		if (!readit(levels[i].passwd, 5))
		    return 0;
		levels[i].passwd[0] ^= 0x99;
		levels[i].passwd[1] ^= 0x99;
		levels[i].passwd[2] ^= 0x99;
		levels[i].passwd[3] ^= 0x99;
	    } else {
		if (!seekit(byte, SEEK_CUR))
		    return 0;
	    }
	    left -= byte;
	}
	if (left != 0) {
	    fprintf(stderr, "%s: file metadata is inconsistent\n", infilename);
	    return 0;
	}
    }

    return 1;
}

/***/

static void *translatemoves(int *ptrsize)
{
    u8	       *outbuf;
    int		oldsize, newsize, alloc;
    u8		dir;
    long	delta;
    int		fours;
    u8		fourdirs;
    int		fourpos;
    u8		byte;
    u16		word;

    oldsize = *ptrsize;
    newsize = 0;
    alloc = 32;
    outbuf = malloc(32);
    if (!outbuf) {
	fputs("OUT OF MEMORY\n", stderr);
	exit(1);
    }

    fours = 0;
    while (oldsize) {
	if (!readit(&byte, 1))
	    return NULL;
	--oldsize;
	dir = byte & 3;
	delta = byte >> 3;
	if (byte & 4) {
	    if (!readit(&byte, 1))
		return NULL;
	    --oldsize;
	    delta += (long)(byte & ~1) << 4;
	    if (byte & 1) {
		if (!readit(&word, 2))
		    return NULL;
		oldsize -= 2;
		delta += (long)word << 12;
	    }
	}

	if (delta != 3) {
	    fours = 0;
	} else if (fours == 0) {
	    fours = 1;
	    fourpos = newsize;
	    fourdirs = dir << 2;
	} else if (fours == 1) {
	    fours = 2;
	    fourdirs |= dir << 4;
	} else {
	    newsize = fourpos;
	    outbuf[newsize++] = (u8)(fourdirs | (dir << 6));
	    fours = 0;
	    fourdirs = 0;
	    continue;
	}

	if (newsize + 4 > alloc) {
	    alloc *= 2;
	    if ((outbuf = realloc(outbuf, alloc)) == NULL) {
		fputs("OUT OF MEMORY\n", stderr);
		exit(1);
	    }
	}

	if (delta < (1 << 4)) {
	    outbuf[newsize++] = (u8)(0x01 | (dir << 2) | (delta << 4));
	} else if (delta < (1 << 12)) {
	    outbuf[newsize++] = (u8)(0x02 | (dir << 2)
					  | ((delta << 4) & 0xF0));
	    outbuf[newsize++] = (u8)(delta >> 4);
	} else {
	    outbuf[newsize++] = (u8)(0x03 | (dir << 2)
					  | ((delta << 4) & 0xF0));
	    outbuf[newsize++] = (u8)((delta >> 4) & 0xFF);
	    outbuf[newsize++] = (u8)((delta >> 12) & 0xFF);
	    outbuf[newsize++] = (u8)((delta >> 20) & 0xFF);
	}
    }

    *ptrsize = newsize;
    return outbuf;
}

/***/

typedef	struct filehead {
    u8		header[8];
} filehead;

typedef	struct levelhead08 {
    u8		number[2];
    u8		slidedir;
    u8		dummy;
    u8		seed[4];
    u8		time[4];
} levelhead08;

typedef	struct levelhead09 {
    u8		number[2];
    u8		passwd[4];
    u8		dummy;
    u8		slidedir;
    u8		seed[4];
    u8		time[4];
} levelhead09;

static int fixfile(void)
{
    filehead		header;
    levelhead08		oldlhead;
    levelhead09		newlhead;
    void	       *ptr;
    u32			dwrd;
    int			size;
    int			n;

    n = fread(&header, sizeof header, 1, infp);
    if (!n) {
	fprintf(stderr, "%s: file is empty; ignoring\n", infilename);
	return 0;
    } else if (n < 0) {
	perror(infilename);
	return -1;
    }
    if (header.header[0] != 0x00 || header.header[1] != 0x02
				 || header.header[2] != 0xAA
				 || header.header[3] != 0xAC) {
	fprintf(stderr, "%s: not an 0.8 save file; ignoring\n", infilename);
	return 0;
    }
    header.header[0] = 0x35;
    header.header[1] = 0x33;
    header.header[2] = 0x9B;
    header.header[3] = 0x99;
    if (!writeit(&header, sizeof header))
	return -1;
    for (;;) {
	n = fread(&dwrd, sizeof dwrd, 1, infp);
	if (n == 0)
	    break;
	if (n < 0) {
	    perror(infilename);
	    return -1;
	}
	if (dwrd == 0)
	    continue;
	if (dwrd < (int)(sizeof oldlhead)) {
	    fprintf(stderr, "%s: file is corrupt\n", infilename);
	    return -1;
	}
	if (!readit(&oldlhead, sizeof oldlhead))
	    return -1;
	n = *(u16*)&oldlhead;
	if (!getpasswd(n, newlhead.passwd)) {
	    fprintf(stderr, "%s: can't find level %d in the .dat file\n",
			    infilename, n);
	    return -1;
	}
	memcpy(&newlhead.number, &oldlhead.number, sizeof newlhead.number);
	memcpy(&newlhead.dummy, &oldlhead.dummy, sizeof newlhead.dummy);
	memcpy(&newlhead.slidedir, &oldlhead.slidedir,
	       sizeof newlhead.slidedir);
	memcpy(&newlhead.seed, &oldlhead.seed, sizeof newlhead.seed);
	memcpy(&newlhead.time, &oldlhead.time, sizeof newlhead.time);
	size = (int)(dwrd - sizeof oldlhead);
	ptr = translatemoves(&size);
	if (!ptr)
	    return -1;
	dwrd = size + sizeof newlhead;
	if (!writeit(&dwrd, sizeof dwrd)) {
	    free(ptr);
	    return -1;
	}
	if (!writeit(&newlhead, sizeof newlhead)) {
	    free(ptr);
	    return -1;
	}
	n = writeit(ptr, size);
	free(ptr);
	if (!n)
	    return -1;
    }

    return +1;
}

/***/

int main(int argc, char *argv[])
{
    char		savedirbuf[256];
    char const	       *datadir = NULL;
    char const	       *savedir = NULL;
    char const	       *path;
    DIR		       *dp;
    struct dirent      *dent;
    filenamestash      *s;
    int			n;

    for (n = 1 ; n < argc ; ++n) {
	if (argv[n][0] != '-' || (argv[n][1] != 'D' && argv[n][1] != 'S'))
	    return !printf("Usage: fixsaves [-D datadir] [-S savedir]\n");
	if (argv[n][1] == 'D')
	    datadir = argv[n][2] ? argv[n] + 2 : argv[++n];
	else
	    savedir = argv[n][2] ? argv[n] + 2 : argv[++n];
    }

    if (!datadir) {
#ifdef unix
	datadir = "/usr/local/share/tworld/data";
#else
	datadir = "data";
#endif
    }
    if (!savedir) {
	path = getenv("HOME");
	if (path) {
	    sprintf(savedirbuf, "%s/.tworld", path);
	    savedir = savedirbuf;
	} else
	    savedir = "./save";
    }

    if ((dp = opendir(savedir)) == NULL) {
	printf("%s: couldn't access directory", savedir);
	return 1;
    }
    while ((dent = readdir(dp)) != NULL) {
	if (dent->d_name[0] == '.')
	    continue;
	n = strlen(dent->d_name);
	if (!strcmp(dent->d_name + n - 4, ".tws")
			|| !strcmp(dent->d_name + n - 4, ".TWS"))
	    stashfilename(dent->d_name);
    }
    closedir(dp);

    for (s = &stash ; s ; s = s->next) {
	if (!*s->filename)
	    continue;
	if (!see(datadir, getdatfilename(s->filename))) {
	    fprintf(stderr, "can't read dat file for %s -- skipping\n",
			    s->filename);
	    *s->filename = '\0';
	    ++unsuccessful;
	    continue;
	}
	n = readleveldata();
	seen();
	if (!n) {
	    fprintf(stderr, "can't read dat file for %s -- skipping\n",
			    s->filename);
	    *s->filename = '\0';
	    ++unsuccessful;
	    continue;
	}
	if (!see(savedir, s->filename)) {
	    fprintf(stderr, "can't read %s -- skipping", s->filename);
	    *s->filename = '\0';
	    ++unsuccessful;
	    continue;
	}
	if (!tell(savedir, getnewfilename(s->filename))) {
	    fprintf(stderr, "can't open new %s -- skipping", s->filename);
	    *s->filename = '\0';
	    ++unsuccessful;
	    continue;
	}
	n = fixfile();
	seen();
	told();
	if (n <= 0) {
	    *s->filename = '\0';
	    if (n < 0)
		++unsuccessful;
	    continue;
	}
	if (!renameit(savedir, s->filename, getoldfilename(s->filename))) {
	    *s->filename = '\0';
	    ++unsuccessful;
	    continue;
	}
	if (!renameit(savedir, getnewfilename(s->filename), s->filename)) {
	    renameit(savedir, getoldfilename(s->filename), s->filename);
	    *s->filename = '\0';
	    ++unsuccessful;
	    continue;
	}

	++successful;
    }

    printf("Files transferred successfully:    %d\n"
	   "Files transferrred unsuccessfully: %d\n",
	   successful, unsuccessful);
    if (successful == 0)
	return !!unsuccessful;

    printf("Press ENTER to remove the old broken files,\n"
	   "otherwise hit Ctrl-C.\n");
    (void)getchar();

    for (s = &stash ; s ; s = s->next)
	if (*s->filename)
	    removeit(savedir, getoldfilename(s->filename));

    return !!unsuccessful;
}

