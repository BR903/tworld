/* play.c: Top-level game-playing functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_play_h_
#define	_play_h_

#include	"defs.h"

/* Initialize the current state to the starting position of the
 * specified level.
 */
extern int initgamestate(gameseries *series, int level, int replay);

/* Handle one tick of the game. cmd is the current keyboard command
 * supplied by the user, or CmdPreserve if any pending command is to
 * be retained. The return value is positive if the game was completed
 * successfully, negative if the game ended unsuccessfully, and zero
 * otherwise.
 */
extern int doturn(int cmd);

/* Display the current game state to the user.
 */
extern int drawscreen(void);

/* Set the keyboard behavior. A TRUE value for active indicates that the
 * keyboard should be set for game play.
 */
extern int activatekeyboard(int active);

/* Return TRUE if a solution exists for the given level.
 */
extern int hassolution(gamesetup const *game);

/* Replace the user's solution with the just-executed solution if it
 * beats the existing solution for shortest time. FALSE is returned if
 * no solution was replaced.
 */
extern int replacesolution(void);

#endif
