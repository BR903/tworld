/* chips.c: Top-level functions.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<getopt.h>
#include	"gen.h"
#include	"dirio.h"
#include	"fileread.h"
#include	"solutions.h"
#include	"state.h"
#include	"timer.h"
#include	"random.h"
#include	"help.h"
#include	"userin.h"
#include	"userout.h"

/* The default directory for the level files.
 */
#ifndef DATADIR
#define	DATADIR		"/usr/local/share/chips"
#endif

/* Structure used to pass data back from initoptionswithcmdline().
 */
typedef	struct startupdata {
    char       *filename;	/* which puzzle file to use */
    int		level;		/* which puzzle to start at */
    int		sludge;		/* slowdown factor (1 == normal speed) */
    int		silence;	/* FALSE if we are allowed to ring the bell */
    int		rawkeys;	/* TRUE if raw keyboard mode is permitted */
    int		listseries;	/* TRUE if the files should be displayed */
} startupdata;

/* Online help.
 */
static char const *yowzitch = 
	"Usage: chips [-hvqlwW] [-d DIR] [-s DIR] [NAME] [-LEVEL]\n"
	"   -h  Display this help\n"
	"   -v  Display version information\n"
	"   -l  Print out the list of available data files\n"
	"   -D  Read data files from DIR instead of the default\n"
	"   -S  Save games in DIR instead of the default\n"
	"   -q  Be quiet; don't ring the bell\n"
	"NAME specifies which setup file to read.\n"
	"LEVEL specifies which level number to start with.\n"
	"(Press ? during the game for further help.)\n";

/* Version information.
 */
static char const *vourzhon =
	"chips, version 0.6. Copyright (C) 2001 by Brian Raiter\n"
	"under the terms of the GNU General Public License.\n";

/* The list of available puzzle files.
 */
static gameseries      *serieslist = NULL;

/* The size of the serieslist array.
 */
static int		seriescount = 0;

/* The index of the current series.
 */
static int		currentseries = 0;

/* The index of the current puzzle within the current series.
 */
static int		currentgame = 0;

/* TRUE if currently in playback mode.
 */
static int		playback = FALSE;

/*
 * Game-choosing functions
 */

/* Ensure that the current level has actually been read into
 * memory. If currentgame is less than zero, or exceeds the total
 * number of levels in the current series, then change the current
 * series if possible. (currentgame and currentseries are
 * appropriately updated in this case.) FALSE is returned if the
 * current level does not exist.
 */
int readlevel(void)
{
    int	level = currentgame;

    while (level < 0) {
	if (!currentseries)
	    return FALSE;
	--currentseries;
	level += serieslist[currentseries].count;
    }
    while (level >= serieslist[currentseries].count) {
	if (!readlevelinseries(serieslist + currentseries, level)) {
	    if (level < serieslist[currentseries].count)
		return FALSE;
	    if (currentseries + 1 >= seriescount)
		return FALSE;
	    level -= serieslist[currentseries].count;
	    ++currentseries;
	}
    }
    currentgame = level;
    return TRUE;
}

/* Print out a list of the available level files.
 */
static void displayserieslist(void)
{
    int	namewidth;
    int	i, n;

    namewidth = 0;
    for (i = 0 ; i < seriescount ; ++i) {
	n = strlen(serieslist[i].filename);
	if (!readlevelinseries(serieslist + i, 0)) {
	    *serieslist[i].filename = '\0';
	    continue;
	}
	if (n > 4 && !strcmp(serieslist[i].filename + n - 4, ".dat")) {
	    n -= 4;
	    serieslist[i].filename[n] = '\0';
	}
	if (n > namewidth)
	    namewidth = n;
    }
    for (i = 0 ; i < seriescount ; ++i) {
	if (*serieslist[i].name)
	    printf("%-*.*s  %s\n", namewidth, namewidth,
				   serieslist[i].filename,
				   serieslist[i].name);
	else
	    puts(serieslist[i].filename);
    }
}

/* Initialize currentseries to point to the chosen level file, and
 * currentgame to the chosen level. If the user didn't request a
 * specific file, start at the first one and use all of them. If the
 * user didn't request a specific level, select the first one for
 * which the user has not found a solution, or the first one if all
 * available levels have been solved.
 */
