/* tworld.c: The top-level module.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"defs.h"
#include	"err.h"
#include	"series.h"
#include	"res.h"
#include	"play.h"
#include	"score.h"
#include	"solution.h"
#include	"help.h"
#include	"oshw.h"
#include	"cmdline.h"
#include	"ver.h"

/* Bell-ringing macro.
 */
#define	bell()	(silence ? (void)0 : ding())

/* The data needed to identify what is being played.
 */
typedef	struct gamespec {
    gameseries	series;		/* the complete set of levels */
    int		currentgame;	/* which level is currently selected */
    int		playback;	/* TRUE if in playback mode */
    int		usepasswds;	/* FALSE if passwords are to be ignored */
    int		status;		/* final status of last game played */
} gamespec;

/* Structure used to pass data back from initoptionswithcmdline().
 */
typedef	struct startupdata {
    char       *filename;	/* which data file to use */
    int		levelnum;	/* a selected initial level */ 
    int		usepasswds;	/* FALSE if passwords are to be ignored */
    int		listdirs;	/* TRUE if directories should be listed */
    int		listseries;	/* TRUE if the files should be listed */
    int		listscores;	/* TRUE if the scores should be listed */
    int		listtimes;	/* TRUE if the times should be listed */
} startupdata;

/* Structure used to hold the information regarding available series.
 */
typedef	struct seriesdata {
    gameseries *list;
    int		count;
    tablespec	table;
} seriesdata;

/* TRUE suppresses sound and the console bell.
 */
static int	silence = FALSE;

/* FALSE suppresses all password checking.
 */
static int	usepasswds = TRUE;

/* TRUE if the user requested an idle-time histogram.
 */
static int	showhistogram = FALSE;

/*
 * The program's text-mode output functions.
 */

/* Render a table to standard output.
 */
void printtable(FILE *out, tablespec const *table)
{
    int	       *colsizes;
    int		len;
    int		c, n, x, y;

    colsizes = malloc(table->cols * sizeof *colsizes);
    for (x = 0 ; x < table->cols ; ++x)
	colsizes[x] = 0;
    n = 0;
    for (y = 0 ; y < table->rows ; ++y) {
	for (x = 0 ; x < table->cols ; ++n) {
	    c = table->items[n][0] - '0';
	    if (c == 1) {
		len = strlen(table->items[n] + 2);
		if (len > colsizes[x])
		    colsizes[x] = len;
	    }
	    x += c;
	}
    }

    n = -table->sep;
    for (x = 0 ; x < table->cols ; ++x)
	n += colsizes[x] + table->sep;
    if (n > 79) {
	n = n - 79;
	if (table->collapse < 0)
	    return;
	if (colsizes[table->collapse] <= n)
	    return;
	colsizes[table->collapse] -= n;
    }

    n = 0;
    for (y = 0 ; y < table->rows ; ++y) {
	for (x = 0 ; x < table->cols ; ++n) {
	    if (x)
		fprintf(out, "%*s", table->sep, "");
	    c = table->items[n][0] - '0';
	    len = -table->sep;
	    while (c--)
		len += colsizes[x++] + table->sep;
	    if (table->items[n][1] == '-')
		fprintf(out, "%-*.*s", len, len, table->items[n] + 2);
	    else if (table->items[n][1] == '+')
		fprintf(out, "%*.*s", len, len, table->items[n] + 2);
	    else if (table->items[n][1] == '.') {
		len -= (len - strlen(table->items[n] + 3)) / 2;
		fprintf(out, "%*.*s", len, len, table->items[n] + 2);
	    }
	}
	fputc('\n', out);
    }
    free(colsizes);
}

static void printdirectories(void)
{
    printf("Resource files read from:        %s\n", resdir);
    printf("Level sets read from:            %s\n", seriesdir);
    printf("Configured data files read from: %s\n", seriesdatdir);
    printf("Solution files saved in:         %s\n", savedir);
}

/*
 * Callback functions for oshw.
 */

/* A callback functions for handling the keyboard while collecting
 * input.
 */
