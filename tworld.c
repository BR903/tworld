/* tworld.c: The top-level module.
 *
 * Copyright (C) 2001-2004 by Brian Raiter, under the GNU General Public
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

/* The data needed to identify what level is being played.
 */
typedef	struct gamespec {
    gameseries	series;		/* the complete set of levels */
    int		currentgame;	/* which level is currently selected */
    int		playback;	/* TRUE if in playback mode */
    int		usepasswds;	/* FALSE if passwords are to be ignored */
    int		status;		/* final status of last game played */
    int		enddisplay;	/* TRUE if the final level was completed */
    int		melindacount;	/* count for Melinda's free pass */
} gamespec;

/* Structure used to hold data collected by initoptionswithcmdline().
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

/* Structure used to hold the complete list of available series.
 */
typedef	struct seriesdata {
    gameseries *list;		/* the array of available series */
    int		count;		/* size of arary */
    tablespec	table;		/* table for displaying the array */
} seriesdata;

/* TRUE suppresses sound and the console bell.
 */
static int	silence = FALSE;

/* TRUE means the program should attempt to run in fullscreen mode.
 */
static int	fullscreen = FALSE;

/* FALSE suppresses all password checking.
 */
static int	usepasswds = TRUE;

/* TRUE if the user requested an idle-time histogram.
 */
static int	showhistogram = FALSE;

/* Slowdown factor, used for debugging.
 */
static int	mudsucking = 1;

/* Frame-skipping disable flag.
 */
static int	noframeskip = FALSE;

/* The sound buffer scaling factor.
 */
static int	soundbufsize = -1;

/* The initial volume level.
 */
static int	volumelevel = -1;

/* The top of the stack of subtitles.
 */
static void   **subtitlestack = NULL;

/*
 * Text-mode output functions.
 */

/* Find a position to break a string inbetween words. The integer at
 * breakpos receives the length of the string prefix less than or
 * equal to len. The string pointer *str is advanced to the first
 * non-whitespace after the break. The original string pointer is
 * returned.
 */
static char *findstrbreak(char const **str, int maxlen, int *breakpos)
{
    char const *start;
    int		n;

  retry:
    start = *str;
    n = strlen(start);
    if (n <= maxlen) {
	*str += n;
	*breakpos = n;
    } else {
	n = maxlen;
	if (isspace(start[n])) {
	    *str += n;
	    while (isspace(**str))
		++*str;
	    while (n > 0 && isspace(start[n - 1]))
		--n;
	    if (n == 0)
		goto retry;
	    *breakpos = n;
	} else {
	    while (n > 0 && !isspace(start[n - 1]))
		--n;
	    if (n == 0) {
		*str += maxlen;
		*breakpos = maxlen;
	    } else {
		*str = start + n;
		while (n > 0 && isspace(start[n - 1]))
		    --n;
		if (n == 0)
		    goto retry;
		*breakpos = n;
	    }
	}
    }
    return (char*)start;
}

/* Render a table to the given file.
 */
