/* mklynxcc.c: Written by Brian Raiter, 2001.
 *
 * This is a quick hack that can be used to make .dat files that use
 * the Lynx ruleset. Run the program, giving it the name of a .dat
 * file, and the program will modify the .dat file to mark it as being
 * for the Lynx ruleset.
 *
 * As an extra bonus, the -x option will cause mklynxcc, when used on
 * the original chips.dat file, to also undo the changes introduced by
 * Microsoft -- namely, the removal of a wall from level 88 (though
 * some chips.dat do not have this alteration), and the correction of
 * the garbled passwords for levels 6, 10, 28, and 96.
 *
 * This source code is in the public domain.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>

#ifndef	TRUE
#define	TRUE	1
#endif
#ifndef	FALSE
#define	FALSE	0
#endif

/* The magic numbers of the .dat file.
 */
#define	CHIPS_SIGBYTE_1		0xAC
#define	CHIPS_SIGBYTE_2		0xAA
#define	CHIPS_SIGBYTE_3		0x02

/* Statistics for the chips.dat file.
 */
#define	CHIPS_DAT_SIZE		108569UL
#define	CHIPS_DAT_CHKSUM_1	4017619UL
#define	CHIPS_DAT_CHKSUM_2	4017628UL
#define	CHIPS_DAT_CHKXOR_1	209
#define	CHIPS_DAT_CHKXOR_2	216

/* Error return values.
 */
#define	ERR_BAD_CMDLINE		127
#define	ERR_NOT_DATFILE		126
#define	ERR_NOT_MS_DATFILE	125
#define	ERR_NOT_CHIPS_DATFILE	124

/* Online help.
 */
#define YOWZITCH	"Usage: mklynxcc [-x] FILENAME\n"

/* General values that I'm too lazy to pass around as arguments.
 */
static FILE		       *fp = NULL;
static char const	       *file = NULL;
static unsigned long		size = 0;
static unsigned long		offset = 0;
static unsigned long		chksum = 0;
static int			chkxor = 0;

/* Compute the .dat file's size, checksum, and check-xor values.
 */
static int examinefile(void)
{
    static unsigned char	buf[8192];
    unsigned char	       *p;
    long			n;

    chksum = 0;
    chkxor = 0;
    for (;;) {
	n = fread(buf, 1, sizeof buf, fp);
	if (n < 0) {
	    perror(file);
	    return FALSE;
	}
	if (n == 0)
	    break;
	offset += n;
	for (p = buf ; n ; --n, ++p) {
	    chksum += *p;
	    chkxor ^= *p;
	}
    }
    size = offset;

    rewind(fp);
    offset = 0;
    return TRUE;
}

/* Replace one byte in the file.
 */
static int changebyte(unsigned long pos, unsigned char byte)
{
    long	n;

    n = pos - offset;
    if (fseek(fp, n, SEEK_CUR)) {
	perror(file);
	return FALSE;
    }
    fputc(byte, fp);
    offset = pos + 1;
    return TRUE;
}


/* Return FALSE if the file is not a .dat file.
 */
static int sanitycheck(void)
{
    unsigned char	header[4];
    int			n;

    n = fread(header, 1, 4, fp);
    if (n < 0) {
	perror(file);
	return FALSE;
    }
    if (n < 4) {
	fprintf(stderr, "%s is not a valid .dat file\n", file);
	errno = ERR_NOT_DATFILE;
	return FALSE;
    }
    if (header[0] != CHIPS_SIGBYTE_1
     || header[1] != CHIPS_SIGBYTE_2
     || header[2] != CHIPS_SIGBYTE_3) {
	fprintf(stderr, "%s is not a valid .dat file\n", file);
	errno = ERR_NOT_DATFILE;
	return FALSE;
    }
    if (header[3] != 0) {
	if (header[3] == 1)
	    fprintf(stderr, "%s is already set to use the Lynx ruleset\n",
			    file);
	else
	    fprintf(stderr, "%s is not set for the MS ruleset\n", file);
	errno = ERR_NOT_MS_DATFILE;
	return FALSE;
    }

    rewind(fp);
    return TRUE;
}