static int keyinputcallback(void)
{
    int	ch;

    ch = input(TRUE);
    if (isalpha(ch))
	return toupper(ch);
    else if (ch == CmdWest)
	return '\b';
    else if (ch == CmdProceed)
	return '\n';
    else if (ch == CmdQuitLevel)
	return -1;
    else if (ch == CmdQuit)
	exit(0);
    return 0;
}

/* A callback function for handling the keyboard while displaying a
 * scrolling list.
 */
static int scrollinputcallback(int *move)
{
    int cmd;
    switch ((cmd = input(TRUE))) {
      case CmdPrev10:		*move = SCROLL_HALFPAGE_UP;	break;
      case CmdNorth:		*move = SCROLL_UP;		break;
      case CmdPrev:		*move = SCROLL_UP;		break;
      case CmdPrevLevel:	*move = SCROLL_UP;		break;
      case CmdSouth:		*move = SCROLL_DN;		break;
      case CmdNext:		*move = SCROLL_DN;		break;
      case CmdNextLevel:	*move = SCROLL_DN;		break;
      case CmdNext10:		*move = SCROLL_HALFPAGE_DN;	break;
      case CmdProceed:		*move = CmdProceed;		return FALSE;
      case CmdQuitLevel:	*move = CmdQuitLevel;		return FALSE;
      case CmdHelp:		*move = CmdHelp;		return FALSE;
      case CmdQuit:						exit(0);

    }
    return TRUE;
}

/*
 * Basic actions.
 */

/* Mark the current level's solution as replaceable.
 */
static void replaceablesolution(gamespec *gs)
{
    gs->series.games[gs->currentgame].sgflags ^= SGF_REPLACEABLE;
}

/* Mark the current level's password as known to the user.
 */
static void passwordseen(gamespec *gs)
{
    if (!(gs->series.games[gs->currentgame].sgflags & SGF_HASPASSWD)) {
	gs->series.games[gs->currentgame].sgflags |= SGF_HASPASSWD;
	savesolutions(&gs->series);
    }
}

/* Change the current game, ensuring that the user is not granted
 * access to a forbidden level. FALSE is returned if the current game
 * was not changed.
 */
static int changecurrentgame(gamespec *gs, int offset)
{
    int	sign, m, n;

    if (offset == 0)
	return FALSE;

    m = gs->currentgame;
    n = m + offset;
    if (n < 0)
	n = 0;
    else if (n >= gs->series.total)
	n = gs->series.total - 1;

    if (gs->usepasswds) {
	sign = offset < 0 ? -1 : +1;
	for ( ; n >= 0 && n < gs->series.total ; n += sign) {
	    if (hassolution(gs->series.games + n)
			|| (gs->series.games[n].sgflags & SGF_HASPASSWD)
			|| (n > 0 && hassolution(gs->series.games + n - 1))) {
		m = n;
		break;
	    }
	}
	n = m;
	if (n == gs->currentgame && offset != sign) {
	    n = gs->currentgame + offset - sign;
	    for ( ; n != gs->currentgame ; n -= sign) {
		if (n < 0 || n >= gs->series.total)
		    continue;
		if (hassolution(gs->series.games + n)
			|| (gs->series.games[n].sgflags & SGF_HASPASSWD)
			|| (n > 0 && hassolution(gs->series.games + n - 1))) {
		    m = n;
		    break;
		}
	    }
	}
    }

    if (n == gs->currentgame) {
	bell();
	return FALSE;
    }
    gs->currentgame = n;
    return TRUE;
}

/*
 *
 */

/* Display the user's current score.
 */
static int showscores(gamespec *gs)
{
    tablespec	table;
    int	       *levellist;
    int		count, f, n;

    if (!createscorelist(&gs->series, gs->usepasswds,
			 &levellist, &count, &table)) {
	bell();
	return FALSE;
    }
    setsubtitle(NULL);
    for (n = 0 ; n < count ; ++n)
	if (levellist[n] == gs->currentgame)
	    break;
    for (;;) {
	f = displaylist(gs->series.name, &table, &n, scrollinputcallback);
	if (f == CmdProceed) {
	    n = levellist[n];
	    break;
	} else if (f == CmdQuitLevel) {
	    n = -1;
	    break;
	} else if (f == CmdHelp)
	    onlinehelp(Help_None);
    }
    freescorelist(levellist, &table);
    if (n >= 0)
	gs->currentgame = n;
    return n >= 0;
}

