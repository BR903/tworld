/* statestr.h: The structure defining the game in progress.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_statestr_h_
#define	_statestr_h_

#include	"gen.h"
#include	"cc.h"
#include	"movelist.h"
#include	"fileread.h"

/* The collection of data corresponding to the game's state.
 */
struct gamestate {
    gamesetup  *game;			/* the level specification */
    short	replay;			/* TRUE if solution is playing back */
    short	completed;		/* TRUE if level has been solved */
    short	chipsneeded;		/* the no. of chips left to collect */
    short	currenttime;		/* the current no. of ticks */
    int		currentinput;		/* the current keystroke */
    int		displayflags;		/* flags that alter the game display */
    short	keys[4];		/* the player's collected keys */
    short	boots[4];		/* the player's collected boots */
    char const *soundeffect;		/* the latest sound effect */
    actlist	moves;			/* the player's list of moves */
    unsigned long rndseed;		/* the starting random-number seed */
    char	rndslidedir;		/* the initial random-slide dir */
    mapcell	map[CXGRID * CYGRID];	/* the game's map */
    creature	creatures[CXGRID * CYGRID];	/* the list of creatures */
};

#endif
