/* tworld.c: The top-level module.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
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
#include	"messages.h"
#include	"help.h"
#include	"oshw.h"
#include	"cmdline.h"
#include	"ver.h"

/* Bell-ringing macro.
 */
#define	bell()	(silence ? (void)0 : ding())

enum { Play_None, Play_Normal, Play_Back, Play_Verify };

/* The data needed to identify what level is being played.
 */
typedef	struct gamespec {
    gameseries	series;		/* the complete set of levels */
    int		currentgame;	/* which level is currently selected */
    int		playmode;	/* which mode to play */
    int		usepasswds;	/* FALSE if passwords are to be ignored */
    int		status;		/* final status of last game played */
    int		enddisplay;	/* TRUE if the final level was completed */
    int		melindacount;	/* count for Melinda's free pass */
} gamespec;

/* Structure used to hold the complete list of available series.
 */
typedef	struct seriesdata {
    gameseries *list;		/* the array of available series */
    int		count;		/* size of arary */
    tablespec	table;		/* table for displaying the array */
} seriesdata;

/* Structure used to hold data collected by initoptionswithcmdline().
 */
typedef	struct startupdata {
    char	       *filename;	/* which data file to use */
    char	       *selectfilename;	/* which data file to select */
    char	       *savefilename;	/* an alternate solution file */
    int			levelnum;	/* a selected initial level */ 
    char const	       *resdir;		/* where the resources are */
    char const	       *seriesdir;	/* where the series files are */
    char const	       *seriesdatdir;	/* where the series data files are */
    char const	       *savedir;	/* where the solution files are */
    int			volumelevel;	/* the initial volume level */
    int			soundbufsize;	/* the sound buffer scaling factor */
    int			mudsucking;	/* slowdown factor (for debugging) */
    unsigned char	listdirs;	/* TRUE to list directories */
    unsigned char	listseries;	/* TRUE to list files */
    unsigned char	listscores;	/* TRUE to list scores */
    unsigned char	listtimes;	/* TRUE to list times */
    unsigned char	batchverify;	/* TRUE to do batch verification */
    unsigned char	showhistogram;	/* TRUE to display idle histogram */
    unsigned char	pedantic;	/* TRUE to set pedantic mode */
    unsigned char	fullscreen;	/* TRUE to run in full-screen mode */
    unsigned char	readonly;	/* TRUE to suppress all file writes */
} startupdata;

/* Name of the user's initialization file.
 */
static char const      *initfilename = "init";

/* TRUE suppresses sound and the console bell.
 */
static int		silence = FALSE;

/* FALSE suppresses all password checking.
 */
static int		usepasswds = TRUE;

/* The top of the stack of subtitles.
 */
static void	      **subtitlestack = NULL;

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

/* Render a table to the given file. This function encapsulates both
 * the process of determining the necessary widths for each column of
 * the table, and then sequentially rendering the table's contents to
 * a stream. On the first pass through the data, single-cell
 * non-word-wrapped entries are measured and each column sized to fit.
 * If the resulting table is too large for the given area, then the
 * collapsible column is reduced as necessary. If there is still
 * space, however, then the entries that span multiple cells are
 * measured in a second pass, and columns are grown to fit them as
 * well where needed. If there is still space after this, the column
 * containing word-wrapped entries may be expanded as well.
 */
