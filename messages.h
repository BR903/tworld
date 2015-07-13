#ifndef _messages_h_
#define _messages_h_

/* Read a file containing an array of labelled texts. Each label is a
 * number, indicating the level after which the message is displayed.
 * (A level number of zero labels introductory text.) The return value
 * is NULL if the file is empty or unreadable.
 */
extern labelledtext *readlabelledtextfile(char const *filename);

extern int gettextforlevel(labelledtext const *lts, int number,
			   tablespec *table);

/* Free all memory associated with an array of labelled texts.
 */
extern void freelabelledtext(labelledtext *lts);

#endif
