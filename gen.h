/* gen.h: General definitions belonging to no single module.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_gen_h_
#define	_gen_h_

/* The standard Boolean values.
 */
#ifndef	TRUE
#define	TRUE	1
#endif
#ifndef	FALSE
#define	FALSE	0
#endif

/* Definition of the contents and layout of a table.
 *
 * The strings making up the contents of a table are each prefixed
 * with two characters that indicate the formatting of their cell. The
 * first character is a digit, usually "1", indicating the number of
 * columns that cell occupies. The second character indicates the
 * placement of the string in that cell: "-" to align to the left of
 * the cell, "+" to align to the right, "." to center the text, and
 * "!" to permit the cell to occupy multiple lines, with word
 * wrapping. (Only one cell per row can use word wrapping.)
 */
typedef	struct tablespec {
    short	rows;		/* number of rows */
    short	cols;		/* number of columns */
    short	sep;		/* amount of space between columns */
    short	collapse;	/* the column to squeeze if necessary */
    char      **items;		/* the table's contents */
} tablespec;

/* The size of each level's grid.
 */
#define	CXGRID	32
#define	CYGRID	32

/* The four directions, plus NIL.
 */
#define	NIL	0
#define	NORTH	1
#define	WEST	2
#define	SOUTH	4
#define	EAST	8

/* Translating directions to and from a two-bit representation. (Note
 * that NIL will map to the same value as NORTH.)
 */
#define	diridx(dir)	((0x30210 >> ((dir) * 2)) & 3)
#define	idxdir(idx)	(1 << ((idx) & 3))

/* The frequency of the gameplay timer.
 */
#define	TICKS_PER_SECOND	20

/* Memory leak detection functions of desperation.
 */
#ifdef DESPERATE_LEAK_DETECTOR
extern void *debug_malloc(char const *file, long line, size_t n);
extern void *debug_calloc(char const *file, long line, size_t m, size_t n);
extern void *debug_realloc(char const *file, long line, void *p, size_t n);
extern void debug_free(char const *file, long line, void *p);
#define	malloc(n)	debug_malloc(__FILE__, __LINE__, n)
#define	calloc(m, n)	debug_calloc(__FILE__, __LINE__, m, n)
#define	realloc(p, n)	debug_realloc(__FILE__, __LINE__, p, n)
#define	free(p)		debug_free(__FILE__, __LINE__, p)
#endif

#endif
