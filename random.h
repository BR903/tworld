/* random.h: The game's random-number generator.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_random_h_
#define	_random_h_

#include	"defs.h"

/* Create a fresh PRNG.
 */
extern prng createprng(void);

/* Mark an existing PRNG as beginning a new sequence.
 */
extern void resetprng(prng *gen);

/* Restart an existing PRNG upon a predetermined sequence.
 */
extern void restartprng(prng *gen, unsigned long initial);

/* Retrieve the original seed value.
 */
#define	getinitialseed(gen)	((gen)->initial)

extern int random4(prng *gen);
extern int randomof3(prng *gen, int a, int b, int c);
extern void randomp3(prng *gen, int *array);
extern void randomp4(prng *gen, int *array);

#if 0

/* The generator's seed value, and macros for accessing same.
 */
extern long randomval;

#define	getrandomseed()		(randomval)
#define	setrandomseed(seed)	(randomval = (seed) & 0x7FFFFFFFL)

/* Either this function or setrandomseed() must be called before
 * any random numbers can be generated.
 */
extern int randominitialize(void);

/* Return a random number between 0 and 3 inclusive.
 */
extern int random4(void);

/* Randomly select an element from a list of three values.
 */
extern int randomof3(int a, int b, int c);

/* Randomly permute an array of three elements.
 */
extern void randomp3(int *array);

/* Randomly permute an array of four elements.
 */
extern void randomp4(int *array);

#endif
#endif
