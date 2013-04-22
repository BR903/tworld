#include <stdio.h>
#include <stdlib.h>

static unsigned long remainders[256];
static char const *filename;

/* Build up the table of remainders for each byte value.
 */
static void inittable(void)
{
    unsigned long	accum, i, j;

    for (i = 0 ; i < 256 ; ++i) {
	accum = i << 24;
	for (j = 0 ; j < 8 ; ++j) {
	    if (accum & 0x80000000UL)
		accum = (accum << 1) ^ 0x04C11DB7UL;
	    else
		accum = (accum << 1);
	}
	remainders[i] = accum & 0xFFFFFFFFUL;
    }
}

/* Compute the CRC-32 for an arbitrary block of data.
 */
static unsigned long hashvalue(unsigned char const *data, int size)
{
    unsigned long	accum;
    int			i, j;

    for (j = 0, accum = 0xFFFFFFFFUL ; j < size ; ++j) {
	i = ((accum >> 24) ^ data[j]) & 0x000000FF;
	accum = ((accum << 8) & 0xFFFFFFFFUL) ^ remainders[i];
    }
    return accum ^ 0xFFFFFFFFUL;
}

/* Read a 16-bit little-endian integer from the file.
 */
static int read16(FILE *fp)
{
    int		a, b;

    a = fgetc(fp);
    if (a != EOF) {
	b = fgetc(fp);
	if (b != EOF)
	    return (a & 0xFF) | ((b & 0xFF) << 8);
    }
    perror(filename);
    exit(EXIT_FAILURE);
}

/* Read the filenames from the command line. For each file, break it
 * apart into individual levels. For each level, calculate and display
 * its hash value.
 */
int main(int argc, char *argv[])
{
    FILE	       *fp;
    unsigned char      *buf = 0;
    int			size, i, n;

    inittable();
    for (i = 1 ; i < argc ; ++i) {
	filename = argv[i];
	if (!(fp = fopen(filename, "rb"))) {
	    perror(filename);
	    exit(EXIT_FAILURE);
	}
	n = read16(fp);
	if (n != 0xAAAC) {
	    fprintf(stderr, "%s: not a CC data file\n", filename);
	    exit(EXIT_FAILURE);
	}
	read16(fp);
	n = read16(fp);
	printf("\n[%s]\n", filename);
	while (n--) {
	    size = read16(fp);
	    if (!(buf = realloc(buf, size)) || !fread(buf, size, 1, fp)) {
		perror(filename);
		exit(EXIT_FAILURE);
	    }
	    printf("%03d: %04X %08lX\n", buf[0] | (buf[1] << 8),
					 size, hashvalue(buf, size));
	}
	fclose(fp);
    }
    return EXIT_SUCCESS;
}