void printtable(FILE *out, tablespec const *table)
{
    int const	maxwidth = 79;
    char const *mlstr;
    char const *p;
    int	       *colsizes;
    int		mlindex, mlwidth, mlpos;
    int		diff, pos;
    int		i, j, n, i0, c, w, z;

    if (!(colsizes = malloc(table->cols * sizeof *colsizes)))
	return;
    for (i = 0 ; i < table->cols ; ++i)
	colsizes[i] = 0;
    mlindex = -1;
    mlwidth = 0;
    n = 0;
    for (j = 0 ; j < table->rows ; ++j) {
	for (i = 0 ; i < table->cols ; ++n) {
	    c = table->items[n][0] - '0';
	    if (c == 1) {
		w = strlen(table->items[n] + 2);
		if (table->items[n][1] == '!') {
		    if (w > mlwidth || mlindex != i)
			mlwidth = w;
		    mlindex = i;
		} else {
		    if (w > colsizes[i])
			colsizes[i] = w;
		}
	    }
	    i += c;
	}
    }

    w = -table->sep;
    for (i = 0 ; i < table->cols ; ++i)
	w += colsizes[i] + table->sep;
    diff = maxwidth - w;

    if (diff < 0 && table->collapse >= 0) {
	w = -diff;
	if (colsizes[table->collapse] < w)
	    w = colsizes[table->collapse] - 1;
	colsizes[table->collapse] -= w;
	diff += w;
    }

    if (diff > 0) {
	n = 0;
	for (j = 0 ; j < table->rows && diff > 0 ; ++j) {
	    for (i = 0 ; i < table->cols ; ++n) {
		c = table->items[n][0] - '0';
		if (c > 1 && table->items[n][1] != '!') {
		    w = table->sep + strlen(table->items[n] + 2);
		    for (i0 = i ; i0 < i + c ; ++i0)
			w -= colsizes[i0] + table->sep;
		    if (w > 0) {
			if (table->collapse >= i && table->collapse < i + c)
			    i0 = table->collapse;
			else if (mlindex >= i && mlindex < i + c)
			    i0 = mlindex;
			else
			    i0 = i + c - 1;
			if (w > diff)
			    w = diff;
			colsizes[i0] += w;
			diff -= w;
			if (diff == 0)
			    break;
		    }
		}
		i += c;
	    }
	}
    }
    if (diff > 0 && mlindex >= 0 && colsizes[mlindex] < mlwidth) {
	mlwidth -= colsizes[mlindex];
	w = mlwidth < diff ? mlwidth : diff;
	colsizes[mlindex] += w;
	diff -= w;
    }

    n = 0;
    for (j = 0 ; j < table->rows ; ++j) {
	mlstr = NULL;
	mlwidth = mlpos = 0;
	pos = 0;
	for (i = 0 ; i < table->cols ; ++n) {
	    if (i)
		pos += fprintf(out, "%*s", table->sep, "");
	    c = table->items[n][0] - '0';
	    w = -table->sep;
	    while (c--)
		w += colsizes[i++] + table->sep;
	    if (table->items[n][1] == '-')
		fprintf(out, "%-*.*s", w, w, table->items[n] + 2);
	    else if (table->items[n][1] == '+')
		fprintf(out, "%*.*s", w, w, table->items[n] + 2);
	    else if (table->items[n][1] == '.') {
		z = (w - strlen(table->items[n] + 2)) / 2;
		if (z < 0)
		    z = w;
		fprintf(out, "%*.*s%*s",
			     w - z, w - z, table->items[n] + 2, z, "");
	    } else if (table->items[n][1] == '!') {
		mlwidth = w;
		mlpos = pos;
		mlstr = table->items[n] + 2;
		p = findstrbreak(&mlstr, w, &z);
		fprintf(out, "%.*s%*s", z, p, w - z, "");
	    }
	    pos += w;
	}
	fputc('\n', out);
	while (mlstr && *mlstr) {
	    p = findstrbreak(&mlstr, mlwidth, &w);
	    fprintf(out, "%*s%.*s\n", mlpos, "", w, p);
	}
    }
    free(colsizes);
}

/* Display directory settings.
 */
static void printdirectories(void)
{
    printf("Resource files read from:        %s\n", getresdir());
    printf("Level sets read from:            %s\n", getseriesdir());
    printf("Configured data files read from: %s\n", getseriesdatdir());
    printf("Solution files saved in:         %s\n", getsavedir());
}

/*
 * Callback functions for oshw.
 */

/* An input callback that only accepts the characters Y and N.
 */
static int yninputcallback(void)
{
    switch (input(TRUE)) {
      case 'Y': case 'y':	return 'Y';
      case 'N': case 'n':	return 'N';
      case CmdWest:		return '\b';
      case CmdProceed:		return '\n';
      case CmdQuitLevel:	return -1;
      case CmdQuit:		exit(0);
    }
    return 0;
}

/* An input callback that accepts only alphabetic characters.
 */
static int keyinputcallback(void)
{
    int	ch;

    ch = input(TRUE);
    switch (ch) {
      case CmdWest:		return '\b';
      case CmdProceed:		return '\n';
      case CmdQuitLevel:	return -1;
      case CmdQuit:		exit(0);
      default:
	if (isalpha(ch))
	    return toupper(ch);
    }
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

/* An input callback used while displaying the scrolling list of scores.
 */
static int scorescrollinputcallback(int *move)
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
      case CmdSeeSolutionFiles:	*move = CmdSeeSolutionFiles;	return FALSE;
      case CmdQuitLevel:	*move = CmdQuitLevel;		return FALSE;
      case CmdHelp:		*move = CmdHelp;		return FALSE;
      case CmdQuit:						exit(0);
    }
    return TRUE;
}

/* An input callback used while displaying the scrolling list of solutions.
 */
static int solutionscrollinputcallback(int *move)
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
      case CmdSeeScores:	*move = CmdSeeScores;		return FALSE;
      case CmdQuitLevel:	*move = CmdQuitLevel;		return FALSE;
      case CmdHelp:		*move = CmdHelp;		return FALSE;
      case CmdQuit:						exit(0);
    }
    return TRUE;
}

/* An input callback used while displaying scrolling text.
 */
static int textscrollinputcallback(int *move)
{
    int cmd;
    switch ((cmd = input(TRUE))) {
      case CmdPrev10:		*move = SCROLL_PAGE_UP;		break;
      case CmdNorth:		*move = SCROLL_UP;		break;
      case CmdPrev:		*move = SCROLL_UP;		break;
      case CmdPrevLevel:	*move = SCROLL_UP;		break;
      case CmdSouth:		*move = SCROLL_DN;		break;
      case CmdNext:		*move = SCROLL_DN;		break;
      case CmdNextLevel:	*move = SCROLL_DN;		break;
      case CmdNext10:		*move = SCROLL_PAGE_DN;		break;
      case CmdProceed:		*move = CmdProceed;		return FALSE;
      case CmdQuitLevel:	*move = CmdQuitLevel;		return FALSE;
      case CmdQuit:						exit(0);
    }
    return TRUE;
}

