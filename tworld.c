/* tworld.c: The top-level module.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"defs.h"
#include	"err.h"
#include	"series.h"
#include	"play.h"
#include	"score.h"
#include	"solution.h"
#include	"help.h"
#include	"oshw.h"
#include	"cmdline.h"

/* Define this symbol to compile in a directory for the data files.
 */
#ifndef DATADIR
#define	DATADIR	""
#endif

/* Define this symbol to compile in a directory for the saved-game files.
 */
#ifndef SAVEDIR
#define	SAVEDIR	""
#endif

/* The data needed to identify what is being played.
 */
typedef	struct gamespec {
    gameseries	series;		/* the complete set of levels */
    int		currentgame;	/* which level is currently selected */
    int		playback;	/* TRUE if in playback mode */
} gamespec;

/* Structure used to pass data back from initoptionswithcmdline().
 */
typedef	struct startupdata {
    char       *filename;	/* which data file to use */
    int		listseries;	/* TRUE if the files should be listed */
    int		listscores;	/* TRUE if the scores should be listed */
} startupdata;

/* Online help.
 */
static char const *yowzitch = 
	"Usage: tworld [-hvls] [-D DIR] [-S DIR] [NAME]\n"
	"   -D  Read data files from DIR instead of the default\n"
	"   -S  Save games in DIR instead of the default\n"
	"   -l  Display the list of available data files and exit\n"
	"   -s  Display the scores for the selected data files and exit\n"
	"   -h  Display this help and exit\n"
	"   -v  Display version information and exit\n"
	"NAME specifies which setup file to read.\n"
	"(Press ? during the game for further help.)\n";

/* Online version data.
 */
static char const *vourzhon =
	"TileWorld, version 0.5.1.\n"
	"Copyright (C) 2001 by Brian Raiter, under the terms of the GNU\n"
	"General Public License; either version 2 of the License, or at\n"
	"your option any later version.\n"
	"This program is distributed without any warranty, express or\n"
	"implied. See COPYING for details.\n"
	"(The author also requests that you voluntarily refrain from\n"
	"redistributing this version of the program, as it is still pretty\n"
	"messy.)\n"
	"Please direct bug reports to breadbox@muppetlabs.com, or post them\n"
	"on annexcafe.chips.challenge.\n";

/*
 * The top-level user interface functions.
 */

/* A callback function for handling the keyboard while displaying a
 * scrolling list.
 */
static int scrollinputcallback(int *move)
{
    switch (input(TRUE)) {
      case CmdPrev10:		*move = -2;		break;
      case CmdNorth:		*move = -1;		break;
      case CmdPrev:		*move = -1;		break;
      case CmdPrevLevel:	*move = -1;		break;
      case CmdSouth:		*move = +1;		break;
      case CmdNext:		*move = +1;		break;
      case CmdNextLevel:	*move = +1;		break;
      case CmdNext10:		*move = +2;		break;
      case CmdProceed:		*move = TRUE;		return FALSE;
      case CmdQuitLevel:	*move = FALSE;		return FALSE;
      case CmdQuit:					exit(0);
    }
    return TRUE;
}

/* Display the user's current score.
 */
static void showscores(gamespec *gs)
{
    char      **texts;
    char const *header;
    int		listsize, n;

    if (!createscorelist(&gs->series, &texts, &listsize, &header)) {
	ding();
	return;
    }
    n = gs->currentgame;
    setsubtitle(NULL);
    if (displaylist("SCORES", header, (char const**)texts, listsize, &n,
		    scrollinputcallback))
	if (n < listsize - 1)
	    gs->currentgame = n;
    freescorelist(texts, listsize);
}

/* Get a keystroke from the user at the completion of the current
 * level.
 */
static void endinput(gamespec *gs, int status)
{
    displayendmessage(status > 0);

    for (;;) {
	switch (input(TRUE)) {
	  case CmdPrev10:	gs->currentgame -= 10;		return;
	  case CmdPrevLevel:	--gs->currentgame;		return;
	  case CmdPrev:		--gs->currentgame;		return;
	  case CmdSameLevel:					return;
	  case CmdSame:						return;
	  case CmdNextLevel:	++gs->currentgame;		return;
	  case CmdNext:		++gs->currentgame;		return;
	  case CmdNext10:	gs->currentgame += 10;		return;
	  case CmdProceed:	if (status > 0) ++gs->currentgame; return;
	  case CmdPlayback:	gs->playback = !gs->playback;	return;
	  case CmdHelp:		runhelp();			break;
	  case CmdSeeScores:	showscores(gs);			return;
	  case CmdQuitLevel:					exit(0);
	  case CmdQuit:						exit(0);
	}
    }
}

/* Play the current level. Return when the user completes the level or
 * requests something else.
 */