/* Obtain a password from the user and move to the requested level.
 */
static int selectlevelbypassword(gamespec *gs)
{
    char	passwd[5] = "";
    int		n;

    setgameplaymode(BeginInput);
    n = displayinputprompt("Enter Password", passwd, 4, keyinputcallback);
    setgameplaymode(EndInput);
    if (!n)
	return FALSE;

    n = findlevelinseries(&gs->series, 0, passwd);
    if (n < 0) {
	bell();
	return FALSE;
    }

    gs->currentgame = n;
    return TRUE;
}

/*
 * The top-level user interface functions.
 */

#define	leveldelta(n)	if (!changecurrentgame(gs, (n))) { bell(); continue; }

/* Get a keystroke from the user at the start of the current level.
 */
static int startinput(gamespec *gs)
{
    int	cmd;

    drawscreen();
    passwordseen(gs);

    for (;;) {
	cmd = input(TRUE);
	if (cmd >= CmdMoveFirst && cmd <= CmdMoveLast)
	    return cmd;
	switch (cmd) {
	  case CmdProceed:					return cmd;
	  case CmdQuitLevel:					return cmd;
	  case CmdPrev10:	leveldelta(-10);		return CmdNone;
	  case CmdPrev:		leveldelta(-1);			return CmdNone;
	  case CmdPrevLevel:	leveldelta(-1);			return CmdNone;
	  case CmdNextLevel:	leveldelta(+1);			return CmdNone;
	  case CmdNext:		leveldelta(+1);			return CmdNone;
	  case CmdNext10:	leveldelta(+10);		return CmdNone;
	  case CmdKillSolution:	replaceablesolution(gs);	break;
	  case CmdVolumeUp:	changevolume(+2, TRUE);		break;
	  case CmdVolumeDown:	changevolume(-2, TRUE);		break;
	  case CmdHelp:		onlinehelp(Help_KeysBetweenGames); break;
	  case CmdQuit:						exit(0);
	  case CmdPlayback:
	    if (prepareplayback()) {
		gs->playback = TRUE;
		return CmdProceed;
	    }
	    bell();
	    break;
	  case CmdSeeScores:
	    if (showscores(gs))
		return CmdNone;
	    break;
	  case CmdGotoLevel:
	    if (selectlevelbypassword(gs))
		return CmdNone;
	    break;
	  default:
	    continue;
	}

	drawscreen();
    }

}

/* Get a keystroke from the user at the completion of the current
 * level.
 */
static int endinput(gamespec *gs)
{
    int	bscore = 0, tscore = 0, gscore = 0;

    if (gs->status >= 0)
	getscoresforlevel(&gs->series, gs->currentgame,
			  &bscore, &tscore, &gscore);

    displayendmessage(bscore, tscore, gscore, gs->status);

    for (;;) {
	switch (input(TRUE)) {
	  case CmdPrev10:	changecurrentgame(gs, -10);	return TRUE;
	  case CmdPrevLevel:	changecurrentgame(gs, -1);	return TRUE;
	  case CmdPrev:		changecurrentgame(gs, -1);	return TRUE;
	  case CmdSameLevel:					return TRUE;
	  case CmdSame:						return TRUE;
	  case CmdNextLevel:	changecurrentgame(gs, +1);	return TRUE;
	  case CmdNext:		changecurrentgame(gs, +1);	return TRUE;
	  case CmdNext10:	changecurrentgame(gs, +10);	return TRUE;
	  case CmdGotoLevel:	selectlevelbypassword(gs);	return TRUE;
	  case CmdPlayback:	gs->playback = !gs->playback;	return TRUE;
	  case CmdSeeScores:	showscores(gs);			return TRUE;
	  case CmdKillSolution:	replaceablesolution(gs);	return TRUE;
	  case CmdHelp:		onlinehelp(Help_KeysBetweenGames); return TRUE;
	  case CmdQuitLevel:					return FALSE;
	  case CmdQuit:						exit(0);
	  case CmdProceed:
	    if (gs->status > 0)
		changecurrentgame(gs, +1);
	    return TRUE;
	}
    }
}

