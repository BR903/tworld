/* state.h: Game-state housekeeping functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_state_h_
#define	_state_h_

#include	"fileread.h"

/* Set the current level to be game. After calling this function,
 * initgamestate() must be called before using any other functions in
 * this module.
 */
extern void selectgame(gamesetup *game);

/* Initialize the current state to the starting position of the
 * current level.
 */
extern int initgamestate(int replay);

/* Handle one tick of the game.
 */
extern int doturn(int ch);
extern int recordmove(int dir);

/* Return TRUE if the playing of the current level has ended.
 */
extern int checkfinished(void);

/* Display the current game state to the user.
 */
extern int drawscreen(void);

/* Replace the user's solution with the just-executed solution if it
 * beats the existing solution for shortest time. FALSE is returned if
 * no solution was replaced.
 */
extern int replacesolution(void);

#endif