static void playgame(gamespec *gs)
{
    int	cmd, n;

    drawscreen();
    cmd = input(TRUE);
    switch (cmd) {
      case CmdNorth: case CmdWest:			break;
      case CmdSouth: case CmdEast:			break;
      case CmdPrev10:		gs->currentgame -= 10;	return;
      case CmdPrev:		--gs->currentgame;	return;
      case CmdPrevLevel:	--gs->currentgame;	return;
      case CmdNextLevel:	++gs->currentgame;	return;
      case CmdNext:		++gs->currentgame;	return;
      case CmdNext10:		gs->currentgame += 10;	return;
      case CmdPlayback:		gs->playback = TRUE;	return;
      case CmdSeeScores:	showscores(gs);		return;
      case CmdHelp:		runhelp();		return;
      case CmdQuitLevel:				exit(0);
      case CmdQuit:					exit(0);
      default:			cmd = CmdNone;		break;
    }

    activatekeyboard(TRUE);
    settimer(+1);
    for (;;) {
	n = doturn(cmd);
	drawscreen();
	if (n)
	    break;
	waitfortick();
	cmd = input(FALSE);
	if (cmd == CmdQuitLevel) {
	    n = -1;
	    break;
	}
	switch (cmd) {
	  case CmdNorth: case CmdWest:			break;
	  case CmdSouth: case CmdEast:			break;
	  case CmdPreserve:				break;
	  case CmdDebugCmd1:				break;
	  case CmdDebugCmd2:				break;
	  case CmdPrevLevel:		n = -1;		goto quitloop;
	  case CmdNextLevel:		n = +1;		goto quitloop;
	  case CmdSameLevel:		n = 0;		goto quitloop;
	  case CmdQuit:					exit(0);
	  case CmdPauseGame:
	    settimer(0);
	    anykey();
	    settimer(+1);
	    cmd = CmdNone;
	    break;
	  case CmdHelp:
	    settimer(0);
	    runhelp();
	    settimer(+1);
	    cmd = CmdNone;
	    break;
	  default:
	    cmd = CmdNone;
	    break;
	}
    }
    settimer(-1);
    activatekeyboard(FALSE);
    if (n > 0 && replacesolution())
	savesolutions(&gs->series);
    endinput(gs, n);
    return;

  quitloop:
    settimer(-1);
    activatekeyboard(FALSE);
    gs->currentgame += n;
}

/* Play back the user's best solution for the current level.
 */
static void playbackgame(gamespec *gs)
{
    int	n;

    drawscreen();

    settimer(+1);
    for (;;) {
	n = doturn(CmdNone);
	drawscreen();
	if (n)
	    break;
	waitfortick();
	switch (input(FALSE)) {
	  case CmdPrevLevel:	--gs->currentgame;		goto quitloop;
	  case CmdNextLevel:	++gs->currentgame;		goto quitloop;
	  case CmdSameLevel:					goto quitloop;
	  case CmdPlayback:	gs->playback = FALSE;		goto quitloop;
	  case CmdQuitLevel:	gs->playback = FALSE;		goto quitloop;
	  case CmdQuit:						exit(0);
	  case CmdPauseGame:
	    settimer(0);
	    anykey();
	    settimer(+1);
	    break;
	  case CmdHelp:
	    settimer(0);
	    runhelp();
	    settimer(+1);
	    break;
	}
    }
    settimer(-1);
    gs->playback = FALSE;
    endinput(gs, n);
    return;

  quitloop:
    settimer(-1);
    gs->playback = FALSE;
}

/*
 * Ancillary top-level functions.
 */

/* Parse the command-line options and arguments, and initialize the
 * user-controlled options.
 */
static int initoptionswithcmdline(int argc, char *argv[], startupdata *start)
{
    cmdlineinfo	opts;
    char const *dir;
    unsigned	maxpath;
    int		ch;

    maxpath = getpathbufferlen();

    datadir = getpathbuffer();
    strcpy(datadir, DATADIR);
    savedir = getpathbuffer();
    strcpy(savedir, SAVEDIR);
    start->filename = getpathbuffer();
    *start->filename = '\0';
    start->listseries = FALSE;
    start->listscores = FALSE;

    initoptions(&opts, argc - 1, argv + 1, "D:S:hlsv");
    while ((ch = readoption(&opts)) >= 0) {
	switch (ch) {
	  case 0:
	    if (*start->filename) {
		fputs(yowzitch, stderr);
		exit(EXIT_FAILURE);
	    }
	    strncpy(start->filename, opts.val, maxpath - 1);
	    break;
	  case 'D':	strncpy(datadir, opts.val, maxpath - 1);	break;
	  case 'S':	strncpy(savedir, opts.val, maxpath - 1);	break;
	  case 'l':	start->listseries = TRUE;			break;
	  case 's':	start->listscores = TRUE;			break;
	  case 'h':	fputs(yowzitch, stdout); 	   exit(EXIT_SUCCESS);
	  case 'v':	fputs(vourzhon, stdout); 	   exit(EXIT_SUCCESS);
	  case ':':
	    fprintf(stderr, "option requires an argument: -%c\n%s",
			    opts.opt, yowzitch);
	    return FALSE;
	  case '?':
	    fprintf(stderr, "unrecognized option: -%c\n%s",
			    opts.opt, yowzitch);
	    return FALSE;
	  default:
	    fputs(yowzitch, stderr);
	    return FALSE;
	}
    }

    if (!*datadir) {
	if ((dir = getenv("TWORLDDATADIR"))) {
	    if (strlen(dir) >= maxpath)
		die("value of TWORLDDATADIR env variable is too long");
	    strcpy(datadir, dir);
	} else {
	    datadir[0] = '.';
	    datadir[1] = '\0';
	    combinepath(datadir, "data");
	}
    }
    datadir[maxpath - 1] = '\0';
    if (!*savedir) {
	if ((dir = getenv("TWORLDSAVEDIR"))) {
	    if (strlen(dir) >= maxpath)
		die("value of TWORLDSAVEDIR env variable is too long");
	    strcpy(savedir, dir);
	} else if ((dir = getenv("HOME")) && strlen(dir) < maxpath - 8) {
	    strcpy(savedir, dir);
	    combinepath(savedir, ".tworld");
	} else {
	    savedir[0] = '.';
	    savedir[1] = '\0';
	    combinepath(savedir, "save");
	}
    }
    savedir[maxpath - 1] = '\0';

    if (start->listscores && !*start->filename)
	strcpy(start->filename, "chips.dat");
    start->filename[maxpath - 1] = '\0';

    return TRUE;
}