/* Return FALSE if the file is not the original MS chips.dat file.
 */
static int ischipsdat(void)
{
    if (size != CHIPS_DAT_SIZE) {
	fprintf(stderr, 
		"File is the wrong size (expected %lu, found %lu).\n",
		CHIPS_DAT_SIZE, size);
	errno = ERR_NOT_CHIPS_DATFILE;
	return FALSE;
    }

    if (chksum == CHIPS_DAT_CHKSUM_1) {
	if (chkxor != CHIPS_DAT_CHKXOR_1) {
	    fprintf(stderr,
		    "File has the wrong xor (expected %d, found %d).\n",
		    CHIPS_DAT_CHKXOR_1, chkxor);
	    errno = ERR_NOT_CHIPS_DATFILE;
	    return FALSE;
	}
    } else if (chksum == CHIPS_DAT_CHKSUM_2) {
	if (chkxor != CHIPS_DAT_CHKXOR_2) {
	    fprintf(stderr,
		    "File has the wrong xor (expected %d, found %d).\n",
		    CHIPS_DAT_CHKXOR_2, chkxor);
	    errno = ERR_NOT_CHIPS_DATFILE;
	    return FALSE;
	}
    } else {
	fprintf(stderr,
		"File has the wrong sum (expected %lu or %lu, found %lu).\n",
		CHIPS_DAT_CHKSUM_1, CHIPS_DAT_CHKSUM_2, chksum);
	errno = ERR_NOT_CHIPS_DATFILE;
	return FALSE;
    }

    return TRUE;
}

/* Change the bytes that differ from the original Lynx data.
 */
static int fixchips(void)
{
    if (!changebyte(0x007F7UL, 'P' ^ 0x99))	/* fix level 6 password */
	return FALSE;
    if (!changebyte(0x01128UL, 'V' ^ 0x99)	/* fix level 10 password */
     || !changebyte(0x01129UL, 'U' ^ 0x99))
	return FALSE;
    if (!changebyte(0x0411CUL, 'D' ^ 0x99))	/* fix level 28 password */
	return FALSE;
    if (!changebyte(0x0EC5CUL, 0x09))		/* replace wall in level 88 */
	return FALSE;
    if (!changebyte(0x105FEUL, 'W' ^ 0x99)	/* fix level 96 password */
     || !changebyte(0x105FFUL, 'V' ^ 0x99)
     || !changebyte(0x10600UL, 'H' ^ 0x99)
     || !changebyte(0x10601UL, 'Y' ^ 0x99))
	return FALSE;

    rewind(fp);
    offset = 0;
    return TRUE;
}

/*
 *
 */

int main(int argc, char *argv[])
{
    int		fullfix = FALSE;
    int		n;

    if (argc > 1 && !strcmp(argv[1], "-h")) {
	fputs(YOWZITCH, stdout);
	return 0;
    }

    errno = 0;

    n = 1;
    if (argc > n && !strcmp(argv[n], "-x")) {
	fullfix = TRUE;
	++n;
    }
    if (argc > n) {
	file = argv[n];
	++n;
    }
    if (argc > n || !file) {
	fputs(YOWZITCH, stderr);
	return ERR_BAD_CMDLINE;
    }

    fp = fopen(file, "r+b");
    if (!fp) {
	perror(file);
	return errno;
    }
    if (!sanitycheck())
	return errno;

    if (fullfix) {
	examinefile();
	if (!ischipsdat())
	    return errno;
	if (!fixchips()) {
	    fprintf(stderr, "ERROR: file may have become corrupt\n");
	    return errno;
	}
    }

    if (!changebyte(3, 1))
	return errno;

    fclose(fp);
    printf("%s was successfully altered\n", file);
    return errno;
}
