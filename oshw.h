/* oshw.h: Platform-specific functions that talk with the OS/hardware.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_oshw_h_
#define	_oshw_h_

#include	<stdarg.h>

/* Initialize the OS/hardware interface. This function must be called
 * before any others in the oshw library.
 */
extern int oshwinitialize(void);

/*
 * Timer functions.
 */

/* Control the timer depending on the value of action. A negative
 * value turns off the timer if it is running and resets the counter
 * to zero. A zero value turns off the timer but does not reset the
 * counter. A positive value starts (or resumes) the timer.
 */
extern void settimer(int action);

/* Return the number of ticks since the timer was last reset.
 */
extern int gettickcount(void);

/* Put the program to sleep until the next timer tick.
 */
extern void waitfortick(void);

/*
 * Keyboard input functions.
 */

/* Alter the keyboard behavior. The Lynx ruleset causes the direction
 * keys to constantly repeat without lingering. The MS ruleset has the
 * direction keys repeat after a small initial delay, and linger for
 * half that time. All other keys in both behaviors are non-repeating
 * if polling is TRUE. If polling is FALSE, all keys exhibit the
 * keyboard's original repeat rate.
 */
extern int setkeyboardbehavior(int ruleset, int polling);

/* Return the latest/current keystroke. If wait is TRUE and no
 * keystrokes are pending, the function blocks until a keystroke
 * arrives.
 */
extern int input(int wait);

/* Wait for a key to be pressed (any key, not just one recognized by
 * the program). FALSE is returned if the key pressed is one of the
 * quit keys.
 */
extern int anykey(void);

/*
 * Video output functions.
 */

/* Display the current game state. timeleft and besttime provide the
 * current time on the clock and the best time recorded for the level,
 * measured in seconds.
 */
extern int displaygame(void const *state, int timeleft, int besttime);

/* Display a short message appropriate to the end of a level's game
 * play. completed is TRUE if the level was successfully completed.
 */
extern int displayendmessage(int completed);

/* Display a scrollable list of strings. title provides the title of
 * the list. header provides a single line of text at the top of the
 * list that doesn't scroll. items is an array of itemcount strings.
 * index points to the index of the item to be initially selected;
 * upon return, the value will be changed to the item that was finally
 * selected. inputcallback points to a function that is called after
 * displaying the list. The function is passed a pointer to an
 * integer; this value should be filled in with an integer indicating
 * how the selection is to be moved. If the return value from the
 * function is TRUE, the display is updated and the function is called
 * again. If the return value from the function is FALSE,
 * displaylist() ends, returning the value that was stored via the
 * pointer argument.
 */
extern int displaylist(char const *title, char const *header,
		       char const **items, int itemcount, int *index,
		       int (*inputcallback)(int*));

/*
 * Miscellaneous functions.
 */

/* Ring the bell.
 */
extern void ding(void);

/* Set the program's "subtitle" (displayed as part the window
 * dressing, if any).
 */
extern void setsubtitle(char const *subtitle);

/* Values used for the first argument of the following function.
 */
enum { NOTIFY_DIE, NOTIFY_ERR, NOTIFY_LOG };

/* Display a message to the user. cfile and lineno optionally identify
 * the source code where this function was called from. prefix is an
 * optional string that is displayed ahead of the main text. fmt and
 * args define the formatted text of the message proper. action is one
 * of the enum values given above. NOTIFY_LOG causes the message to be
 * displayed in a way that does not interfere with the rest of the
 * program. NOTIFY_DIE gives particular emphasis to the message.
 */
extern void usermessage(int action, char const *prefix,
			char const *cfile, unsigned long lineno,
			char const *fmt, va_list args);

/* A structure used to define text with illustrations.
 */
typedef	struct objhelptext {
    int		isfloor;	/* TRUE if the images are floor tiles */
    int		item1;		/* first illustration */
    int		item2;		/* second illustration */
    char const *desc;		/* text */
} objhelptext;

/* Values indicating the type of data passed to the following function.
 */
enum { HELP_TABTEXT, HELP_OBJECTS };

/* Displays a screenful of (hopefully) helpful information. title
 * provides the title of the display. text points to an array of
 * textcount objects. If type is HELP_TABTEXT, then the array contains
 * strings, each with one embedded tab character. The text before and
 * after the tabs is arranged in columns, with the text
 * left-justified. (The text after the tab is permitted to occupy more
 * than one line on the screen.) If type is HELP_OBJECTS, then the
 * array contains objhelptext structures, and the text is displayed to
 * the right of the indicated tiles.
 */
extern int displayhelp(int type, char const *title,
		       void const *text, int textcount);

#endif
