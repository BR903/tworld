/* play.c: Top-level game-playing functions.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_play_h_
#define	_play_h_

#include	"defs.h"

/* The different modes of the program with respect to game-play.
 */
enum { BeginPlay, EndPlay, SuspendPlay, ResumePlay, BeginInput, EndInput };

/* Initialize the current state to the starting position of the
 * given level.
 */
extern int initgamestate(gamesetup *game, int ruleset);

/* Change the current state to run from the recorded solution.
 */
extern int prepareplayback(void);

/* Move the program in and out of game-play mode. This affects
 * the running of the timer and the handling of the keyboard.
 */
extern void setgameplaymode(int mode);

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

/* Free any resources associates with the current game state.
 */
extern int endgamestate(void);

/* Free all persistent resources in the module.
 */
extern void shutdowngamestate(void);

/* Return TRUE if a solution exists for the given level.
 */
extern int hassolution(gamesetup const *game);

/* Replace the user's solution with the just-executed solution if it
 * beats the existing solution for shortest time. FALSE is returned if
 * no solution was replaced.
 */
extern int replacesolution(void);
extern int checksolution(void);

#endif
