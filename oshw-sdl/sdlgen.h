/* sdlgen.h: The internal shared functions of the SDL layer.
 * 
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_sdlgen_h_
#define	_sdlgen_h_

#include	"../defs.h"
#include	"../oshw.h"

/* From sdloshw.c: Process all pending events. If no events are
 * currently pending and wait is TRUE, the function blocks until an
 * event occurs.
 */
extern void _sdleventupdate(int wait);

/* From sdltimer.c: Initialize the timer subsystem.
 */
extern int _sdltimerinitialize(void);

/* From sdlin.c: Initialize the keyboard subsystem.
 */
extern int _sdlinputinitialize(void);

/* From sdlin.c: A function that is called every time a keyboard key
 * is pressed or released. scancode is an SDL key symbol. down is TRUE
 * if the key was pressed or FALSE if it was released.
 */
extern void _sdlkeyeventcallback(int scancode, int down);

/* From sdlout.c: Initialize the graphic output subsystem.
 */
extern int _sdloutputinitialize(void);

#endif