static void pickstartinggame(char const *startfile, int startlevel)
{
    int	i, n;

    if (*startfile) {
	n = strlen(startfile);
	for (i = 0 ; i < seriescount ; ++i)
	    if (!strncmp(serieslist[i].filename, startfile, n))
		break;
	if (i == seriescount)
	    die("Couldn't find any data file named \"%s\".", startfile);
	serieslist += i;
	seriescount = 1;
    }
    currentseries = 0;

    if (startlevel) {
	currentgame = startlevel - 1;
	if (!readlevel()) {
	    if (*startfile)
		die("Couldn't find a level %d in %s.", startlevel, startfile);
	    else
		die("Couldn't find a level %d to start with.", startlevel);
	}
    } else {
	currentgame = 0;
	if (!readlevel()) {
	    if (*startfile)
		die("Couldn't find any levels in %s.", startfile);
	    else
		die("Couldn't find any levels in any files in %s.", datadir);
	}
	while (serieslist[currentseries].games[currentgame].besttime) {
	    ++currentgame;
	    if (!readlevel()) {
		currentseries = 0;
		currentgame = 0;
		break;
	    }
	}
    }
}

/*
 * User interface functions
 */

static void setplaymode(int playing)
{
    if (playing) {
	settimer(+1);
    } else {
	settimer(0);
    }
}

/* Get a keystroke from the user at the completion of the current
 * level.
 */
static int endinput(int status)
{
    displayendmessage(status > 0);

    for (;;) {
	switch (input(TRUE)) {
	  case CmdPrevLevel:				return -1;
	  case CmdSameLevel:				return 0;
	  case CmdNextLevel:				return +1;
	  case CmdPrev:					return -1;
	  case CmdSame:					return 0;
	  case CmdNext:					return +1;
	  case CmdPlayback:	playback = !playback;	return 0;
	  case CmdQuitLevel:				exit(0);
	  case CmdQuit:					exit(0);
	  case CmdHelp:		runhelp();		break;
	  case CmdProceed:	return status > 0 ? +1 : 0;
	}
    }
}

#if 0
/* Play through a single tick of the current game. The return value
 * indicates if the current game has ended.
 */
static int playtick(int *ret)
{
    int	cmd;

    cmd = translatekey(input(FALSE), 0);

    switch (cmd) {
      case CmdHelp:
	setplaymode(FALSE);
	runhelp();
	setplaymode(TRUE);
	*ret = 0;
	break;
      case CmdPrevLevel:		*ret = -1;		return FALSE;
      case CmdNextLevel:		*ret = +1;		return FALSE;
      case CmdSameLevel:		*ret = 0;		return FALSE;
      case CmdQuitLevel:		*ret = -1;		break;
      case CmdQuit:			exit(0);
      case CmdNorth: case CmdWest:	*ret = doturn(cmd);	break;
      case CmdSouth: case CmdEast:	*ret = doturn(cmd);	break;
      case CmdPreserve:			*ret = doturn(cmd);	break;
      case CmdDebugCmd:			*ret = doturn(cmd);	break;
      case CmdDebugDumpMap:		*ret = doturn(cmd);	break;
      default:				*ret = doturn(NIL);	break;
    }

    drawscreen();
    return TRUE;
}
#endif

/* Play the current level. Return when the user solves the level or
 * requests a different level.
 */
static int playgame(void)
{
    int	cmd, n;

    drawscreen();
    cmd = input(TRUE);
    switch (cmd) {
      case CmdNorth: case CmdWest:			break;
      case CmdSouth: case CmdEast:			break;
      case CmdPrevLevel:				return -1;
      case CmdNextLevel:				return +1;
      case CmdPrev:					return -1;
      case CmdNext:					return +1;
      case CmdPlayback:		playback = TRUE;	return 0;
      case CmdHelp:		runhelp();		return 0;
      case CmdQuitLevel:				exit(0);
      case CmdQuit:					exit(0);
      default:			cmd = NIL;		break;
    }
    setplaymode(TRUE);
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
	    setplaymode(FALSE);
	    anykey();
	    setplaymode(TRUE);
	    cmd = NIL;
	    break;
	  case CmdHelp:
	    setplaymode(FALSE);
	    runhelp();
	    setplaymode(TRUE);
	    cmd = NIL;
	    break;
	  default:
	    cmd = NIL;
	    break;
	}
    }
    setplaymode(FALSE);
    settimer(-1);
    if (n > 0 && replacesolution())
	savesolutions(serieslist + currentseries);
    return endinput(n);

  quitloop:
    setplaymode(FALSE);
    settimer(-1);
    return n;
}