void printtable(FILE *out, tablespec const *table)
{
    char const *mlstr;
    char const *p;
    int	       *colsizes;
    int		len, pos;
    int		mlpos, mllen;
    int		c, n, x, y, z;

    colsizes = malloc(table->cols * sizeof *colsizes);
    for (x = 0 ; x < table->cols ; ++x)
	colsizes[x] = 0;
    mlpos = -1;
    n = 0;
    for (y = 0 ; y < table->rows ; ++y) {
	for (x = 0 ; x < table->cols ; ++n) {
	    c = table->items[n][0] - '0';
	    if (table->items[n][1] == '!') {
		mlpos = x;
	    } else if (c == 1) {
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
    } else if (mlpos >= 0) {
	colsizes[mlpos] += 79 - n;
    }

    n = 0;
    for (y = 0 ; y < table->rows ; ++y) {
	mlstr = NULL;
	mllen = 0;
	pos = 0;
	for (x = 0 ; x < table->cols ; ++n) {
	    if (x)
		pos += fprintf(out, "%*s", table->sep, "");
	    c = table->items[n][0] - '0';
	    len = -table->sep;
	    while (c--)
		len += colsizes[x++] + table->sep;
	    if (table->items[n][1] == '-')
		fprintf(out, "%-*.*s", len, len, table->items[n] + 2);
	    else if (table->items[n][1] == '+')
		fprintf(out, "%*.*s", len, len, table->items[n] + 2);
	    else if (table->items[n][1] == '.') {
		z = (len - strlen(table->items[n] + 2)) / 2;
		if (z < 0)
		    z = len;
		fprintf(out, "%*.*s%*s",
			     len - z, len - z, table->items[n] + 2, z, "");
	    } else if (table->items[n][1] == '!') {
		mllen = len;
		mlpos = pos;
		mlstr = table->items[n] + 2;
		p = findstrbreak(&mlstr, len, &z);
		fprintf(out, "%.*s%*s", z, p, len - z, "");
	    }
	    pos += len;
	}
	fputc('\n', out);
	while (mlstr && *mlstr) {
	    p = findstrbreak(&mlstr, mllen, &len);
	    fprintf(out, "%*s%.*s\n", mlpos, "", len, p);
	}
    }
    free(colsizes);
}

/* Display directory settings.
 */
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

/* An input callback that only accepts the characters Y and N.
 */
static int yninputcallback(void)
{
    int	ch;

    ch = input(TRUE);
    if (ch == CmdWest)
	return '\b';
    else if (ch == CmdProceed)
	return '\n';
    else if (ch == CmdQuitLevel)
	return -1;
    else if (ch == CmdQuit)
	exit(0);
    else if (isalpha(ch)) {
	ch = toupper(ch);
	if (ch == 'Y' || ch == 'N')
	    return ch;
    }
    return 0;
}

/* An input callback that accepts only alphabetic characters.
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

/* An input callback used while displaying a scrolling list.
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
 * Basic game activities.
 */

/* Return TRUE if the given level is a final level.
 */
static int islastinseries(gamespec *gs, int index)
{
    return index == gs->series.total - 1
	|| gs->series.games[index].number == gs->series.final;
}

/* Mark the current level's solution as replaceable.
 */
static void replaceablesolution(gamespec *gs, int change)
{
    if (change < 0)
	gs->series.games[gs->currentgame].sgflags ^= SGF_REPLACEABLE;
    else if (change > 0)
	gs->series.games[gs->currentgame].sgflags |= SGF_REPLACEABLE;
    else
	gs->series.games[gs->currentgame].sgflags &= ~SGF_REPLACEABLE;
}

/* Mark the current level's password as known to the user.
 */
static void passwordseen(gamespec *gs, int number)
{
    if (!(gs->series.games[number].sgflags & SGF_HASPASSWD)) {
	gs->series.games[number].sgflags |= SGF_HASPASSWD;
	savesolutions(&gs->series);
    }
}

/* Change the current level, ensuring that the user is not granted
 * access to a forbidden level. FALSE is returned if the specified
 * level is not available to the user.
 */
static int setcurrentgame(gamespec *gs, int n)
{
    if (n == gs->currentgame)
	return TRUE;
    if (n < 0 || n >= gs->series.total)
	return FALSE;

    if (gs->usepasswds)
	if (n > 0 && !(gs->series.games[n].sgflags & SGF_HASPASSWD)
		  && !hassolution(gs->series.games + n - 1))
	    return FALSE;

    gs->currentgame = n;
    gs->melindacount = 0;
    return TRUE;
}

/* Change the current level by a delta value. If the user cannot go to
 * that level, the "nearest" level in that direction is chosen
 * instead. FALSE is returned if the current level remained unchanged.
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

    if (gs->usepasswds && n > 0) {
	sign = offset < 0 ? -1 : +1;
	for ( ; n >= 0 && n < gs->series.total ; n += sign) {
	    if (!n || (gs->series.games[n].sgflags & SGF_HASPASSWD)
		   || hassolution(gs->series.games + n - 1)) {
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
		if (!n || (gs->series.games[n].sgflags & SGF_HASPASSWD)
		       || hassolution(gs->series.games + n - 1))
		    break;
	    }
	}
    }

    if (n == gs->currentgame)
	return FALSE;

    gs->currentgame = n;
    gs->melindacount = 0;
    return TRUE;
}

/* Return TRUE if Melinda is watching Chip's progress on this level --
 * i.e., if it is possible to earn a pass to the next level.
 */
static int melindawatching(gamespec *gs)
{
    if (!gs->usepasswds)
	return FALSE;
    if (islastinseries(gs, gs->currentgame))
	return FALSE;
    if (gs->series.games[gs->currentgame + 1].sgflags & SGF_HASPASSWD)
	return FALSE;
    if (hassolution(gs->series.games + gs->currentgame))
	return FALSE;
    return TRUE;
}

/*
 * The subtitle stack
 */

static void pushsubtitle(char const *subtitle)
{
    void      **stk;
    int		n;

    if (!subtitle)
	subtitle = "";
    n = strlen(subtitle) + 1;
    stk = NULL;
    xalloc(stk, sizeof(void**) + n);
    *stk = subtitlestack;
    subtitlestack = stk;
    memcpy(stk + 1, subtitle, n);
    setsubtitle(subtitle);
}

static void popsubtitle(void)
{
    void      **stk;

    if (subtitlestack) {
	stk = *subtitlestack;
	free(subtitlestack);
	subtitlestack = stk;
    }
    setsubtitle(subtitlestack ? (char*)(subtitlestack + 1) : NULL);
}

static void changesubtitle(char const *subtitle)
{
    int		n;

    if (!subtitle)
	subtitle = "";
    n = strlen(subtitle) + 1;
    xalloc(subtitlestack, sizeof(void**) + n);
    memcpy(subtitlestack + 1, subtitle, n);
    setsubtitle(subtitle);
}

/*
 *
 */

static void dohelp(int topic)
{
    pushsubtitle("Help");
    onlinemainhelp(topic);
    popsubtitle();
}

/* Display the scrolling list of the user's current scores, and allow
 * the user to select a current level.
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
    for (n = 0 ; n < count ; ++n)
	if (levellist[n] == gs->currentgame)
	    break;
    pushsubtitle(gs->series.name);
    for (;;) {
	f = displaylist(gs->series.filebase, &table, &n, scrollinputcallback);
	if (f == CmdProceed) {
	    n = levellist[n];
	    break;
	} else if (f == CmdQuitLevel) {
	    n = -1;
	    break;
	} else if (f == CmdHelp)
	    dohelp(Help_None);
    }
    popsubtitle();
    freescorelist(levellist, &table);
    if (n < 0)
	return FALSE;
    return setcurrentgame(gs, n);
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
    passwordseen(gs, n);
    return setcurrentgame(gs, n);
}

static void changestepping(int delta, int display)
{
    (void)delta;
    if (display)
	setdisplaymsg("stepping changed", 1000, 1000);
}

/*
 * The game-playing functions.
 */

#define	leveldelta(n)	if (!changecurrentgame(gs, (n))) { bell(); continue; }

/* Get a key command from the user at the start of the current level.
 */
static int startinput(gamespec *gs)
{
    int	cmd;

    drawscreen(TRUE);
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
	  case CmdKillSolution:	replaceablesolution(gs, -1);	break;
	  case CmdSteppingUp:	changestepping(+1, TRUE);	break;
	  case CmdSteppingDown:	changestepping(-1, TRUE);	break;
	  case CmdVolumeUp:	changevolume(+2, TRUE);		break;
	  case CmdVolumeDown:	changevolume(-2, TRUE);		break;
	  case CmdHelp:		dohelp(Help_KeysBetweenGames); break;
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
	drawscreen(TRUE);
    }
}

/* Get a key command from the user at the completion of the current
 * level.
 */
static int endinput(gamespec *gs)
{
    char	yn[2] = "";
    int		bscore = 0, tscore = 0;
    long	gscore = 0;
    int		n;

    if (gs->status < 0) {
	if (melindawatching(gs) && secondsplayed() >= 10) {
	    ++gs->melindacount;
	    if (gs->melindacount >= 10) {
		setgameplaymode(BeginInput);
		n = displayinputprompt("Skip level?", yn, 1, yninputcallback);
		setgameplaymode(EndInput);
		if (n && *yn == 'Y') {
		    passwordseen(gs, gs->currentgame + 1);
		    changecurrentgame(gs, +1);
		}
		gs->melindacount = 0;
		return TRUE;
	    }
	}
    } else {
	getscoresforlevel(&gs->series, gs->currentgame,
			  &bscore, &tscore, &gscore);
    }

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
	  case CmdKillSolution:	replaceablesolution(gs, -1);	return TRUE;
	  case CmdHelp:		dohelp(Help_KeysBetweenGames); return TRUE;
	  case CmdQuitLevel:					return FALSE;
	  case CmdQuit:						exit(0);
	  case CmdProceed:
	    if (gs->status > 0) {
		if (islastinseries(gs, gs->currentgame))
		    gs->enddisplay = TRUE;
		else
		    changecurrentgame(gs, +1);
	    }
	    return TRUE;
	}
    }
}

