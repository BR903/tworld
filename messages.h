/* messages.h: Functions for finding and reading the series files.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef _messages_h_
#define _messages_h_

#include "defs.h"
#include "gen.h"

/* Read a file containing an array of tagged message texts. Each tag
 * is a number, indicating the level for which the message is
 * displayed. The return value is NULL if the file is empty or
 * unreadable. The caller must eventually free the data via the
 * freetaggedtext() function.
 *
 * A message text is a sequence of paragraphs. The tag must appear
 * before the text in square brackets, on a line by itself. If the tag
 * is preceded by a minus sign, the message is marked as prologue
 * text; if it is preceded by a plus sign, the message is epilogue
 * text. (If no sign is present, epilogue text is assumed.) Within the
 * message text, paragraphs are separated by blank lines, i.e. two or
 * more line break characters in a row. (Line breaks at the beginning
 * or end of a text are dropped.) Single line breaks within a
 * paragraph are replaced with spaces. A literal line break can be
 * indicated by ending a line with two spaces before the line break.
 */
extern taggedtext *readmessagesfile(char const *filename);

/* Look up the given number in the array of tagged texts, and if a
 * text exists for that number, return it. pcount receives the number
 * of strings making up the returned text. The return value is NULL if
 * no such text is present in the given array.
 */
extern char const **gettaggedmessage(taggedtext const *lts, int number,
				     int *pcount);

/* Free all memory associated with the given array of tagged texts.
 */
extern void freetaggedtext(taggedtext *lts);

#endif