/* Play the current level. Return when the user completes the level or
 * requests something else.
 */
static int playgame(gamespec *gs, int firstcmd)
{
    int	cmd, n;

    cmd = firstcmd;
    if (cmd == CmdProceed)
	cmd = CmdNone;

    gs->status = 0;
    setgameplaymode(BeginPlay);
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
	if (!(cmd >= CmdMoveFirst && cmd <= CmdMoveLast)) {
	    switch (cmd) {
	      case CmdPreserve:					break;
	      case CmdPrevLevel:		n = -1;		goto quitloop;
	      case CmdNextLevel:		n = +1;		goto quitloop;
	      case CmdSameLevel:		n = 0;		goto quitloop;
	      case CmdDebugCmd1:				break;
	      case CmdDebugCmd2:				break;
	      case CmdQuit:					exit(0);
	      case CmdVolumeUp:
		changevolume(+2, TRUE);
		cmd = CmdNone;
		break;
	      case CmdVolumeDown:
		changevolume(-2, TRUE);
		cmd = CmdNone;
		break;
	      case CmdPauseGame:
		setgameplaymode(SuspendPlay);
		anykey();
		setgameplaymode(ResumePlay);
		cmd = CmdNone;
		break;
	      case CmdHelp:
		setgameplaymode(SuspendPlay);
		onlinehelp(Help_KeysDuringGame);
		setgameplaymode(ResumePlay);
		cmd = CmdNone;
		break;
	      case CmdCheatNorth:     case CmdCheatWest:	break;
	      case CmdCheatSouth:     case CmdCheatEast:	break;
	      case CmdCheatHome:				break;
	      case CmdCheatKeyRed:    case CmdCheatKeyBlue:	break;
	      case CmdCheatKeyYellow: case CmdCheatKeyGreen:	break;
	      case CmdCheatBootsIce:  case CmdCheatBootsSlide:	break;
	      case CmdCheatBootsFire: case CmdCheatBootsWater:	break;
	      case CmdCheatICChip:				break;
	      default:
		cmd = CmdNone;
		break;
	    }
	}
    }
    setgameplaymode(EndPlay);
    if (n > 0) {
	if (replacesolution())
	    savesolutions(&gs->series);
	if (gs->currentgame + 1 >= gs->series.count)
	    n = 0;
    }
    gs->status = n;
    return TRUE;

  quitloop:
    setgameplaymode(EndPlay);
    if (n)
	changecurrentgame(gs, n);
    return FALSE;
}

/* Play back the user's best solution for the current level.
 */
static int playbackgame(gamespec *gs)
{
    int	n;

    drawscreen();

    gs->status = 0;
    setgameplaymode(BeginPlay);
    for (;;) {
	n = doturn(CmdNone);
	drawscreen();
	if (n)
	    break;
	waitfortick();
	switch (input(FALSE)) {
	  case CmdPrevLevel:	changecurrentgame(gs, -1);	goto quitloop;
	  case CmdNextLevel:	changecurrentgame(gs, +1);	goto quitloop;
	  case CmdSameLevel:					goto quitloop;
	  case CmdPlayback:	gs->playback = FALSE;		goto quitloop;
	  case CmdQuitLevel:	gs->playback = FALSE;		goto quitloop;
	  case CmdQuit:						exit(0);
	  case CmdVolumeUp:
	    changevolume(+2, TRUE);
	    break;
	  case CmdVolumeDown:
	    changevolume(-2, TRUE);
	    break;
	  case CmdPauseGame:
	    setgameplaymode(SuspendPlay);
	    anykey();
	    setgameplaymode(ResumePlay);
	    break;
	  case CmdHelp:
	    setgameplaymode(SuspendPlay);
	    onlinehelp(Help_None);
	    setgameplaymode(ResumePlay);
	    break;
	}
    }
    setgameplaymode(EndPlay);
    gs->playback = FALSE;
    if (n > 0) {
	if (checksolution())
	    savesolutions(&gs->series);
	if (gs->series.games[gs->currentgame].number == gs->series.final)
	    n = 0;
	else if (gs->currentgame + 1 >= gs->series.count)
	    n = 0;
    }
    gs->status = n;
    return TRUE;

  quitloop:
    setgameplaymode(EndPlay);
    gs->playback = FALSE;
    return FALSE;
}

