#ifndef _messages_h_
#define _messages_h_

#include "defs.h"
#include "gen.h"

/* Read a file containing an array of labelled texts. Each label is a
 * number, indicating the level after which the message is displayed.
 * (A level number of zero labels introductory text.) The return value
 * is NULL if the file is empty or unreadable.
 */
extern labelledtext *readlabelledtextfile(char const *filename);

/* Look up the given number in the array of labelled texts, and if a
 * text exists for that number, fill out the given table spec to
 * display that text. The return value is false if no such text is
 * present in the given array. Otherwise, the caller is responsible
 * for freeing the table's item array afterwards.
 */
extern int gettextforlevel(labelledtext const *lts, int number,
			   tablespec *table);

/* Free all memory associated with the given array of labelled texts.
 */
extern void freelabelledtext(labelledtext *lts);

#endif
