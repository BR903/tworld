/* messages.h: Functions for finding and reading the series files.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef _messages_h_
#define _messages_h_

#include "defs.h"
#include "gen.h"

/* Read a file containing an array of labelled texts. Each label is a
 * number, indicating the level after which the message is displayed.
 * (A level number of zero labels introductory text.) The return value
 * is NULL if the file is empty or unreadable. The caller must
 * eventually free the data via the freelabelledtext() function.
 */
extern labelledtext *readlabelledtextfile(char const *filename);

/* Look up the given number in the array of labelled texts, and if a
 * text exists for that number, return it. pcount receives the number
 * of strings making up the returned text. The return value is NULL if
 * no such text is present in the given array.
 */
extern char const **getlevelmessage(labelledtext const *lts, int number,
				    int *pcount);

/* Free all memory associated with the given array of labelled texts.
 */
extern void freelabelledtext(labelledtext *lts);

#endif