/*
 * Basic game activities.
 */

/* Return TRUE if the given level is a final level.
 */
static int islastinseries(gamespec const *gs, int index)
{
    return index == gs->series.count - 1
	|| gs->series.games[index].number == gs->series.final;
}

/* Return TRUE if the current level has a solution.
 */
static int issolved(gamespec const *gs, int index)
{
    return hassolution(gs->series.games + index);
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
    if (n < 0 || n >= gs->series.count)
	return FALSE;

    if (gs->usepasswds)
	if (n > 0 && !(gs->series.games[n].sgflags & SGF_HASPASSWD)
		  && !issolved(gs, n -1))
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
    else if (n >= gs->series.count)
	n = gs->series.count - 1;

    if (gs->usepasswds && n > 0) {
	sign = offset < 0 ? -1 : +1;
	for ( ; n >= 0 && n < gs->series.count ; n += sign) {
	    if (!n || (gs->series.games[n].sgflags & SGF_HASPASSWD)
		   || issolved(gs, n - 1)) {
		m = n;
		break;
	    }
	}
	n = m;
	if (n == gs->currentgame && offset != sign) {
	    n = gs->currentgame + offset - sign;
	    for ( ; n != gs->currentgame ; n -= sign) {
		if (n < 0 || n >= gs->series.count)
		    continue;
		if (!n || (gs->series.games[n].sgflags & SGF_HASPASSWD)
		       || issolved(gs, n - 1))
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
static int melindawatching(gamespec const *gs)
{
    if (!gs->usepasswds)
	return FALSE;
    if (islastinseries(gs, gs->currentgame))
	return FALSE;
    if (gs->series.games[gs->currentgame + 1].sgflags & SGF_HASPASSWD)
	return FALSE;
    if (issolved(gs, gs->currentgame))
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
    switch (topic) {
      case Help_First:
      case Help_FileListKeys:
      case Help_ScoreListKeys:
	onlinecontexthelp(topic);
	break;
      default:
	onlinemainhelp(topic);
	break;
    }
    popsubtitle();
}

/* Display a scrolling list of the available solution files, and allow
 * the user to select one. Return TRUE if the user selected a solution
 * file different from the current one. Do nothing if there is only
 * one solution file available. (If for some reason the new solution
 * file cannot be read, TRUE will still be returned, as the list of
 * solved levels will still need to be updated.)
 */
static int showsolutionfiles(gamespec *gs)
{
    tablespec		table;
    char const	      **filelist;
    int			readonly = FALSE;
    int			count, current, f, n;

    if (haspathname(gs->series.name) || (gs->series.savefilename
				&& haspathname(gs->series.savefilename))) {
	bell();
	return FALSE;
    } else if (!createsolutionfilelist(&gs->series, FALSE, &filelist,
				       &count, &table)) {
	bell();
	return FALSE;
    }

    current = -1;
    n = 0;
    if (gs->series.savefilename) {
	for (n = 0 ; n < count ; ++n)
	    if (!strcmp(filelist[n], gs->series.savefilename))
		break;
	if (n == count)
	    n = 0;
	else
	    current = n;
    }

    pushsubtitle(gs->series.name);
    for (;;) {
	f = displaylist("SOLUTION FILES", &table, &n,
			solutionscrollinputcallback);
	if (f == CmdProceed) {
	    readonly = FALSE;
	    break;
	} else if (f == CmdSeeScores) {
	    readonly = TRUE;
	    break;
	} else if (f == CmdQuitLevel) {
	    n = -1;
	    break;
	} else if (f == CmdHelp) {
	    dohelp(Help_FileListKeys);
	}
    }
    popsubtitle();

    f = n >= 0 && n != current;
    if (f) {
	clearsolutions(&gs->series);
	if (!gs->series.savefilename)
	    gs->series.savefilename = getpathbuffer();
	sprintf(gs->series.savefilename, "%.*s", getpathbufferlen(),
						 filelist[n]);
	if (readsolutions(&gs->series)) {
	    if (readonly)
		gs->series.gsflags |= GSF_NOSAVING;
	} else {
	    bell();
	}
	n = gs->currentgame;
	gs->currentgame = 0;
	passwordseen(gs, 0);
	changecurrentgame(gs, n);
    }

    freesolutionfilelist(filelist, &table);
    return f;
}

/* Display the scrolling list of the user's current scores, and allow
 * the user to select a current level.
 */
static int showscores(gamespec *gs)
{
    tablespec	table;
    int	       *levellist;
    int		ret = FALSE;
    int		count, f, n;

  restart:
    if (!createscorelist(&gs->series, gs->usepasswds, CHAR_MZERO,
			 &levellist, &count, &table)) {
	bell();
	return ret;
    }
    for (n = 0 ; n < count ; ++n)
	if (levellist[n] == gs->currentgame)
	    break;
    pushsubtitle(gs->series.name);
    for (;;) {
	f = displaylist(gs->series.filebase, &table, &n,
			scorescrollinputcallback);
	if (f == CmdProceed) {
	    n = levellist[n];
	    break;
	} else if (f == CmdSeeSolutionFiles) {
	    if (!(gs->series.gsflags & GSF_NODEFAULTSAVE)) {
		n = levellist[n];
		break;
	    }
	} else if (f == CmdQuitLevel) {
	    n = -1;
	    break;
	} else if (f == CmdHelp) {
	    dohelp(Help_ScoreListKeys);
	}
    }
    popsubtitle();
    freescorelist(levellist, &table);
    if (f == CmdSeeSolutionFiles) {
	setcurrentgame(gs, n);
	ret = showsolutionfiles(gs);
	goto restart;
    }
    if (n < 0)
	return ret;
    return setcurrentgame(gs, n) || ret;
}

/* Render the message (if any) associated with the current level. If
 * stage is negative, then display the prologue message; if stage is
 * positive, then the epilogue message is displayed.
 */
static void showlevelmessage(gamespec const *gs, int stage)
{
    char const **pparray;
    int ppcount;
    int number;

    if (stage < 0) {
	if (issolved(gs, gs->currentgame))
	    return;
	number = -gs->series.games[gs->currentgame].number;
    } else {
	number = gs->series.games[gs->currentgame].number;
    }
    pparray = gettaggedmessage(gs->series.messages, number, &ppcount);
    if (pparray)
	displaytextscroll(" ", pparray, ppcount, +1, textscrollinputcallback);
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

/*
 * The game-playing functions.
 */

#define	leveldelta(n)	if (!changecurrentgame(gs, (n))) { bell(); continue; }

/* Get a key command from the user at the start of the current level.
 */
static int startinput(gamespec *gs)
{
    static int	lastlevel = -1;
    char	yn[2];
    int		cmd, n;

    if (gs->currentgame != lastlevel) {
	showlevelmessage(gs, -1);
	lastlevel = gs->currentgame;
	setstepping(0, FALSE);
    }
    drawscreen(TRUE);
    gs->playmode = Play_None;
    for (;;) {
	cmd = input(TRUE);
	if (cmd >= CmdMoveFirst && cmd <= CmdMoveLast) {
	    gs->playmode = Play_Normal;
	    return cmd;
	}
	switch (cmd) {
	  case CmdProceed:	gs->playmode = Play_Normal;	return cmd;
	  case CmdQuitLevel:					return cmd;
	  case CmdPrev10:	leveldelta(-10);		return CmdNone;
	  case CmdPrev:		leveldelta(-1);			return CmdNone;
	  case CmdPrevLevel:	leveldelta(-1);			return CmdNone;
	  case CmdNextLevel:	leveldelta(+1);			return CmdNone;
	  case CmdNext:		leveldelta(+1);			return CmdNone;
	  case CmdNext10:	leveldelta(+10);		return CmdNone;
	  case CmdStepping:	changestepping(4, TRUE);	break;
	  case CmdSubStepping:	changestepping(1, TRUE);	break;
	  case CmdRndSlideDir:	rotaterndslidedir(TRUE);	break;
	  case CmdVolumeUp:	changevolume(+2, TRUE);		break;
	  case CmdVolumeDown:	changevolume(-2, TRUE);		break;
	  case CmdHelp:		dohelp(Help_KeysBetweenGames);	break;
	  case CmdQuit:						exit(0);
	  case CmdPlayback:
	    if (prepareplayback()) {
		gs->playmode = Play_Back;
		return CmdProceed;
	    }
	    bell();
	    break;
	  case CmdCheckSolution:
	    if (prepareplayback()) {
		gs->playmode = Play_Verify;
		return CmdProceed;
	    }
	    bell();
	    break;
	  case CmdReplSolution:
	    if (issolved(gs, gs->currentgame))
		replaceablesolution(gs, -1);
	    else
		bell();
	    break;
	  case CmdKillSolution:
	    if (!issolved(gs, gs->currentgame)) {
		bell();
		break;
	    }
	    yn[0] = '\0';
	    setgameplaymode(BeginInput);
	    n = displayinputprompt("Really delete solution?",
				   yn, 1, yninputcallback);
	    setgameplaymode(EndInput);
	    if (n && *yn == 'Y')
		if (deletesolution())
		    savesolutions(&gs->series);
	    break;
	  case CmdSeeScores:
	    if (showscores(gs))
		return CmdNone;
	    break;
	  case CmdSeeSolutionFiles:
	    if (showsolutionfiles(gs))
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
    char	yn[2];
    int		bscore = 0, tscore = 0;
    long	gscore = 0;
    int		n;

    if (gs->status < 0) {
	if (melindawatching(gs) && secondsplayed() >= 10) {
	    ++gs->melindacount;
	    if (gs->melindacount >= 10) {
		yn[0] = '\0';
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
	  case CmdVolumeUp:	changevolume(+2, TRUE);		break;
	  case CmdVolumeDown:	changevolume(-2, TRUE);		break;
	  case CmdPrev10:	changecurrentgame(gs, -10);	return TRUE;
	  case CmdPrevLevel:	changecurrentgame(gs, -1);	return TRUE;
	  case CmdPrev:		changecurrentgame(gs, -1);	return TRUE;
	  case CmdSameLevel:					return TRUE;
	  case CmdSame:						return TRUE;
	  case CmdNextLevel:	changecurrentgame(gs, +1);	return TRUE;
	  case CmdNext:		changecurrentgame(gs, +1);	return TRUE;
	  case CmdNext10:	changecurrentgame(gs, +10);	return TRUE;
	  case CmdGotoLevel:	selectlevelbypassword(gs);	return TRUE;
	  case CmdPlayback:					return TRUE;
	  case CmdSeeScores:	showscores(gs);			return TRUE;
	  case CmdSeeSolutionFiles: showsolutionfiles(gs);	return TRUE;
	  case CmdKillSolution:					return TRUE;
	  case CmdHelp:		dohelp(Help_KeysBetweenGames);	return TRUE;
	  case CmdQuitLevel:					return FALSE;
	  case CmdQuit:						exit(0);
	  case CmdCheckSolution:
	  case CmdProceed:
	    if (gs->status > 0) {
		if (gs->playmode == Play_Normal)
		    showlevelmessage(gs, +1);
		if (islastinseries(gs, gs->currentgame))
		    gs->enddisplay = TRUE;
		else
		    changecurrentgame(gs, +1);
	    }
	    return TRUE;
	  case CmdReplSolution:
	    if (issolved(gs, gs->currentgame))
		replaceablesolution(gs, -1);
	    else
		bell();
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
	render = waitfortick();
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
		setgameplaymode(SuspendPlayShuttered);
		drawscreen(TRUE);
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
	      case CmdCheatHome:      case CmdCheatStuff:	break;
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

/* Play back the user's best solution for the current level in real
 * time. Other than the fact that this function runs from a
 * prerecorded series of moves, it has the same behavior as
 * playgame().
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
	render = waitfortick();
	switch (input(FALSE)) {
	  case CmdVolumeUp:	changevolume(+2, TRUE);		break;
	  case CmdVolumeDown:	changevolume(-2, TRUE);		break;
	  case CmdPrevLevel:	changecurrentgame(gs, -1);	goto quitloop;
	  case CmdNextLevel:	changecurrentgame(gs, +1);	goto quitloop;
	  case CmdSameLevel:					goto quitloop;
	  case CmdPlayback:					goto quitloop;
	  case CmdQuitLevel:					goto quitloop;
	  case CmdQuit:						exit(0);
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
    gs->playmode = Play_None;
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
    gs->playmode = Play_None;
    return FALSE;
}

/* Quickly play back the user's best solution for the current level
 * without rendering and without using the timer the keyboard. The
 * playback stops when the solution is finished or gameplay has
 * ended.
 */
static int verifyplayback(gamespec *gs)
{
    int	n;

    gs->status = 0;
    setdisplaymsg("Verifying ...", 1, 0);
    setgameplaymode(BeginVerify);
    for (;;) {
	n = doturn(CmdNone);
	if (n)
	    break;
	advancetick();
	switch (input(FALSE)) {
	  case CmdPrevLevel:	changecurrentgame(gs, -1);	goto quitloop;
	  case CmdNextLevel:	changecurrentgame(gs, +1);	goto quitloop;
	  case CmdSameLevel:					goto quitloop;
	  case CmdPlayback:					goto quitloop;
	  case CmdQuitLevel:					goto quitloop;
	  case CmdQuit:						exit(0);
	}
    }
    gs->playmode = Play_None;
    quitgamestate();
    drawscreen(TRUE);
    setgameplaymode(EndVerify);
    if (n < 0) {
	setdisplaymsg("Invalid solution!", 1, 1);
	replaceablesolution(gs, +1);
    }
    if (n > 0) {
	if (checksolution())
	    savesolutions(&gs->series);
	setdisplaymsg(NULL, 0, 0);
	if (islastinseries(gs, gs->currentgame))
	    n = 0;
    }
    gs->status = n;
    return TRUE;

  quitloop:
    setdisplaymsg(NULL, 0, 0);
    gs->playmode = Play_None;
    setgameplaymode(EndVerify);
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
	changesubtitle(NULL);
	setenddisplay();
	drawscreen(TRUE);
	displayendmessage(0, 0, 0, 0);
	endgamestate();
	return finalinput(gs);
    }

    valid = initgamestate(gs->series.games + gs->currentgame,
			  gs->series.ruleset, TRUE);
    changesubtitle(gs->series.games[gs->currentgame].name);
    passwordseen(gs, gs->currentgame);
    if (!islastinseries(gs, gs->currentgame))
	if (!valid || gs->series.games[gs->currentgame].unsolvable)
	    passwordseen(gs, gs->currentgame + 1);

    cmd = startinput(gs);
    if (cmd == CmdQuitLevel) {
	ret = FALSE;
    } else {
	if (cmd != CmdNone) {
	    if (valid) {
		switch (gs->playmode) {
		  case Play_Normal:	f = playgame(gs, cmd);		break;
		  case Play_Back:	f = playbackgame(gs);		break;
		  case Play_Verify:	f = verifyplayback(gs);		break;
		  default:		f = FALSE;			break;
		}
		if (f)
		    ret = endinput(gs);
	    } else
		bell();
	}
    }

    endgamestate();
    return ret;
}

/* Quickly play back all of the user's solutions in the series without
 * rendering or using the timer or the keyboard. If display is TRUE,
 * the solutions that cannot be verified are reported to stdout. The
 * return value is the number of invalid solutions found.
 */
static int batchverify(gameseries *series, int display)
{
    gamesetup  *game;
    int		valid = 0, invalid = 0;
    int		i, f;

    for (i = 0, game = series->games ; i < series->count ; ++i, ++game) {
	if (!hassolution(game))
	    continue;
	if (initgamestate(game, series->ruleset, FALSE) && prepareplayback()) {
	    setgameplaymode(BeginVerify);
	    while (!(f = doturn(CmdNone)))
		advancetick();
	    setgameplaymode(EndVerify);
	    if (f > 0) {
		++valid;
		checksolution();
	    } else {
		++invalid;
		game->sgflags |= SGF_REPLACEABLE;
		if (display)
		    printf("Solution for level %d is invalid\n", game->number);
	    }
	}
	endgamestate();
    }

    if (display) {
	if (valid + invalid == 0) {
	    printf("No solutions were found.\n");
	} else {
	    printf("  Valid solutions:%4d\n", valid);
	    printf("Invalid solutions:%4d\n", invalid);
	}
    }
    return invalid;
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
 * access matches it, then that level will be the initial current
 * level. The return value is zero if nothing was selected, negative
 * if an error occurred, or positive otherwise.
 */
static int selectseriesandlevel(gamespec *gs, seriesdata *series, int autosel,
				char const *defaultseries, int defaultlevel)
{
    int	level, okay, f, n;

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
	    f = displaylist("   Welcome to Tile World. Type ? or F1 for help.",
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
		dohelp(Help_First);
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
    if (gs->series.count < 1) {
	errmsg(gs->series.filebase, "no levels found in data file");
	freeseriesdata(&gs->series);
	return -1;
    }

    gs->enddisplay = FALSE;
    gs->playmode = Play_None;
    gs->usepasswds = usepasswds && !(gs->series.gsflags & GSF_IGNOREPASSWDS);
    gs->currentgame = -1;
    gs->melindacount = 0;

    level = defaultlevel ? defaultlevel : gs->series.currentlevel;
    if (level) {
	n = findlevelinseries(&gs->series, level, NULL);
	if (n >= 0) {
	    gs->currentgame = n;
	    if (gs->usepasswds &&
			!(gs->series.games[n].sgflags & SGF_HASPASSWD))
		changecurrentgame(gs, -1);
	}
    }
    if (gs->currentgame < 0) {
	gs->currentgame = 0;
	for (n = 0 ; n < gs->series.count ; ++n) {
	    if (!issolved(gs, n)) {
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

/* Record the number of the level last visited in the solution file.
 */
static int rememberlastlevel(gamespec *gs)
{
    if (gs->currentgame < 0 || gs->currentgame >= gs->series.count)
	gs->series.currentlevel = 0;
    else
	gs->series.currentlevel = gs->series.games[gs->currentgame].number;
    return savesolutionlevel(&gs->series);
}

/*
 * Initialization functions.
 */

/* Allocate and assemble a directory path based on a root location, a
 * default subdirectory name, and an optional override value.
 */
static char const *choosepath(char const *root, char const *dirname,
			      char const *override)
{
    char       *dir;

    dir = getpathbuffer();
    if (override && *override)
	strcpy(dir, override);
    else
	combinepath(dir, root, dirname);
    return dir;
}

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

    setresdir(choosepath(root, "res", res));
    setseriesdir(choosepath(root, "sets", series));
    setseriesdatdir(choosepath(root, "data", seriesdat));
#ifdef SAVEDIR
    setsavedir(choosepath(SAVEDIR, ".", save));
#else
    if ((dir = getenv("HOME")) && *dir && strlen(dir) < maxpath - 8)
	setsavedir(choosepath(dir, ".tworld", save));
    else
	setsavedir(choosepath(root, "save", save));
#endif
}

/* Basic number-parsing function that silently clamps input to a valid
 * value. Non-numeric string input will return min. (This function
 * assumes that min and max constrained enough that they are not equal
 * to LONG_MIN and/or LONG_MAX.)
 */
static int nparse(char const *str, int min, int max)
{
    int n;

    parseint(str, &n, min);
    return n < min ? min : n > max ? max : n;
}

/* Handle one option/argument from the command line or init file.
 */
static int processoption(int opt, char const *val, void *data)
{
    startupdata	       *start = data;
    int			n;

    switch (opt) {
      case 0:
	if (start->savefilename && start->levelnum) {
	    fprintf(stderr, "too many arguments: %s\n", val);
	    return 1;
	}
	if (!start->levelnum && (n = nparse(val, 0, 999)) > 0) {
	    start->levelnum = n;
	} else if (*start->filename) {
	    start->savefilename = getpathbuffer();
	    sprintf(start->savefilename, "%.*s", getpathbufferlen(), val);
	} else {
	    sprintf(start->filename, "%.*s", getpathbufferlen(), val);
	}
	break;
      case 'i':
	start->selectfilename = getpathbuffer();
	sprintf(start->selectfilename, "%.*s", getpathbufferlen(), val);
	break;
      case 'D':	    start->seriesdatdir = val;			    break;
      case 'L':	    start->seriesdir = val;			    break;
      case 'R':	    start->resdir = val;			    break;
      case 'S':	    start->savedir = val;			    break;
      case 'H':	    start->showhistogram = !start->showhistogram;   break;
      case 'F':	    start->fullscreen = !start->fullscreen;	    break;
      case 'p':	    usepasswds = !usepasswds;			    break;
      case 'q':	    silence = !silence;				    break;
      case 'r':	    start->readonly = !start->readonly;		    break;
      case 'P':	    start->pedantic = !start->pedantic;		    break;
      case 'n':	    start->volumelevel = nparse(val, 0, 10);	    break;
      case 'a':	    start->soundbufsize = nparse(val, 0, 5);	    break;
      case 'd':	    start->listdirs = TRUE;			    break;
      case 'l':	    start->listseries = TRUE;			    break;
      case 's':	    start->listscores = TRUE;			    break;
      case 't':	    start->listtimes = TRUE;			    break;
      case 'b':	    start->batchverify = TRUE;			    break;
      case 'm':	    start->mudsucking = nparse(val, 1, 10);	    break;
      case 'h':	    printtable(stdout, yowzitch);      exit(EXIT_SUCCESS);
      case 'V':	    printtable(stdout, vourzhon);      exit(EXIT_SUCCESS);
      case 'v':	    puts(VERSION);		       exit(EXIT_SUCCESS);
      case ':':
	fprintf(stderr, "option requires an argument: %s\n", val);
	return 1;
      case '=':
	fprintf(stderr, "option does not take an argument: %s\n", val);
	return 1;
      default:
	fprintf(stderr, "unrecognized option: %s\n", val);
	return 1;
    }
    return 0;
}

/* Parse the command-line options and arguments, and initialize the
 * user-controlled options.
 */
static int getsettingsfromcmdline(int argc, char *argv[], startupdata *start)
{
    static option const optlist[] = {
	{ "audio-buffer",	'a', 'a', 1 },
	{ "batch-verify",	'b', 'b', 0 },
	{ "data-dir",		'D', 'D', 1 },
	{ "list-dirs",		'd', 'd', 0 },
	{ "full-screen",	'F', 'F', 0 },
	{ "histogram",		 0 , 'H', 0 },
	{ "help",		'h', 'h', 0 },
	{ "initial-levelset",	 0 , 'i', 1 },
	{ "levelset-dir",	'L', 'L', 1 },
	{ "list-levelsets",	'l', 'l', 0 },
#ifndef NDEBUG
	{ "mud-sucking",	'm', 'm', 1 },
#endif
	{ "volume",		'n', 'n', 1 },
	{ "pedantic",		'P', 'P', 0 },
	{ "no-passwords",	'p', 'p', 0 },
	{ "quiet",		'q', 'q', 0 },
	{ "resource-dir",	'R', 'R', 1 },
	{ "read-only",		'r', 'r', 0 },
	{ "save-dir",		'S', 'S', 1 },
	{ "list-scores",	's', 's', 0 },
	{ "list-times",		't', 't', 0 },
	{ "version",		'V', 'V', 0 },
	{ "version-number",	'v', 'v', 0 },
	{ 0, 0, 0, 0 }
    };

    char	buf[256];

    start->filename = getpathbuffer();
    *start->filename = '\0';
    start->selectfilename = NULL;
    start->savefilename = NULL;
    start->levelnum = 0;
    start->resdir = NULL;
    start->seriesdir = NULL;
    start->seriesdatdir = NULL;
    start->savedir = NULL;
    start->listdirs = FALSE;
    start->listseries = FALSE;
    start->listscores = FALSE;
    start->listtimes = FALSE;
    start->batchverify = FALSE;
    start->showhistogram = FALSE;
    start->pedantic = FALSE;
    start->fullscreen = FALSE;
    start->readonly = FALSE;
    start->volumelevel = -1;
    start->soundbufsize = -1;
    start->mudsucking = 1;

    if (readoptions(optlist, argc, argv, processoption, start)) {
	fprintf(stderr, "Try --help for more information.\n");
	return FALSE;
    }
    if (start->readonly)
	setreadonly();
    if (start->pedantic)
	setpedanticmode();

    initdirs(start->seriesdir, start->seriesdatdir,
	     start->resdir, start->savedir);
    if (start->listdirs) {
	printdirectories();
	exit(EXIT_SUCCESS);
    }

    if (*start->filename && !start->savefilename) {
	if (loadsolutionsetname(start->filename, buf) > 0) {
	    start->savefilename = getpathbuffer();
	    strcpy(start->savefilename, start->filename);
	    strcpy(start->filename, buf);
	}
    }

    return TRUE;
}

/* Parse the user's initialization file, if it exists, and add its
 * values to the startup data struct. (Note that any values provided
 * on the command line take precedence over initialization file
 * settings. But the initialization file needs to be read after the
 * command-line options, in case a non-default savedir was specified.)
 */
static int getsettingsfrominitfile(startupdata *start)
{
    static option const optlist[] = {
	{ "initial-levelset",	0, 'i', 1 },
	{ "volume",		0, 'n', 1 },
	{ 0, 0, 0, 0 }
    };

    startupdata		rcstart;
    fileinfo		file;
    int			f;

    clearfileinfo(&file);
    if (!openfileindir(&file, getsavedir(), initfilename, "r", NULL))
	return TRUE;

    rcstart.selectfilename = NULL;
    rcstart.volumelevel = -1;
    if (readinitfile(optlist, &file, processoption, &rcstart) == 0) {
	if (!start->selectfilename)
	    start->selectfilename = rcstart.selectfilename;
	if (start->volumelevel < 0)
	    start->volumelevel = rcstart.volumelevel;
	f = TRUE;
    } else {
	fileerr(&file, "invalid initialization file syntax");
	f = FALSE;
    }

    fileclose(&file, NULL);
    return f;
}

/* Write (or rewrite) the initialization file with the current values
 * for the settings. If the current levelset was a one-off entered on
 * the command line, then don't save it.
 */
static int writeinitfile(char const *lastseries)
{
    fileinfo file;

    if (haspathname(lastseries))
	return TRUE;
    clearfileinfo(&file);
    if (!openfileindir(&file, getsavedir(), initfilename, "w", "write error"))
	return FALSE;
    fprintf(file.fp, "initial-levelset=%s\n", lastseries);
    fprintf(file.fp, "volume=%d\n", getvolume());
    fileclose(&file, NULL);
    return TRUE;
}

/* Read program settings from the initialization file and the
 * command-line arguments.
 */
static int getsettings(int argc, char *argv[], startupdata *start)
{
    if (!getsettingsfromcmdline(argc, argv, start))
	return FALSE;
    if (!getsettingsfrominitfile(start))
	return FALSE;
    if (start->listscores || start->listtimes || start->batchverify
					      || start->levelnum) {
	if (!*start->filename) {
	    errmsg(NULL, "no level set specified");
	    return FALSE;
	}
    }
    return TRUE;
}

/* Run the initialization routines of oshw and the resource module.
 */
static int initializesystem(startupdata const *start)
{
    setmudsuckingfactor(start->mudsucking);
    if (!oshwinitialize(silence, start->soundbufsize,
			start->showhistogram, start->fullscreen))
	return FALSE;
    if (!initresources())
	return FALSE;
    setkeyboardrepeat(TRUE);
    if (start->volumelevel >= 0)
	setvolume(start->volumelevel, FALSE);
    return TRUE;
}

/* Time for everyone to clean up and go home.
 */
static void shutdownsystem(void)
{
    shutdowngamestate();
    freeallresources();
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
    int		n;

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
	if (start->savefilename)
	    series.list[0].savefilename = start->savefilename;
	if (!readseriesfile(series.list)) {
	    errmsg(series.list[0].filebase, "cannot read level set");
	    return -1;
	}
	if (start->batchverify) {
	    n = batchverify(series.list, !silence && !start->listtimes
						  && !start->listscores);
	    if (silence)
		exit(n > 100 ? 100 : n);
	    else if (!start->listtimes && !start->listscores)
		return 0;
	}
	if (start->listscores) {
	    if (!createscorelist(series.list, usepasswds, '0',
				 NULL, NULL, &table))
		return -1;
	    freeserieslist(series.list, series.count, &series.table);
	    printtable(stdout, &table);
	    freescorelist(NULL, &table);
	    return 0;
	}
	if (start->listtimes) {
	    if (!createtimelist(series.list,
				series.list->ruleset == Ruleset_MS ? 10 : 100,
				'0', NULL, NULL, &table))
		return -1;
	    freeserieslist(series.list, series.count, &series.table);
	    printtable(stdout, &table);
	    freetimelist(NULL, &table);
	    return 0;
	}
    }

    if (!initializesystem(start)) {
	errmsg(NULL, "cannot initialize program due to previous errors");
	return -1;
    }

    return selectseriesandlevel(gs, &series, TRUE,
				start->selectfilename, start->levelnum);
}

/*
 * The main function.
 */

int tworld(int argc, char *argv[])
{
    startupdata	start;
    gamespec	spec;
    char	lastseries[sizeof spec.series.filebase];
    int		f;

    if (!getsettings(argc, argv, &start))
	return EXIT_FAILURE;

    f = choosegameatstartup(&spec, &start);
    if (f < 0)
	return EXIT_FAILURE;
    else if (f == 0)
	return EXIT_SUCCESS;

    do {
	pushsubtitle(NULL);
	while (runcurrentlevel(&spec)) ;
	rememberlastlevel(&spec);
	popsubtitle();
	cleardisplay();
	strcpy(lastseries, spec.series.filebase);
	freeseriesdata(&spec.series);
	f = choosegame(&spec, lastseries);
    } while (f > 0);

    writeinitfile(lastseries);
    shutdownsystem();
    return f == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
