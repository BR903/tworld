/* play.c: Top-level game-playing functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<assert.h>
#include	"defs.h"
#include	"err.h"
#include	"state.h"
#include	"oshw.h"
#include	"res.h"
#include	"mslogic.h"
#include	"lxlogic.h"
#include	"random.h"
#include	"solution.h"
#include	"play.h"

/* The functions used to apply the selected ruleset.
 */
static int	      (*initgame)(gamestate*) = NULL;
static int	      (*advancegame)(gamestate*) = NULL;
static int	      (*endgame)(gamestate*) = NULL;

/* The current state of the current game.
 */
static gamestate	state;

/* Configure the game logic, and some of the OS/hardware layer, to the
 * behavior expected for the given ruleset.
 */
static int setrulesetbehavior(int ruleset)
{
    static int	lastruleset = Ruleset_None;

    if (ruleset == lastruleset)
	return TRUE;
    lastruleset = ruleset;

    switch (ruleset) {
      case Ruleset_Lynx:
	initgame = lynx_initgame;
	advancegame = lynx_advancegame;
	endgame = lynx_endgame;
	setkeyboardarrowsrepeat(TRUE);
	settimersecond(1000);
	break;
      case Ruleset_MS:
	initgame = ms_initgame;
	advancegame = ms_advancegame;
	endgame = ms_endgame;
	setkeyboardarrowsrepeat(FALSE);
	settimersecond(1100);
	break;
      default:
	die("Unknown ruleset requested");
	return FALSE;
    }

    if (!loadgameresources(ruleset) || !creategamedisplay())
	die("Unable to proceed due to previous errors.");

    return TRUE;
}

/* Initialize the current state to the starting position of the
 * current level.
 */
int initgamestate(gameseries *series, int level, int replay)
{
    if (!setrulesetbehavior(series->ruleset))
	return FALSE;

    memset(&state, 0, sizeof state);
    state.game = &series->games[level];
    state.ruleset = series->ruleset;
    state.lastmove = NIL;
    state.statusflags |= SF_ONOMATOPOEIA;
    state.soundeffects = 0;
    state.timelimit = state.game->time * TICKS_PER_SECOND;

    if (replay) {
	if (!state.game->savedsolution.count)
	    return FALSE;
	state.replay = 0;
	copymovelist(&state.moves, &state.game->savedsolution);
	state.initrndslidedir = state.game->savedrndslidedir;
	restartprng(&state.mainprng, state.game->savedrndseed);
    } else {
	state.replay = -1;
	initmovelist(&state.moves);
	state.initrndslidedir = NIL;
	resetprng(&state.mainprng);
    }

    return (*initgame)(&state);
}

/* Put the program into game-play mode.
 */
void setgameplaymode(int mode)
{
    switch (mode) {
      case BeginPlay:	settimer(+1);	setkeyboardrepeat(FALSE);	break;
      case SuspendPlay:	settimer(0);	setkeyboardrepeat(TRUE);	break;
      case ResumePlay:	settimer(+1);	setkeyboardrepeat(FALSE);	break;
      case EndPlay:	settimer(-1);	setkeyboardrepeat(TRUE);	break;
    }
}

/* Advance the game one tick. cmd is the current keyboard command
 * supplied by the user. The return value is positive if the game was
 * completed successfully, negative if the game ended unsuccessfully,
 * and zero otherwise.
 */
int doturn(int cmd)
{
    action	act;
    int		n;

    state.soundeffects = 0;
    state.currenttime = gettickcount();
    if (state.replay < 0) {
	if (cmd != CmdPreserve)
	    state.currentinput = cmd;
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

    n = (*advancegame)(&state);

    if (state.replay < 0 && state.lastmove) {
	act.when = state.currenttime;
	act.dir = state.lastmove;
	addtomovelist(&state.moves, act);
	state.lastmove = NIL;
    }

    if (n)
	return n;

    return 0;
}

/* Update the display of the current game state.
 */
int drawscreen(void)
{
    int currtime, besttime;

    if (hassolution(state.game)) {
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

int endgamestate(void)
{
    return (*endgame)(&state);
}

/* Return TRUE if a solution exists for the given level.
 */
int hassolution(gamesetup const *game)
{
    return game->savedsolution.count > 0;
}

/* Compare the most recent solution for the current game with the
 * user's best solution (if any). If this solution beats what's there,
 * replace it. TRUE is returned if the solution was replaced.
 */
int replacesolution(void)
{
    if (state.statusflags & SF_NOSAVING)
	return FALSE;
    if (hassolution(state.game) && !state.game->replacebest
				&& state.currenttime >= state.game->besttime)
	return FALSE;

    state.game->besttime = state.currenttime;
    state.game->replacebest = FALSE;
    state.game->savedrndslidedir = state.initrndslidedir;
    state.game->savedrndseed = getinitialseed(&state.mainprng);
    copymovelist(&state.game->savedsolution, &state.moves);
    return TRUE;
}
