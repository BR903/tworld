/* timer.h: Game timing functions
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_timer_h_
#define	_timer_h_

#define	TICKS_PER_SECOND	10

extern int timerinitialize(void);
extern void settimer(int action);
extern int gettickcount(void);
extern void waitfortick(void);

#endif
