/* random.c: The game's random-number generator
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

/* 
 * This module is not here because I don't trust the C library's
 * random-number generator. (In fact, this module uses the linear
 * congruential generator, which is hardly impressive. But this isn't
 * strong cryptography; it's a game.) It is here simply because it is
 * necessary for the game to always use the SAME generator. In order
 * for playback of solutions to work correctly, the game must use the
 * same sequence of random numbers (for blobs, walkers, and random
 * slide floors) as when it was recorded. Therefore the random-number
 * seed that was originally used is stored in the saved-game file for
 * each level. But this would still fail if the playback occurred on a
 * version compiled with a different generator. Thus, this module.
 */

#include	<assert.h>
#include	<time.h>
#include	"gen.h"
#include	"random.h"

/* The random number.
 */
long	randomval = -1;

/* The linear congruential random-number generator needs no
 * introduction.
 */
static void nextrandom(void)
{
    assert(randomval >= 0);
    randomval = ((randomval * 1103515245UL) + 12345UL) & 0x7FFFFFFF;
}

/* We start off a fresh series by taking the current time. A few
 * numbers are generated and discarded to work out the biases of the
 * seed value.
 */
int randominitialize(void)
{
    randomval = time(NULL);
    randomval = ((randomval * 1103515245UL) + 12345UL) & 0x7FFFFFFF;
    randomval = ((randomval * 1103515245UL) + 12345UL) & 0x7FFFFFFF;
    randomval = ((randomval * 1103515245UL) + 12345UL) & 0x7FFFFFFF;
    randomval = ((randomval * 1103515245UL) + 12345UL) & 0x7FFFFFFF;
    return TRUE;
}

/* Use the top two bits to get a random number between 0 and 3.
 */
int random4(void)
{
    nextrandom();
    return randomval >> 29;
}

/* Randomly select an element from a list of three values.
 */
int randomof3(int a, int b, int c)
{
    int	n;

    nextrandom();
    n = (int)((3.0 * (randomval & 0x3FFFFFFF)) / (double)0x40000000);
    return n < 2 ? n < 1 ? a : b : c;
}

/* Randomly permute a list of three values. Two random numbers are
 * used, with the ranges [0,1] and [0,1,2].
 */
void randomp3(int *array)
{
    int	n, t;

    nextrandom();
    n = randomval >> 30;
    t = array[n];  array[n] = array[1];  array[1] = t;
    n = (int)((3.0 * (randomval & 0x3FFFFFFF)) / (double)0x40000000);
    t = array[n];  array[n] = array[2];  array[2] = t;
}

/* Randomly permute a list of four values. Three random numbers are
 * used, with the ranges [0,1], [0,1,2], and [0,1,2,3].
 */
void randomp4(int *array)
{
    int	n, t;

    nextrandom();
    n = randomval >> 30;
    t = array[n];  array[n] = array[1];  array[1] = t;
    n = (int)((3.0 * (randomval & 0x0FFFFFFF)) / (double)0x10000000);
    t = array[n];  array[n] = array[2];  array[2] = t;
    n = (randomval >> 28) & 3;
    t = array[n];  array[n] = array[3];  array[3] = t;
}
