#include	<stdlib.h>
#include	<string.h>
#include	"gen.h"
#include	"defs.h"
#include	"err.h"
#include	"fileio.h"
#include	"messages.h"

labelledtext *readlabelledtextfile(char const *filename)
{
    fileinfo file;
    labelledtext lt, *lts;
    char buf[256], *line;
    int labelcount;
    int buflen, linelen, label, eof;

    clearfileinfo(&file);
    if (!fileopen(&file, filename, "r", "unknown error"))
	return NULL;

    labelcount = 0;
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
	} else if (linelen == 0 && sscanf(buf, "[%d]", &label) == 1) {
	    if (lt.linecount) {
		++labelcount;
		xalloc(lts, labelcount * sizeof *lts);
		lts[labelcount - 1] = lt;
	    }
	    lt.label = label;
	    lt.linecount = 0;
	    lt.lines = NULL;
	} else {
	    xalloc(line, linelen + buflen + 1);
	    memcpy(line + linelen, buf, buflen);
	    linelen += buflen;
	    line[linelen++] = ' ';
	}
    }

    fileclose(&file, NULL);

    if (lt.linecount) {
	labelcount += 2;
	xalloc(lts, labelcount * sizeof *lts);
	lts[labelcount - 2] = lt;
    } else {
	++labelcount;
	xalloc(lts, labelcount * sizeof *lts);
    }
    lts[labelcount - 1].label = 0;
    lts[labelcount - 1].linecount = 0;
    lts[labelcount - 1].lines = NULL;

    return lts;
}

char const **getlevelmessage(labelledtext const *lts, int number, int *pcount)
{
    labelledtext const *lt;

    if (!lts)
	return NULL;
    for (lt = lts ; lt->lines ; ++lt)
	if (lt->label == number)
	    break;
    if (!lt->lines)
	return NULL;
    *pcount = lt->linecount;
    return (char const**)lt->lines;
}

int gettextforlevel(labelledtext const *lts, int number, tablespec *table)
{
    char **items = NULL;
    labelledtext const *lt;
    int i;

    if (!lts)
	return FALSE;
    for (lt = lts ; lt->lines ; ++lt)
	if (lt->label == number)
	    break;
    if (!lt->lines)
	return FALSE;

    table->rows = 2 * lt->linecount;
    table->cols = 3;
    table->sep = 8;
    table->collapse = -1;
    table->items = NULL;
    xalloc(table->items, 3 * 2 * lt->linecount * sizeof *items);
    items = table->items;
    for (i = 0 ; i < lt->linecount ; ++i) {
	*items++ = "1- ";
	*items++ = "1! ";
	*items++ = "1+ ";
	*items++ = "1- ";
	*items++ = lt->lines[i];
	*items++ = "1+ ";
    }
    return TRUE;
}

void freelabelledtext(labelledtext *lts)
{
    labelledtext *lt;
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
