/* state.c: Game-state housekeeping functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"gen.h"
#include	"cc.h"
#include	"movelist.h"
#include	"timer.h"
#include	"random.h"
#include	"logic.h"
#include	"userio.h"
#include	"state.h"

/* The current state of the current game.
 */
static gamestate	state;

/* Initialize the current state to the starting position of the
 * current level.
 */
int initgamestate(int playback)
{
    translatemapdata(&state);

    if (playback) {
	if (!state.game->savedsolution.count)
	    return FALSE;
	state.replay = 0;
	copymovelist(&state.moves, &state.game->savedsolution);
	setrandomseed(state.game->savedrndseed);
    } else {
	state.replay = -1;
	initmovelist(&state.moves);
	state.rndseed = getrandomseed();
    }

    state.currentinput = NIL;
    state.currenttime = 0;

    return TRUE;
}

/* Set the current puzzle to be game, with the given level number.
 */
void selectgame(gamesetup *game)
{
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
	if (m != NIL)
	    state.currentinput = m;
    } else {
	if (state.replay < state.moves.count) {
	    if (state.currenttime == state.moves.list[state.replay].when) {
		state.currentinput = state.moves.list[state.replay].dir;
		++state.replay;
	    }
	}
    }
    n = advancegame(&state);
    if (n)
	return n;
    if (state.game->time && state.currenttime / 10 >= state.game->time)
	return -5;
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

    return displaygame(state.map, state.currpos, state.currdir,
		       state.chipsneeded, currtime, (state.currenttime > 0),
		       state.game->number, state.game->name,
		       state.game->passwd, besttime, state.game->hinttext,
		       state.keys, state.boots, state.soundeffect,
		       state.displayflags);
}

/* Compare the solution currently sitting in the undo list with the
 * user's best solutions (if any). If this solution beats what's
 * there, replace them. If this solution has the same number of moves
 * as the least-moves solution, but fewer steps, then the replacement
 * will be done, and likewise for the least-steps solution. Note that
 * the undo list contains the moves in backwards order, so the list
 * needs to be reversed when it is copied. TRUE is returned if the
 * solution was replaced.
 */
int replacesolution(int saveinc)
{
    if (state.game->besttime) {
	if (saveinc)
	    return FALSE;
	if (state.currenttime >= state.game->besttime)
	    return FALSE;
	state.game->besttime = state.currenttime;
    } else {
	if (!saveinc)
	    state.game->besttime = state.currenttime;
    }

    state.game->savedrndseed = state.rndseed;
    copymovelist(&state.game->savedsolution, &state.moves);

    return TRUE;
}