static int playbackgame(void)
{
    int	n;

    drawscreen();
    settimer(+1);
    for (;;) {
	n = doturn(NIL);
	drawscreen();
	if (n)
	    break;
	waitfortick();
	n = input(FALSE);
	switch (n) {
	  case CmdPrevLevel:	n = -1;				goto quitloop;
	  case CmdNextLevel:	n = +1;				goto quitloop;
	  case CmdSameLevel:	n = 0;				goto quitloop;
	  case CmdPlayback:	n = 0;	playback = FALSE;	goto quitloop;
	  case CmdQuitLevel:	n = 0;	playback = FALSE;	goto quitloop;
	  case CmdQuit:						exit(0);
	  case CmdHelp:
	    settimer(0);
	    runhelp();
	    settimer(+1);
	    break;
	}
    }
    settimer(-1);
    return endinput(n);

  quitloop:
    settimer(-1);
    return n > 0 ? +1 : 0;
}

/*
 * Main functions
 */

/* Initialize the user-controlled options to their default values, and
 * then parse the command-line options and arguments.
 */
static void initoptionswithcmdline(int argc, char *argv[], startupdata *start)
{
    extern int xviewshift, yviewshift;
    char const *dir;
    int		len;
    int		ch;

    programname = argv[0];

    len = getpathbufferlen();

    datadir = getpathbuffer();
    copypath(datadir, DATADIR);

    savedir = getpathbuffer();
    if ((dir = getenv("CHIPSSAVEDIR"))) {
	strncpy(savedir, dir, len - 1);
	savedir[len - 1] = '\0';
    } else if ((dir = getenv("HOME")))
	sprintf(savedir, "%.*s/.chips", len, dir);

    start->filename = getpathbuffer();
    *start->filename = '\0';
    start->level = 0;
    start->sludge = 0;
    start->rawkeys = TRUE;
    start->silence = FALSE;
    start->listseries = FALSE;

    while ((ch = getopt(argc, argv, "0123456789D:S:hlkqs:vx:y:")) != EOF) {
	switch (ch) {
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
	    start->level = start->level * 10 + ch - '0';
	    break;
	  case 'D':	strncpy(datadir, optarg, len - 1);		break;
	  case 'S':	strncpy(savedir, optarg, len - 1);		break;
	  case 'q':	start->silence = TRUE;				break;
	  case 'k':	start->rawkeys = FALSE;				break;
	  case 'l':	start->listseries = TRUE;			break;
	  case 's':	start->sludge = atoi(optarg);			break;
	  case 'x':	xviewshift = atoi(optarg);			break;
	  case 'y':	yviewshift = atoi(optarg);			break;
	  case 'h':	fputs(yowzitch, stdout); 	exit(EXIT_SUCCESS);
	  case 'v':	fputs(vourzhon, stdout); 	exit(EXIT_SUCCESS);
	  default:	fputs(yowzitch, stderr); 	exit(EXIT_FAILURE);
	}
    }
    if (start->sludge < 1)
	start->sludge = 1;

    if (optind < argc)
	start->filename = argv[optind++];
    if (optind < argc) {
	fputs(yowzitch, stderr);
	exit(EXIT_FAILURE);
    }
}

/* main().
 */
int main(int argc, char *argv[])
{
    startupdata	start;
    int		n;

    initoptionswithcmdline(argc, argv, &start);

    if (!getseriesfiles(start.filename, &serieslist, &seriescount))
	return EXIT_FAILURE;

    if (start.listseries) {
	displayserieslist();
	return EXIT_SUCCESS;
    }

    pickstartinggame(start.filename, start.level);

    if (!inputinitialize(start.rawkeys) || !outputinitialize(start.silence)
					|| !timerinitialize(start.sludge)
					|| !randominitialize())
	die("Failed to initialize.");

    setkeypolling(TRUE);

    for (;;) {
	selectgame(serieslist[currentseries].games + currentgame);
	if (playback) {
	    if (initgamestate(TRUE))
		n = playbackgame();
	    else
		ding();
	    playback = FALSE;
	    /*n = 0;*/
	} else {
	    initgamestate(FALSE);
	    n = playgame();
	}
	currentgame += n;
	if (!readlevel()) {
	    ding();
	    currentgame -= n;
	}
    }

    setkeypolling(FALSE);

    return EXIT_SUCCESS;
}
