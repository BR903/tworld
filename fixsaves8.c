/*
 * Compile: gcc -Wall -o fixsaves fixsaves.c
 * Then copy the binary to your ~/.tworld directory and run it.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<dirent.h>

typedef	unsigned char	uchar;

/***/

typedef	struct filenamestash {
    struct filenamestash       *next;
    char			filename[508];
} filenamestash;

static filenamestash	stash = { NULL, "" };

static void stashfilename(char const *filename)
{
    filenamestash      *s;

    for (s = &stash ; s->next ; s = s->next) ;
    strcpy(s->filename, filename);
    if (!(s->next = malloc(sizeof *s))) {
	fputs("OUT OF MEMORY\n", stderr);
	exit(1);
    }
    s->next->next = NULL;
    s->next->filename[0] = '\0';
}

/***/

typedef	struct filehead {
    uchar	header[8];
} filehead;

typedef	struct oldlhead {
    uchar	time[4];
    uchar	seed[4];
    uchar	slidedir;
} oldlevelhead;

typedef	struct newlhead {
    short	number;
    uchar	slidedir;
    uchar	dummy;
    uchar	seed[4];
    uchar	time[4];
} newlevelhead;

static uchar	buf[65536];

static int fixfile(char const *infilename, char const *outfilename)
{
    FILE	       *fpin, *fpout;
    filehead		header;
    oldlevelhead	oldlhead;
    newlevelhead	newlhead;
    unsigned int	size, newsize;
    short		number;
    int			n;

    if (!(fpin = fopen(infilename, "rb"))) {
	perror(infilename);
	return -1;
    }
    n = fread(&header, sizeof header, 1, fpin);
    if (!n) {
	fprintf(stderr, "%s: file is empty; ignoring", infilename);
	fclose(fpin);
	return 0;
    } else if (n < 0) {
	perror(infilename);
	fclose(fpin);
	return -1;
    }
    if (!(fpout = fopen(outfilename, "wb"))) {
	perror(outfilename);
	fclose(fpin);
	return -1;
    }
    if (fwrite(&header, sizeof header, 1, fpout) != 1) {
	perror(outfilename);
	return -1;
    }
    for (number = 1 ; ; ++number) {
	n = fread(&size, 4, 1, fpin);
	if (n == 0)
	    break;
	if (n < 0) {
	    perror(infilename);
	    return -1;
	}
	if (size == 0)
	    continue;
	if (size < 9) {
	    fprintf(stderr, "%s: file is corrupt", infilename);
	    return -1;
	}
	n = fread(&oldlhead, 9, 1, fpin);
	if (n == 0) {
	    fprintf(stderr, "%s: file is corrupt", infilename);
	    return -1;
	} else if (n < 0) {
	    perror(infilename);
	    return -1;
	}
	newsize = size + 3;
	newlhead.number = number;
	newlhead.slidedir = oldlhead.slidedir;
	newlhead.dummy = 0;
	memcpy(&newlhead.seed, &oldlhead.seed, 4);
	memcpy(&newlhead.time, &oldlhead.time, 4);
	size -= 9;
	if (size > 65536) {
	    fprintf(stderr, "%s: level %d is too damn big!",
			    infilename, (int)number);
	    return -1;
	}
	n = fread(buf, size, 1, fpin);
	if (n < 0) {
	    perror(infilename);
	    return -1;
	} else if (n != 1) {
	    fprintf(stderr, "%s: file is corrupt", infilename);
	    return -1;
	}
	if (fwrite(&newsize, 4, 1, fpout) != 1
		|| fwrite(&newlhead, 12, 1, fpout) != 1
		|| fwrite(buf, 1, size, fpout) != size) {
	    perror(outfilename);
	    return -1;
	}
    }
    fclose(fpin);
    fclose(fpout);
    return +1;
}

static char const *newfilename(char const *oldfilename)
{
    static char	buf[8192];
    int		n;

    n = strlen(oldfilename);
    if (oldfilename[n - 4] == '.' && tolower(oldfilename[n - 3]) == 'd'
				  && tolower(oldfilename[n - 2]) == 'a'
				  && tolower(oldfilename[n - 1]) == 't')
	n -= 4;
    memcpy(buf, oldfilename, n);
    memcpy(buf + n, ".tws", 5);
    return buf;
}

int main(int argc, char *argv[])
{
    char const	       *dirname;
    DIR		       *dp;
    struct dirent      *dent;
    filenamestash      *s;
    int			unsuccessful = 0;
    int			successful = 0;
    int			n;

    if ((argc == 2 && !strcmp(argv[1], "-h")) || argc > 2) {
	printf("Usage: %s [DIRECTORY]\n"
	       "With no arguments, operates on the current directory.\n",
	       argv[0]);
	return 0;
    }

    dirname = argc == 1 ? "." : argv[1];
    if (!(dp = opendir(dirname))) {
	printf("%s: couldn't access directory", dirname);
	return 1;
    }

    while ((dent = readdir(dp))) {
	if (dent->d_name[0] == '.')
	    continue;
	n = strlen(dent->d_name);
	if (!strcmp(dent->d_name + n - 4, ".dat")
			|| !strcmp(dent->d_name + n - 4, ".DAT"))
	    stashfilename(dent->d_name);
    }

    for (s = &stash ; s ; s = s->next) {
	if (!*s->filename)
	    continue;
	n = fixfile(s->filename, newfilename(s->filename));
	if (n > 0) {
	    ++successful;
	} else {
	    s->filename[0] = '\0';
	    if (n < 0)
		++unsuccessful;
	}
    }
    closedir(dp);

    printf("Files transferred successfully:    %d\n"
	   "Files transferrred unsuccessfully: %d\n",
	   successful, unsuccessful);
    if (!successful)
	return 0;
    printf("Press ENTER to remove the old broken files,\n"
	   "otherwise hit Ctrl-C.\n");
    (void)getchar();
    for (s = &stash ; s ; s = s->next)
	if (*s->filename)
	    remove(s->filename);

    return 0;
}

