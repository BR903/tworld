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
#include	"userio.h"

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
    int		replay;		/* TRUE if playbacks are to be displayed */
    int		silence;	/* FALSE if we are allowed to ring the bell */
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
	"chips, version 0.3. Copyright (C) 2001 by Brian Raiter\n"
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

/* TRUE if currently in instant-replay mode.
 */
static int		replay = FALSE;

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

static int translatekey(int ch, int status)
{
    switch (ch) {
      case 'k':		return CmdNorth;
      case 'h':		return CmdWest;
      case 'j':		return CmdSouth;
      case 'l':		return CmdEast;
      case '\t':	return CmdReplay;
      case '\016':	return CmdNextLevel;
      case '\020':	return CmdPrevLevel;
      case '\022':	return CmdSameLevel;
      case 'N':		return CmdNextLevel;
      case 'P':		return CmdPrevLevel;
      case 'R':		return CmdSameLevel;
      case 'n':		return status ? CmdNextLevel : CmdNone;
      case 'p':		return status ? CmdPrevLevel : CmdNone;
      case 'r':		return status ? CmdSameLevel : CmdNone;
      case 'q':		return status ? CmdQuit : CmdQuitLevel;
      case 'Q':		return CmdQuit;
      case '?':		return CmdHelp;
      case '\n':
      case '\r':
      case ' ':
	if (status > 0)
	    return CmdNextLevel;
	else if (status < 0)
	    return CmdSameLevel;
	return CmdNone;
      case 'd':
	return CmdDebugDumpMap;
      case 'D':
	return CmdDebugCmd;
    }
    return CmdNone;
}

/* Get a keystroke from the user at the completion of the current
 * level. The default behavior of advancing to the next level is
 * overridden by a non-zero return value.
 */
static int endinput(int status)
{
    displayendmessage(status > 0);

    for (;;) {
	inputwait();
	switch (translatekey(input(), status)) {
	  case CmdPrevLevel:	return -1;
	  case CmdSameLevel:	return 0;
	  case CmdNextLevel:	return +1;
	  case CmdProceed:	return status > 0 ? +1 : 0;
	  case CmdReplay:	replay = !replay; return 0;
	  case CmdQuit:		exit(0);
	}
    }
}

/* Play the current level. Return when the user solves the level or
 * requests a different level.
 */
static int playgame(void)
{
    int	k, n;

    drawscreen();
    inputwait();
    k = translatekey(input(), -1);
    switch (k) {
      case CmdPrevLevel:			return -1;
      case CmdNextLevel:			return +1;
      case CmdReplay:		replay = TRUE;	return 0;
      case CmdHelp:		runhelp();	return 0;
      case CmdQuit:				exit(0);
    }
    settimer(+1);
    for (;;) {
	n = doturn(k);
	drawscreen();
	if (n)
	    break;
	waitfortick();
	k = translatekey(input(), 0);
	if (k == CmdQuitLevel) {
	    n = -1;
	    break;
	}
	switch (k) {
	  case CmdPrevLevel:	n = -1;		goto quitloop;
	  case CmdNextLevel:	n = +1;		goto quitloop;
	  case CmdSameLevel:	n = 0;		goto quitloop;
	  case CmdQuitLevel:	n = -1;		goto quitloop;
	  case CmdQuit:				exit(0);
	  case CmdHelp:
	    settimer(0);
	    runhelp();
	    settimer(+1);
	    break;
	}
    }
    settimer(-1);
    if (n > 0 && replacesolution(FALSE))
	savesolutions(serieslist + currentseries);
    return endinput(n);

  quitloop:
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
	n = translatekey(input(), 0);
	switch (n) {
	  case CmdPrevLevel:	n = -1;			goto quitloop;
	  case CmdNextLevel:	n = +1;			goto quitloop;
	  case CmdSameLevel:	n = 0;			goto quitloop;
	  case CmdReplay:	n = 0;	replay = FALSE;	goto quitloop;
	  case CmdQuitLevel:	n = 0;	replay = FALSE;	goto quitloop;
	  case CmdQuit:					exit(0);
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
    char const *dir;
    int		ch;

    programname = argv[0];

    datadir = getpathbuffer();
    copypath(datadir, DATADIR);

    savedir = getpathbuffer();
    if ((dir = getenv("CHIPSSAVEDIR"))) {
	strncpy(savedir, dir, sizeof savedir - 1);
	savedir[sizeof savedir - 1] = '\0';
    } else if ((dir = getenv("HOME")))
	sprintf(savedir, "%.*s/.chips", (int)(sizeof savedir - 11), dir);

    start->filename = getpathbuffer();
    *start->filename = '\0';
    start->level = 0;
    start->replay = FALSE;
    start->silence = FALSE;
    start->listseries = FALSE;

    while ((ch = getopt(argc, argv, "0123456789D:S:hlqrv")) != EOF) {
	switch (ch) {
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
	    start->level = start->level * 10 + ch - '0';
	    break;
	  case 'D':	strncpy(datadir, optarg, sizeof datadir - 1);	break;
	  case 'S':	strncpy(savedir, optarg, sizeof savedir - 1);	break;
	  case 'q':	start->silence = TRUE;				break;
	  case 'r':	start->replay = TRUE;				break;
	  case 'l':	start->listseries = TRUE;			break;
	  case 'h':	fputs(yowzitch, stdout); 	exit(EXIT_SUCCESS);
	  case 'v':	fputs(vourzhon, stdout); 	exit(EXIT_SUCCESS);
	  default:	fputs(yowzitch, stderr); 	exit(EXIT_FAILURE);
	}
    }

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

    if (!ioinitialize(start.silence) || !timerinitialize()
				     || !randominitialize())
	die("Failed to initialize.");

    for (;;) {
	selectgame(serieslist[currentseries].games + currentgame);
	if (replay) {
	    if (initgamestate(TRUE))
		playbackgame();
	    else
		ding();
	    replay = FALSE;
	    n = 0;
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

    return EXIT_SUCCESS;
}
