/* timer.c: Game timing functions
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<signal.h>
#include	<unistd.h>
#include	<sys/time.h>
#include	"gen.h"
#include	"timer.h"

static struct itimerval		timer;

static volatile sig_atomic_t	utick = 0;

void settimer(int action)
{
    if (action > 0)
	timer.it_value = timer.it_interval;
    else {
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	if (action < 0)
	    utick = 0;
    }
    setitimer(ITIMER_REAL, &timer, NULL);
}

int gettickcount(void)
{
    return (int)utick;
}

void waitfortick(void)
{
    pause();
}

/*
 *
 */

static void sigalrm(int sig)
{
    ++utick;
    (void)sig;
}

int timerinitialize(int sludgefactor)
{
    struct sigaction	act;

    act.sa_handler = sigalrm;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGALRM, &act, NULL);

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 1000000 / TICKS_PER_SECOND;
    timer.it_interval.tv_usec = (1100000 * sludgefactor) / TICKS_PER_SECOND;

    return TRUE;
}
