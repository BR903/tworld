/* sdltimer.c: Game timing functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"SDL.h"
#include	"sdlgen.h"

/* By default, a second of game time lasts for 1000 milliseconds of
 * real time.
 */
static int	mspertick = 1000 / TICKS_PER_SECOND;

/* The tick counter.
 */
static int	utick = 0;

/* The time of the next tick.
 */
static int	nexttickat = 0;

/* Set the length (in real time) of a second of game time. A value of
 * zero selects the default length of one second.
 */
void settimersecond(int ms)
{
    mspertick = (ms ? ms : 1000) / TICKS_PER_SECOND;
}

/* Control the timer depending on the value of action. A negative
 * value turns off the timer if it is running and resets the counter
 * to zero. A zero value turns off the timer but does not reset the
 * counter. A positive value starts (or resumes) the timer.
 */
void settimer(int action)
{
    if (action < 0) {
	nexttickat = 0;
	utick = 0;
    } else if (action > 0) {
	if (nexttickat < 0)
	    nexttickat = SDL_GetTicks() - nexttickat;
	else
	    nexttickat = SDL_GetTicks() + mspertick;
    } else {
	if (nexttickat > 0)
	    nexttickat = SDL_GetTicks() - nexttickat;
    }
}

/* Return the number of ticks since the timer was last reset.
 */
int gettickcount(void)
{
    return (int)utick;
}

/* Put the program to sleep until the next timer tick.
 */
void waitfortick(void)
{
    int	ms;

    ms = nexttickat - SDL_GetTicks();
    while (ms < 0)
	ms += mspertick;
    if (ms > 0)
	SDL_Delay(ms);
    ++utick;
    nexttickat += mspertick;
}

static void shutdown(void)
{
    settimer(-1);
}

/* Initialize and reset the timer.
 */
int _sdltimerinitialize(void)
{
    atexit(shutdown);
    settimer(-1);
    return TRUE;
}
