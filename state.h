/* state.h: Game-state housekeeping functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_state_h_
#define	_state_h_

#include	"gen.h"
#include	"cc.h"
#include	"movelist.h"
#include	"fileread.h"

/* The collection of data corresponding to the game's state.
 */
typedef	struct gamestate {
    gamesetup  *game;			/* the level specification */
    short	replay;			/* TRUE if solution is playing back */
    short	completed;		/* TRUE if level has been solved */
    short	currenttime;		/* the current no. of ticks */
    int		currentinput;		/* the current keystroke */
    short	currpos;		/* the player's current position */
    short	currdir;		/* the player's current direction */
    short	chipsneeded;		/* the no. of chips left to collect */
    int		displayflags;		/* flags that alter the game display */
    short	keys[4];		/* the player's collected keys */
    short	boots[4];		/* the player's collected boots */
    char const *soundeffect;		/* the latest sound effect */
    actlist	moves;			/* the player's list of moves */
    unsigned	rndseed;		/* the starting random-number seed */
    mapcell	map[CXGRID * CYGRID];	/* the game's map */
    creature	creatures[CXGRID * CYGRID];	/* the list of creatures */
} gamestate;

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
 * no solution was replaced. If saveinc is TRUE, then the user's
 * "solution" is not actually complete, in which case it will only be
 * saved if no complete solution is currently saved.
 */
extern int replacesolution(int saveinc);

#endif