/*
 *
 */

static int selectseriesandlevel(gamespec *gs, seriesdata *series, int autosel,
				char const *defaultseries, int defaultlevel)
{
    int	okay, f, n;

    if (series->count < 1) {
	errmsg(NULL, "no level sets found");
	return FALSE;
    }

    okay = TRUE;
    if (series->count == 1 && autosel) {
	getseriesfromlist(&gs->series, series->list, 0);
    } else {
	n = 0;
	if (defaultseries) {
	    n = series->count;
	    while (n)
		if (!strcmp(series->list[--n].filebase, defaultseries))
		    break;
	}
	for (;;) {
	    f = displaylist(" Welcome to Tile World. Select your destination.",
			    &series->table, &n, scrollinputcallback);
	    if (f == CmdProceed) {
		getseriesfromlist(&gs->series, series->list, n);
		okay = TRUE;
		break;
	    } else if (f == CmdQuitLevel) {
		okay = FALSE;
		break;
	    }
	}
    }
    freeserieslist(series->list, series->count, &series->table);
    if (!okay)
	return FALSE;

    if (!readseriesfile(&gs->series)) {
	errmsg(NULL, "cannot read data file");
	freeseriesdata(&gs->series);
	return FALSE;
    }
    if (gs->series.total < 1) {
	errmsg(NULL, "no levels found in data file");
	freeseriesdata(&gs->series);
	return FALSE;
    }

    gs->playback = FALSE;
    gs->usepasswds = usepasswds && gs->series.usepasswds;
    gs->currentgame = -1;

    if (defaultlevel) {
	n = findlevelinseries(&gs->series, defaultlevel, NULL);
	if (n >= 0) {
	    if (!gs->usepasswds ||
			(gs->series.games[n].sgflags & SGF_HASPASSWD))
		gs->currentgame = n;
	}
    }
    if (gs->currentgame < 0) {
	gs->currentgame = 0;
	for (n = 0 ; n < gs->series.total ; ++n) {
	    if (!hassolution(gs->series.games + n)) {
		gs->currentgame = n;
		break;
	    }
	}
    }

    return TRUE;
}

static int choosegame(gamespec *gs, char const *lastseries)
{
    seriesdata	s;

    if (!createserieslist(NULL, &s.list, &s.count, &s.table))
	return -1;
    return selectseriesandlevel(gs, &s, FALSE, lastseries, 0);
}

/*
 * Initialization functions.
 */

/* Assign values to the different directories that the program uses.
 */
