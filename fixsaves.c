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

static void *translatemoves(int *ptrsize)
{
    u8		       *outbuf;
    int			oldsize, newsize, alloc;
    u8			tag, dir;
    unsigned long	delta;
    u8			byte;
    u16			word;

    oldsize = *ptrsize;
    newsize = 0;
    alloc = 32;
    outbuf = malloc(32);
    if (!outbuf) {
	fputs("OUT OF MEMORY\n", stderr);
	exit(1);
    }

    while (oldsize) {
	if (!readit(&byte, 1))
	    return NULL;
	--oldsize;
	tag = byte & 3;
	dir = (byte >> 2) & 0x03;
	delta = (byte >> 4) & 0x0F;
	if (tag > 1) {
	    if (!readit(&byte, 1))
		return NULL;
	    --oldsize;
	    delta |= (unsigned int)byte << 4;
	    if (tag > 2) {
		if (!readit(&word, 2))
		    return NULL;
		oldsize -= 2;
		delta |= (unsigned long)word << 12;
	    }
	}

	if ((tag == 1 && delta >= 0x08) || (tag == 2 && delta >= 0x800))
	    ++tag;

	if (newsize + 4 > alloc) {
	    alloc *= 2;
	    if ((outbuf = realloc(outbuf, alloc)) == NULL) {
		fputs("OUT OF MEMORY\n", stderr);
		exit(1);
	    }
	}

	switch (tag) {
	  case 0:
	    outbuf[newsize++] = byte;
	    break;
	  case 1:
	    outbuf[newsize++] = (u8)(tag | (dir << 2) | (delta << 5));
	    break;
	  case 2:
	    outbuf[newsize++] = (u8)(tag | (dir << 2) | ((delta << 5) & 0xE0));
	    outbuf[newsize++] = (u8)((delta >> 3) & 0xFF);
	    break;
	  case 3:
	    outbuf[newsize++] = (u8)(tag | (dir << 2) | ((delta << 5) & 0xE0));
	    outbuf[newsize++] = (u8)((delta >> 3) & 0xFF);
	    outbuf[newsize++] = (u8)((delta >> 11) & 0xFF);
	    outbuf[newsize++] = (u8)((delta >> 19) & 0xFF);
	    break;
	}
    }

    *ptrsize = newsize;
    return outbuf;
}

/***/

typedef	struct filehead {
    u8		header[8];
} filehead;

typedef	struct levelhead {
    u8		number[2];
    u8		passwd[4];
    u8		dummy;
    u8		slidedir;
    u8		seed[4];
    u8		time[4];
} levelhead;

static int fixfile(void)
{
    filehead		header;
    levelhead		lhead;
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
    if (header.header[0] != 0x35 || header.header[1] != 0x33
				 || header.header[2] != 0x9B
				 || header.header[3] != 0x99) {
	fprintf(stderr, "%s: not a save file; ignoring\n", infilename);
	return 0;
    }
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
	if (dwrd <= (int)(sizeof lhead)) {
	    if (!readit(&lhead, dwrd) || !writeit(&dwrd, sizeof dwrd)
				      || !writeit(&lhead, dwrd))
		return -1;
	    continue;
	}
	if (!readit(&lhead, sizeof lhead))
	    return -1;
	size = (int)(dwrd - sizeof lhead);
	ptr = translatemoves(&size);
	if (!ptr)
	    return -1;
	dwrd = size + sizeof lhead;
	if (!writeit(&dwrd, sizeof dwrd)) {
	    free(ptr);
	    return -1;
	}
	if (!writeit(&lhead, sizeof lhead)) {
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
    char		savedir[256] = "";
    char const	       *path;
    DIR		       *dp;
    struct dirent      *dent;
    filenamestash      *s;
    int			n;

    for (n = 1 ; n < argc ; ++n) {
	if (argv[n][0] != '-' || argv[n][1] != 'S')
	    return !printf("Usage: fixsaves [-S savedir]\n");
	strcpy(savedir, argv[n][2] ? argv[n] + 2 : argv[++n]);
    }
    if (!*savedir) {
	path = getenv("HOME");
	if (path)
	    sprintf(savedir, "%s/.tworld", path);
	else
	    strcpy(savedir, "./save");
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

