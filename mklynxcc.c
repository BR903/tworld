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
#include	<sys/stat.h>

#define	CHIPS_DAT_SIZE		108569
#define	CHIPS_DAT_CHKSUM	4017580
#define	CHIPS_DAT_CHKXOR	202

static FILE	       *fp = NULL;
static char const      *file = NULL;
static unsigned char   *buf = NULL;
static long		size = 0;

static void fixchips(void)
{
    int	n, y, z;

    buf[0x0EC5C] = 0x09;	/* replace wall in level 88 */
    buf[0x007F7] = 'P' ^ 0x99;	/* fix level 6 password */
    buf[0x01128] = 'V' ^ 0x99;	/* fix level 10 password */
    buf[0x01129] = 'U' ^ 0x99;
    buf[0x0411C] = 'D' ^ 0x99;	/* fix level 28 password */
    buf[0x105FE] = 'W' ^ 0x99;	/* fix level 96 password */
    buf[0x105FF] = 'V' ^ 0x99;
    buf[0x10600] = 'H' ^ 0x99;
    buf[0x10601] = 'Y' ^ 0x99;

    if (size != CHIPS_DAT_SIZE) {
	fprintf(stderr, "File is the wrong size (expected %u, found %lu).\n",
			CHIPS_DAT_SIZE, size);
	exit(1);
    }
    y = 0;
    z = 0;
    for (n = 0 ; n < size ; ++n) {
	y ^= buf[n];
	z += buf[n];
    }
    if (z != CHIPS_DAT_CHKSUM) {
	fprintf(stderr, "File has the wrong checksum (%d instead of %d).\n",
			z, CHIPS_DAT_CHKSUM);
	exit(1);
    }
    if (y != CHIPS_DAT_CHKXOR) {
	fprintf(stderr, "File has the wrong check-xor (%d instead of %d).\n",
			y, CHIPS_DAT_CHKXOR);
	exit(1);
    }
}

int main(int argc, char *argv[])
{
    struct stat	st;
    int		fullfix = 0;
    int		n;

    n = 1;
    if (argc > n && !strcmp(argv[n], "-x")) {
	fullfix = 1;
	++n;
    }
    if (argc > n) {
	file = argv[n];
	++n;
    }
    if (argc > n || !file) {
	fprintf(stderr, "Usage: %s [-x] FILENAME\n", argv[0]);
	exit(1);
    }

    if (stat(file, &st)) {
	perror(file);
	exit(1);
    }
    size = st.st_size;

    if (!(buf = malloc(st.st_size)) || !(fp = fopen(file, "r+b"))
				    || fread(buf, size, 1, fp) != 1) {
	perror(file);
	exit(1);
    }

    buf[0x00003] = 0x01;

    if (fullfix)
	fixchips();

    rewind(fp);
    if (fwrite(buf, size, 1, fp) != 1) {
	perror(file);
	exit(1);
    }

    fclose(fp);
    printf("%s was successfully altered to use the Lynx ruleset\n", file);
    return 0;
}