static void initdirs(char const *series, char const *seriesdat,
		     char const *res, char const *save)
{
    unsigned int	maxpath;
    char const	       *root = NULL;
    char const	       *dir;

    maxpath = getpathbufferlen() - 1;
    if (series && strlen(series) >= maxpath) {
	errmsg(NULL, "Data (-D) directory name is too long;"
		     " using default value instead");
	series = NULL;
    }
    if (seriesdat && strlen(seriesdat) >= maxpath) {
	errmsg(NULL, "Configured data (-C) directory name is too long;"
		     " using default value instead");
	seriesdat = NULL;
    }
    if (res && strlen(res) >= maxpath) {
	errmsg(NULL, "Resource (-R) directory name is too long;"
		     " using default value instead");
	res = NULL;
    }
    if (save && strlen(save) >= maxpath) {
	errmsg(NULL, "Save (-S) directory name is too long;"
		     " using default value instead");
	save = NULL;
    }
    if (!save && (dir = getenv("TWORLDSAVEDIR")) && *dir) {
	if (strlen(dir) < maxpath)
	    save = dir;
	else
	    warn("Value of environment variable TWORLDSAVEDIR is too long");
    }

    if (!res || !series) {
	if ((dir = getenv("TWORLDDIR")) && *dir) {
	    if (strlen(dir) < maxpath - 8)
		root = dir;
	    else
		warn("Value of environment variable TWORLDDIR is too long");
	}
	if (!root) {
#ifdef ROOTDIR
	    root = ROOTDIR;
#else
	    root = ".";
#endif
	}
    }

    resdir = getpathbuffer();
    if (res)
	strcpy(resdir, res);
    else
	strcpy(resdir, root);

    seriesdir = getpathbuffer();
    if (series)
	strcpy(seriesdir, series);
    else
	combinepath(seriesdir, root, "sets");

    seriesdatdir = getpathbuffer();
    if (seriesdat)
	strcpy(seriesdatdir, seriesdat);
    else
	combinepath(seriesdatdir, root, "data");

    savedir = getpathbuffer();
    if (!save) {
#ifdef SAVEDIR
	save = SAVEDIR;
#else
	if ((dir = getenv("HOME")) && *dir && strlen(dir) < maxpath - 8)
	    combinepath(savedir, dir, ".tworld");
	else
	    combinepath(savedir, root, "save");

#endif
    } else {
	strcpy(savedir, save);
    }
}

/* Parse the command-line options and arguments, and initialize the
 * user-controlled options.
 */
static int initoptionswithcmdline(int argc, char *argv[], startupdata *start)
{
    cmdlineinfo	opts;
    char const *optresdir = NULL;
    char const *optseriesdir = NULL;
    char const *optseriesdatdir = NULL;
    char const *optsavedir = NULL;
    int		listdirs;
    int		ch, n;

    start->filename = getpathbuffer();
    *start->filename = '\0';
    start->levelnum = 0;
    start->listseries = FALSE;
    start->listscores = FALSE;
    start->listtimes = FALSE;
    listdirs = FALSE;

    initoptions(&opts, argc - 1, argv + 1, "D:L:HR:S:Vdhlpqstv");
    while ((ch = readoption(&opts)) >= 0) {
	switch (ch) {
	  case 0:
	    if (*start->filename && start->levelnum) {
		fprintf(stderr, "too many arguments: %s\n", opts.val);
		printtable(stderr, yowzitch);
		return FALSE;
	    }
	    if (sscanf(opts.val, "%d", &n) == 1)
		start->levelnum = n;
	    else
		strncpy(start->filename, opts.val, getpathbufferlen() - 1);
	    break;
	  case 'D':	optseriesdatdir = opts.val;			break;
	  case 'L':	optseriesdir = opts.val;			break;
	  case 'R':	optresdir = opts.val;				break;
	  case 'S':	optsavedir = opts.val;				break;
	  case 'H':	showhistogram = !showhistogram;			break;
	  case 'p':	usepasswds = !usepasswds;			break;
	  case 'q':	silence = !silence;				break;
	  case 'd':	listdirs = TRUE;				break;
	  case 'l':	start->listseries = TRUE;			break;
	  case 's':	start->listscores = TRUE;			break;
	  case 't':	start->listtimes = TRUE;			break;
	  case 'h':	printtable(stdout, yowzitch); 	   exit(EXIT_SUCCESS);
	  case 'v':	puts(VERSION);		 	   exit(EXIT_SUCCESS);
	  case 'V':	printtable(stdout, vourzhon); 	   exit(EXIT_SUCCESS);
	  case ':':
	    fprintf(stderr, "option requires an argument: -%c\n", opts.opt);
	    printtable(stderr, yowzitch);
	    return FALSE;
	  case '?':
	    fprintf(stderr, "unrecognized option: -%c\n", opts.opt);
	    printtable(stderr, yowzitch);
	    return FALSE;
	  default:
	    printtable(stderr, yowzitch);
	    return FALSE;
	}
    }

    if (start->listscores || start->listtimes || start->levelnum)
	if (!*start->filename)
	    strcpy(start->filename, "chips.dat");
    start->filename[getpathbufferlen() - 1] = '\0';

    initdirs(optseriesdir, optseriesdatdir, optresdir, optsavedir);
    if (listdirs) {
	printdirectories();
	exit(EXIT_SUCCESS);
    }

    return TRUE;
}

