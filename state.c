/* state.c: Game-state housekeeping functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"gen.h"
#include	"cc.h"
#include	"commands.h"
#include	"movelist.h"
#include	"timer.h"
#include	"random.h"
#include	"logic.h"
#include	"userout.h"
#include	"state.h"
#include	"statestr.h"

/* The current state of the current game.
 */
static gamestate	state;

/* Initialize the current state to the starting position of the
 * current level.
 */
int initgamestate(int playback)
{
    if (playback) {
	if (!state.game->savedsolution.count)
	    return FALSE;
	state.replay = 0;
	copymovelist(&state.moves, &state.game->savedsolution);
	setrandomseed(state.game->savedrndseed);
	state.rndslidedir = state.game->savedrndslidedir;
    } else {
	state.replay = -1;
	initmovelist(&state.moves);
	state.rndseed = getrandomseed();
	state.rndslidedir = NIL;
    }

    state.currentinput = NIL;
    state.currenttime = 0;

    initgamelogic(&state);

    return TRUE;
}

/* Set the current puzzle to be game, with the given level number.
 */
void selectgame(gamesetup *game)
{
    memset(&state, 0, sizeof state);
    state.game = game;
    settimer(-1);
    settimer(0);
}

int recordmove(int dir)
{
    action	act;

    if (state.replay < 0) {
	act.when = state.currenttime;
	act.dir = dir;
	addtomovelist(&state.moves, act);
    }
    return TRUE;
}

int doturn(int m)
{
    int	n;

    state.soundeffect = NULL;
    state.currenttime = gettickcount();
    if (state.replay < 0) {
	/*if (m != NIL)*/
	if (m != CmdPreserve)
	    state.currentinput = m;
    } else {
	if (state.replay < state.moves.count) {
	    if (state.currenttime > state.moves.list[state.replay].when)
		die("Replay: Got ahead of saved solution: %d > %d!",
		    state.currenttime, state.moves.list[state.replay].when);
	    if (state.currenttime == state.moves.list[state.replay].when) {
		state.currentinput = state.moves.list[state.replay].dir;
		++state.replay;
	    }
	}
    }
    n = advancegame(&state);
    if (n)
	return n;
    if (state.game->time)
	if (state.currenttime / TICKS_PER_SECOND >= state.game->time)
	    return -1;
    return 0;
}

/* Display the current game state to the user.
 */
int drawscreen(void)
{
    int currtime, besttime;

    if (state.game->besttime) {
	besttime = (state.game->time ? state.game->time : 999)
				- state.game->besttime / TICKS_PER_SECOND;
	if (besttime == 0)
	     besttime = -1;
    } else
	besttime = 0;

    if (state.game->time)
	currtime = state.game->time - state.currenttime / TICKS_PER_SECOND;
    else
	currtime = -1;

    return displaygame(&state, currtime, besttime);
}

/* Compare the most recent solution for the current game with the
 * user's best solution (if any). If this solution beats what's there,
 * replace it. TRUE is returned if the solution was replaced.
 */
int replacesolution(void)
{
    if (state.game->besttime && state.currenttime >= state.game->besttime)
	return FALSE;

    state.game->besttime = state.currenttime;
    state.game->savedrndseed = state.rndseed;
    state.game->savedrndslidedir = state.rndslidedir;
    copymovelist(&state.game->savedsolution, &state.moves);
    return TRUE;
}
