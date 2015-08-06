/* messages.c: Functions for finding and reading the series files.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	"gen.h"
#include	"defs.h"
#include	"err.h"
#include	"fileio.h"
#include	"messages.h"

/* Parse a file into an array of messages. Each message consists of a
 * numerical tag and a sequence of paragraphs, the latter being stored
 * as an array of strings, one paragraph per string. Since this
 * function is only run when a level set is being loaded into memory,
 * the code doesn't try to allocate memory efficiently, but just
 * reallocates each memory buffer as it grows.
 */
taggedtext *readmessagesfile(char const *filename)
{
    fileinfo file;
    taggedtext lt, *lts;
    char buf[256], *line;
    int tagcount;
    int buflen, linelen, tag, eof;

    clearfileinfo(&file);
    if (!fileopen(&file, filename, "r", "unknown error"))
	return NULL;

    tagcount = 0;
    lts = NULL;
    lt.linecount = 0;
    lt.lines = NULL;
    line = NULL;
    linelen = 0;
    eof = FALSE;
    while (!eof) {
	buflen = sizeof buf - 1;
	if (!filegetline(&file, buf, &buflen, NULL)) {
	    eof = TRUE;
	    *buf = '\0';
	    buflen = 0;
	}
	if (*buf == '#' && linelen == 0)
	    continue;
	if (buflen == 0) {
	    if (linelen) {
		line[linelen - 1] = '\0';
		++lt.linecount;
		xalloc(lt.lines, lt.linecount * sizeof *lt.lines);
		lt.lines[lt.linecount - 1] = line;
		line = NULL;
		linelen = 0;
	    }
	} else if (linelen == 0 && sscanf(buf, "[%d]", &tag) == 1) {
	    if (tag == 0)
		tag = -1;
	    if (lt.linecount) {
		++tagcount;
		xalloc(lts, tagcount * sizeof *lts);
		lts[tagcount - 1] = lt;
	    }
	    lt.tag = tag;
	    lt.linecount = 0;
	    lt.lines = NULL;
	} else {
	    xalloc(line, linelen + buflen + 1);
	    memcpy(line + linelen, buf, buflen);
	    linelen += buflen;
	    if (buflen > 2 && buf[buflen - 2] == ' '
			   && buf[buflen - 1] == ' ') {
		--linelen;
		line[linelen - 1] = '\n';
	    } else {
		line[linelen++] = ' ';
	    }
	}
    }

    fileclose(&file, NULL);

    if (lt.linecount) {
	tagcount += 2;
	xalloc(lts, tagcount * sizeof *lts);
	lts[tagcount - 2] = lt;
    } else {
	++tagcount;
	xalloc(lts, tagcount * sizeof *lts);
    }
    lts[tagcount - 1].tag = 0;
    lts[tagcount - 1].linecount = 0;
    lts[tagcount - 1].lines = NULL;

    return lts;
}

/* Search the given array of tagged texts for a specific tag.
 */
char const **gettaggedmessage(taggedtext const *lts, int number, int *pcount)
{
    taggedtext const *lt;

    if (!lts)
	return NULL;
    for (lt = lts ; lt->lines ; ++lt)
	if (lt->tag == number)
	    break;
    if (!lt->lines)
	return NULL;
    *pcount = lt->linecount;
    return (char const**)lt->lines;
}

/* Free all memory associated with a tagged text array.
 */
void freetaggedtext(taggedtext *lts)
{
    taggedtext *lt;
    int i;

    if (!lts)
	return;
    for (lt = lts ; lt->lines ; ++lt) {
	for (i = 0 ; i < lt->linecount ; ++i)
	    free(lt->lines[i]);
	free(lt->lines);
    }
    free(lts);
}