/* Get a key command from the user at the completion of the current
 * series.
 */
static int finalinput(gamespec *gs)
{
    int	cmd;

    for (;;) {
	cmd = input(TRUE);
	switch (cmd) {
	  case CmdSameLevel:
	  case CmdSame:
	    return TRUE;
	  case CmdPrevLevel:
	  case CmdPrev:
	  case CmdNextLevel:
	  case CmdNext:
	    setcurrentgame(gs, 0);
	    return TRUE;
	  case CmdQuit:
	    exit(0);
	  default:
	    return FALSE;
	}
    }
}

/* Play the current level, using firstcmd as the initial key command,
 * and returning when the level's play ends. The return value is FALSE
 * if play ended because the user restarted or changed the current
 * level (indicating that the program should not prompt the user
 * before continuing). If the return value is TRUE, the gamespec
 * structure's status field will contain the return value of the last
 * call to doturn() -- i.e., positive if the level was completed
 * successfully, negative if the level ended unsuccessfully. Likewise,
 * the gamespec structure will be updated if the user ended play by
 * changing the current level.
 */
static int playgame(gamespec *gs, int firstcmd)
{
    int	render, lastrendered;
    int	cmd, n;

    cmd = firstcmd;
    if (cmd == CmdProceed)
	cmd = CmdNone;

    gs->status = 0;
    setgameplaymode(BeginPlay);
    render = lastrendered = TRUE;
    for (;;) {
	n = doturn(cmd);
	drawscreen(render);
	lastrendered = render;
	if (n)
	    break;
	render = waitfortick() || noframeskip;
	cmd = input(FALSE);
	if (cmd == CmdQuitLevel) {
	    quitgamestate();
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
		setdisplaymsg("(paused)", 1, 1);
		for (;;) {
		    switch (input(TRUE)) {
		      case CmdQuit:		exit(0);
		      case CmdPauseGame:	break;
		      default:			continue;
		    }
		    break;
		}
		setgameplaymode(ResumePlay);
		cmd = CmdNone;
		break;
	      case CmdHelp:
		setgameplaymode(SuspendPlay);
		dohelp(Help_KeysDuringGame);
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
    if (!lastrendered)
	drawscreen(TRUE);
    setgameplaymode(EndPlay);
    if (n > 0)
	if (replacesolution())
	    savesolutions(&gs->series);
    gs->status = n;
    return TRUE;

  quitloop:
    if (!lastrendered)
	drawscreen(TRUE);
    quitgamestate();
    setgameplaymode(EndPlay);
    if (n)
	changecurrentgame(gs, n);
    return FALSE;
}

/* Play back the user's best solution for the current level. Other than
 * the fact that this function runs from a prerecorded series of moves,
 * it has the same behavior as playgame().
 */
static int playbackgame(gamespec *gs)
{
    int	render, lastrendered, n;

    drawscreen(TRUE);

    gs->status = 0;
    setgameplaymode(BeginPlay);
    render = lastrendered = TRUE;
    for (;;) {
	n = doturn(CmdNone);
	drawscreen(render);
	lastrendered = render;
	if (n)
	    break;
	render = waitfortick() || noframeskip;
	switch (input(FALSE)) {
	  case CmdPrevLevel:	changecurrentgame(gs, -1);	goto quitloop;
	  case CmdNextLevel:	changecurrentgame(gs, +1);	goto quitloop;
	  case CmdSameLevel:					goto quitloop;
	  case CmdPlayback:					goto quitloop;
	  case CmdQuitLevel:					goto quitloop;
	  case CmdQuit:						exit(0);
	  case CmdVolumeUp:
	    changevolume(+2, TRUE);
	    break;
	  case CmdVolumeDown:
	    changevolume(-2, TRUE);
	    break;
	  case CmdPauseGame:
	    setgameplaymode(SuspendPlay);
	    setdisplaymsg("(paused)", 1, 1);
	    for (;;) {
		switch (input(TRUE)) {
		  case CmdQuit:		exit(0);
		  case CmdPauseGame:	break;
		  default:		continue;
		}
		break;
	    }
	    setgameplaymode(ResumePlay);
	    break;
	  case CmdHelp:
	    setgameplaymode(SuspendPlay);
	    dohelp(Help_None);
	    setgameplaymode(ResumePlay);
	    break;
	}
    }
    if (!lastrendered)
	drawscreen(TRUE);
    setgameplaymode(EndPlay);
    gs->playback = FALSE;
    if (n < 0)
	replaceablesolution(gs, +1);
    if (n > 0) {
	if (checksolution())
	    savesolutions(&gs->series);
	if (islastinseries(gs, gs->currentgame))
	    n = 0;
    }
    gs->status = n;
    return TRUE;

  quitloop:
    if (!lastrendered)
	drawscreen(TRUE);
    quitgamestate();
    setgameplaymode(EndPlay);
    gs->playback = FALSE;
    return FALSE;
}

/* Manage a single session of playing the current level, from start to
 * finish. A return value of FALSE indicates that the user is done
 * playing levels from the current series; otherwise, the gamespec
 * structure is updated as necessary upon return.
 */
static int runcurrentlevel(gamespec *gs)
{
    int ret = TRUE;
    int	cmd;
    int	valid, f;

    if (gs->enddisplay) {
	gs->enddisplay = FALSE;
	initgamestate(enddisplaylevel(), gs->series.ruleset);
	changesubtitle(NULL);
	drawscreen(TRUE);
	displayendmessage(0, 0, 0, 0);
	endgamestate();
	return finalinput(gs);
    }

    valid = initgamestate(gs->series.games + gs->currentgame,
			  gs->series.ruleset);
    changesubtitle(gs->series.games[gs->currentgame].name);
    passwordseen(gs, gs->currentgame);
    if (!valid && !islastinseries(gs, gs->currentgame))
	passwordseen(gs, gs->currentgame + 1);

    cmd = startinput(gs);
    if (cmd == CmdQuitLevel) {
	ret = FALSE;
    } else {
	if (cmd != CmdNone) {
	    if (valid) {
		f = gs->playback ? playbackgame(gs) : playgame(gs, cmd);
		if (f)
		    ret = endinput(gs);
	    } else
		bell();
	}
    }

    endgamestate();
    return ret;
}

/*
 * Game selection functions
 */

/* Display the full selection of available series to the user as a
 * scrolling list, and permit one to be selected. When one is chosen,
 * pick one of levels to be the current level. All fields of the
 * gamespec structure are initiailzed. If autosel is TRUE, then the
 * function will skip the display if there is only one series
 * available. If defaultseries is not NULL, and matches the name of
 * one of the series in the array, then the scrolling list will be
 * initialized with that series selected. If defaultlevel is not zero,
 * and a level in the selected series that the user is permitted to
 * access matches it, then that level will be thhe initial current
 * level. The return value is zero if nothing was selected, negative
 * if an error occurred, or positive otherwise.
 */
static int selectseriesandlevel(gamespec *gs, seriesdata *series, int autosel,
				char const *defaultseries, int defaultlevel)
{
    int	okay, f, n;

    if (series->count < 1) {
	errmsg(NULL, "no level sets found");
	return -1;
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
	    f = displaylist("    Welcome to Tile World. Type ? for help.",
			    &series->table, &n, scrollinputcallback);
	    if (f == CmdProceed) {
		getseriesfromlist(&gs->series, series->list, n);
		okay = TRUE;
		break;
	    } else if (f == CmdQuitLevel) {
		okay = FALSE;
		break;
	    } else if (f == CmdHelp) {
		pushsubtitle("Help");
		onlinefirsthelp();
		popsubtitle();
	    }
	}
    }
    freeserieslist(series->list, series->count, &series->table);
    if (!okay)
	return 0;

    if (!readseriesfile(&gs->series)) {
	errmsg(gs->series.filebase, "cannot read data file");
	freeseriesdata(&gs->series);
	return -1;
    }
    if (gs->series.total < 1) {
	errmsg(gs->series.filebase, "no levels found in data file");
	freeseriesdata(&gs->series);
	return -1;
    }

    gs->enddisplay = FALSE;
    gs->playback = FALSE;
    gs->usepasswds = usepasswds && !(gs->series.gsflags & GSF_IGNOREPASSWDS);
    gs->currentgame = -1;
    gs->melindacount = 0;

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

    return +1;
}

/* Get the list of available series and permit the user to select one
 * to play. If lastseries is not NULL, use that series as the default.
 * The return value is zero if nothing was selected, negative if an
 * error occurred, or positive otherwise.
 */
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

/* Set the four directories that the program uses (the series
 * directory, the series data directory, the resource directory, and
 * the save directory).  Any or all of the arguments can be NULL,
 * indicating that the default value should be used. The environment
 * variables TWORLDDIR, TWORLDSAVEDIR, and HOME can define the default
 * values. If any or all of these are unset, the program will use the
 * default values it was compiled with.
 */
static void initdirs(char const *series, char const *seriesdat,
		     char const *res, char const *save)
{
    unsigned int	maxpath;
    char const	       *root = NULL;
    char const	       *dir;

    maxpath = getpathbufferlen();
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

    if (!res || !series || !seriesdat) {
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
	combinepath(resdir, root, "res");

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
    mudsucking = 1;
    soundbufsize = 0;
    volumelevel = -1;

    initoptions(&opts, argc - 1, argv + 1, "aD:dFfHhL:lm:n:pqR:S:stVv");
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
		strncpy(start->filename, opts.val, getpathbufferlen());
	    break;
	  case 'D':	optseriesdatdir = opts.val;			break;
	  case 'L':	optseriesdir = opts.val;			break;
	  case 'R':	optresdir = opts.val;				break;
	  case 'S':	optsavedir = opts.val;				break;
	  case 'H':	showhistogram = !showhistogram;			break;
	  case 'f':	noframeskip = !noframeskip;			break;
	  case 'F':	fullscreen = !fullscreen;			break;
	  case 'p':	usepasswds = !usepasswds;			break;
	  case 'q':	silence = !silence;				break;
	  case 'a':	++soundbufsize;					break;
	  case 'd':	listdirs = TRUE;				break;
	  case 'l':	start->listseries = TRUE;			break;
	  case 's':	start->listscores = TRUE;			break;
	  case 't':	start->listtimes = TRUE;			break;
	  case 'm':	mudsucking = atoi(opts.val);			break;
	  case 'n':	volumelevel = atoi(opts.val);			break;
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
    start->filename[getpathbufferlen()] = '\0';

    initdirs(optseriesdir, optseriesdatdir, optresdir, optsavedir);
    if (listdirs) {
	printdirectories();
	exit(EXIT_SUCCESS);
    }

    return TRUE;
}

/* Run the initialization routines of oshw and the resource module.
 */
static int initializesystem(void)
{
#ifdef NDEBUG
    mudsucking = 1;
#endif
    setmudsuckingfactor(mudsucking);
    if (!oshwinitialize(silence, soundbufsize, showhistogram, fullscreen))
	return FALSE;
    if (!initresources())
	return FALSE;
    setkeyboardrepeat(TRUE);
    if (volumelevel >= 0)
	setvolume(volumelevel, FALSE);
    return TRUE;
}

/* Time for everyone to clean up and go home.
 */
static void shutdownsystem(void)
{
    shutdowngamestate();
    freeallresources();
    free(resdir);
    free(seriesdir);
    free(seriesdatdir);
    free(savedir);
}

/* Determine what to play. A list of available series is drawn up; if
 * only one is found, it is selected automatically. Otherwise, if the
 * listseries option is TRUE, the available series are displayed on
 * stdout and the program exits. Otherwise, if listscores or listtimes
 * is TRUE, the scores or times for a single series is display on
 * stdout and the program exits. (These options need to be checked for
 * before initializing the graphics subsystem.) Otherwise, the
 * selectseriesandlevel() function handles the rest of the work. Note
 * that this function is only called during the initial startup; if
 * the user returns to the series list later on, the choosegame()
 * function is called instead.
 */
static int choosegameatstartup(gamespec *gs, startupdata const *start)
{
    seriesdata	series;
    tablespec	table;

    if (!createserieslist(start->filename,
			  &series.list, &series.count, &series.table))
	return -1;

    free(start->filename);

    if (series.count <= 0) {
	errmsg(NULL, "no level sets found");
	return -1;
    }

    if (start->listseries) {
	printtable(stdout, &series.table);
	if (!series.count)
	    puts("(no files)");
	return 0;
    }

    if (series.count == 1) {
	if (!readseriesfile(series.list)) {
	    errmsg(series.list[0].filebase, "cannot read level set");
	    return -1;
	}
	if (start->listscores) {
	    if (!createscorelist(series.list, start->usepasswds,
				 NULL, NULL, &table))
		return -1;
	    freeserieslist(series.list, series.count, &series.table);
	    printtable(stdout, &table);
	    freescorelist(NULL, &table);
	    return 0;
	}
	if (start->listtimes) {
	    if (!createtimelist(series.list,
				series.list->ruleset == Ruleset_Lynx ? 2 : 1,
				NULL, NULL, &table))
		return -1;
	    freeserieslist(series.list, series.count, &series.table);
	    printtable(stdout, &table);
	    freetimelist(NULL, &table);
	    return 0;
	}
    }

    if (!initializesystem()) {
	errmsg(NULL, "cannot initialize program due to previous errors");
	return -1;
    }

    return selectseriesandlevel(gs, &series, TRUE, NULL, start->levelnum);
}

/*
 * main().
 */

int main(int argc, char *argv[])
{
    startupdata	start;
    gamespec	spec;
    char	lastseries[sizeof spec.series.filebase];
    int		f;

    if (!initoptionswithcmdline(argc, argv, &start))
	return EXIT_FAILURE;

    f = choosegameatstartup(&spec, &start);
    if (f < 0)
	return EXIT_FAILURE;
    else if (f == 0)
	return EXIT_SUCCESS;

    do {
	pushsubtitle(NULL);
	while (runcurrentlevel(&spec)) ;
	popsubtitle();
	cleardisplay();
	strcpy(lastseries, spec.series.filebase);
	freeseriesdata(&spec.series);
	f = choosegame(&spec, lastseries);
    } while (f > 0);

    shutdownsystem();
    return f == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