/* Initialize the other modules.
 */
static int initializesystem(void)
{
    if (!oshwinitialize(silence, showhistogram))
	return FALSE;
    if (!initresources())
	return FALSE;
    setsubtitle(NULL);
    setkeyboardrepeat(TRUE);
    return TRUE;
}

static void shutdownsystem(void)
{
    setsubtitle(NULL);
    shutdowngamestate();
    freeallresources();
    free(resdir);
    free(seriesdir);
    free(seriesdatdir);
    free(savedir);
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
static void choosegameatstartup(gamespec *gs, startupdata const *start)
{
    seriesdata	series;
    tablespec	table;
    int		f;

    if (!createserieslist(start->filename,
			  &series.list, &series.count, &series.table))
	die("unable to create list of available level sets");
    free(start->filename);

    if (series.count <= 0)
	die("no level sets found");

    if (start->listseries) {
	printtable(stdout, &series.table);
	if (!series.count)
	    puts("(no files)");
	exit(EXIT_SUCCESS);
    }

    if (series.count == 1) {
	if (!readseriesfile(series.list))
	    die("cannot read level set");
	if (start->listscores) {
	    if (!createscorelist(series.list, start->usepasswds,
				 NULL, NULL, &table))
		exit(EXIT_FAILURE);
	    freeserieslist(series.list, series.count, &series.table);
	    printtable(stdout, &table);
	    freescorelist(NULL, &table);
	    exit(EXIT_SUCCESS);
	}
	if (start->listtimes) {
	    if (!createtimelist(series.list,
				series.list->ruleset == Ruleset_Lynx,
				NULL, NULL, &table))
		exit(EXIT_FAILURE);
	    freeserieslist(series.list, series.count, &series.table);
	    printtable(stdout, &table);
	    freetimelist(NULL, &table);
	    exit(EXIT_SUCCESS);
	}
    }

    if (!initializesystem())
	die("cannot initialize program due to previous errors");

    f = selectseriesandlevel(gs, &series, TRUE, NULL, start->levelnum);
    if (f < 0)
	die("cannot proceed due to previous errors");
    else if (f == 0)
	exit(EXIT_SUCCESS);
}

/*
 * main().
 */

static int runcurrentlevel(gamespec *gs)
{
    int ret = TRUE;
    int	cmd;
    int	valid, f;

    valid = initgamestate(gs->series.games + gs->currentgame,
			  gs->series.ruleset);
    setsubtitle(gs->series.games[gs->currentgame].name);

    cmd = startinput(gs);
    if (cmd == CmdQuitLevel) {
	ret = FALSE;
    } else {
	if (cmd != CmdNone) {
	    if (!valid) {
		bell();
	    } else {
		f = gs->playback ? playbackgame(gs)
				 : playgame(gs, cmd);
		if (f)
		    ret = endinput(gs);
	    }
	}
    }

    endgamestate();
    return ret;
}

/* Initialize the system and enter an infinite loop of displaying and
 * playing levels.
 */
int main(int argc, char *argv[])
{
    startupdata	start;
    gamespec	spec;
    char	lastseries[sizeof spec.series.filebase];
    int		f;

    if (!initoptionswithcmdline(argc, argv, &start))
	return EXIT_FAILURE;

    choosegameatstartup(&spec, &start);

    for (;;) {
	while (runcurrentlevel(&spec)) ;
	cleardisplay();
	setsubtitle(NULL);
	strcpy(lastseries, spec.series.filebase);
	freeseriesdata(&spec.series);
	f = choosegame(&spec, lastseries);
	if (f <= 0)
	    break;
    }

    shutdownsystem();
    return f == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