/* Initialize the other modules.
 */
static int initializesystem(void)
{
    return oshwinitialize();
}

/* Initialize the program. The list of available data files is drawn
 * up; if only one is found, it is selected automatically. Otherwise
 * the list is presented to the user, and their selection determines
 * which one is used. printseriesandquit causes the list of data files
 * to be displayed on stdout before initializing graphic output.
 * printscoresandquit causes the scores for a single data file to be
 * displayed on stdout before initializing graphic output (which
 * requires that only one data file is considered for selection).
 * Once a data file is selected, the first level for which the user
 * has no saved solution is found and selected, or the first level if
 * the user has solved every level.
 */
static int startup(gamespec *gs, char const *preferredfile,
		   int printseriesandquit, int printscoresandquit)
{
    gameseries *serieslist;
    char      **texts;
    char const *header;
    int		listsize, n;

    if (!createserieslist(preferredfile, &serieslist,
			  &texts, &listsize, &header))
	return FALSE;
    if (listsize < 1)
	return FALSE;

    if (printseriesandquit) {
	puts(header);
	for (n = 0 ; n < listsize ; ++n)
	    puts(texts[n]);
	exit(EXIT_SUCCESS);
    }

    if (listsize == 1) {
	gs->series = serieslist[0];
	if (!readseriesfile(&gs->series))
	    return FALSE;
	if (printscoresandquit) {
	    freeserieslist(texts, listsize);
	    if (!createscorelist(&gs->series, &texts, &listsize, &header))
		return FALSE;
	    puts(header);
	    for (n = 0 ; n < listsize ; ++n)
		puts(texts[n]);
	    exit(EXIT_SUCCESS);
	}
	if (!initializesystem())
	    return FALSE;
    } else {
	if (!initializesystem())
	    return FALSE;
	n = 0;
	setsubtitle(NULL);
	if (!displaylist("Welcome to Tile World. Select your destination.",
			 header, (char const**)texts, listsize, &n,
			 scrollinputcallback))
	    exit(EXIT_SUCCESS);
	if (n < 0 || n >= listsize)
	    return FALSE;
	gs->series = serieslist[n];
	if (!readseriesfile(&gs->series))
	    return FALSE;
    }

    freeserieslist(texts, listsize);
    free(serieslist);

    gs->currentgame = 0;
    for (n = 0 ; n < gs->series.total ; ++n) {
	if (!hassolution(gs->series.games + n)) {
	    gs->currentgame = n;
	    break;
	}
    }
    gs->playback = FALSE;
    return TRUE;
}

/*
 * main().
 */

/* Initialize the system and enter an infinite loop of displaying a
 * playing the selected level.
 */
int main(int argc, char *argv[])
{
    startupdata	start;
    gamespec	spec;

    if (!initoptionswithcmdline(argc, argv, &start))
	return EXIT_FAILURE;

    if (!startup(&spec, start.filename, start.listseries, start.listscores))
	return EXIT_FAILURE;

    activatekeyboard(FALSE);

    for (;;) {
	if (spec.currentgame < 0) {
	    spec.currentgame = 0;
	    ding();
	}
	if (spec.currentgame >= spec.series.total) {
	    spec.currentgame = spec.series.total - 1;
	    ding();
	}
	if (spec.playback
		&& !hassolution(spec.series.games + spec.currentgame)) {
	    spec.playback = FALSE;
	    ding();
	}

	if (!initgamestate(&spec.series, spec.currentgame, spec.playback)) {
	    ding();
	    break;
	}
	setsubtitle(spec.series.games[spec.currentgame].name);

	if (spec.playback)
	    playbackgame(&spec);
	else
	    playgame(&spec);
    }

    return EXIT_FAILURE;
}
