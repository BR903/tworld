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
#include	"logic.h"
#include	"random.h"
#include	"solution.h"
#include	"play.h"

/* The current state of the current game.
 */
static gamestate	state;

/* The current logic module.
 */
static gamelogic       *logic = NULL;

/* Configure the game logic, and some of the OS/hardware layer, to the
 * behavior expected for the given ruleset.
 */
static int setrulesetbehavior(int ruleset)
{
    if (logic) {
	if (ruleset == logic->ruleset)
	    return TRUE;
	(*logic->shutdown)(logic);
	logic = NULL;
    }
    if (ruleset == Ruleset_None)
	return TRUE;

    switch (ruleset) {
      case Ruleset_Lynx:
	logic = lynxlogicstartup();
	if (!logic)
	    return FALSE;
	setkeyboardarrowsrepeat(TRUE);
	settimersecond(1000);
	break;
      case Ruleset_MS:
	logic = mslogicstartup();
	if (!logic)
	    return FALSE;
	setkeyboardarrowsrepeat(FALSE);
	settimersecond(1100);
	break;
      default:
	die("Unknown ruleset requested");
	return FALSE;
    }

    if (!loadgameresources(ruleset) || !creategamedisplay())
	die("Unable to proceed due to previous errors.");

    logic->state = &state;
    return TRUE;
}

/* Initialize the current state to the starting position of the
 * given level.
 */
int initgamestate(gamesetup *game, int ruleset)
{
    if (!setrulesetbehavior(ruleset))
	return FALSE;

    memset(state.map, 0, sizeof state.map);
    memset(state.keys, 0, sizeof state.keys);
    memset(state.boots, 0, sizeof state.boots);
    state.game = game;
    state.ruleset = ruleset;
    state.replay = -1;
    state.currenttime = -1;
    state.currentinput = NIL;
    state.lastmove = NIL;
    state.initrndslidedir = NIL;
    state.statusflags = 0;
    state.soundeffects = 0;
    state.timelimit = game->time * TICKS_PER_SECOND;
    initmovelist(&state.moves);
    resetprng(&state.mainprng);

    return (*logic->initgame)(logic);
}

/* Change the current state to run from the recorded solution.
 */
int prepareplayback(void)
{
    if (!state.game->savedsolution.count)
	return FALSE;

    state.replay = 0;
    copymovelist(&state.moves, &state.game->savedsolution);
    state.initrndslidedir = state.game->savedrndslidedir;
    restartprng(&state.mainprng, state.game->savedrndseed);
    return TRUE;
}

/* Put the program into a game-play mode.
 */
void setgameplaymode(int mode)
{
    switch (mode) {
      case BeginPlay:	setkeyboardrepeat(FALSE);	settimer(+1);	break;
      case SuspendPlay:	setkeyboardrepeat(TRUE);	settimer(0);	break;
      case ResumePlay:	setkeyboardrepeat(FALSE);	settimer(+1);	break;
      case EndPlay:	setkeyboardrepeat(TRUE);	settimer(-1);	break;
      case BeginInput:	setkeyboardinputmode(TRUE);			break;
      case EndInput:	setkeyboardinputmode(FALSE);			break;
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

    state.soundeffects &= ~((1 << SND_ONESHOT_COUNT) - 1);
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

    n = (*logic->advancegame)(logic);

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
    int timeleft, besttime;

    if (hassolution(state.game)) {
	besttime = (state.game->time ? state.game->time : 999)
				- state.game->besttime / TICKS_PER_SECOND;
	if (besttime == 0)
	     besttime = -1;
    } else
	besttime = 0;

    if (state.game->time)
	timeleft = state.game->time - state.currenttime / TICKS_PER_SECOND;
    else
	timeleft = -1;

    playsoundeffects(state.soundeffects);
    return displaygame(&state, timeleft, besttime);
}

int endgamestate(void)
{
    clearsoundeffects();
    return (*logic->endgame)(logic);
}

void shutdowngamestate(void)
{
    setrulesetbehavior(Ruleset_None);
    destroymovelist(&state.moves);
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
    if (hassolution(state.game) && !(state.game->sgflags & SGF_REPLACEABLE)
				&& state.currenttime >= state.game->besttime)
	return FALSE;

    state.game->besttime = state.currenttime;
    state.game->sgflags &= ~SGF_REPLACEABLE;
    state.game->savedrndslidedir = state.initrndslidedir;
    state.game->savedrndseed = getinitialseed(&state.mainprng);
    copymovelist(&state.game->savedsolution, &state.moves);
    return TRUE;
}
