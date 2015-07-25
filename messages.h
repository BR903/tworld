/* messages.h: Functions for finding and reading the series files.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef _messages_h_
#define _messages_h_

#include "defs.h"
#include "gen.h"

/* Read a file containing an array of tagged texts. Each tag is a
 * number, indicating the level for which the message is displayed. (A
 * negative number tags prolog text, a positive number tags epilog
 * text.) The return value is NULL if the file is empty or unreadable.
 * The caller must eventually free the data via the freetaggedtext()
 * function.
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
