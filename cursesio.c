#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<signal.h>
#include	<errno.h>
#include	<stdarg.h>
#include	<unistd.h>
#include	<sys/time.h>
#include	<ncurses.h>
#include	"gen.h"
#include	"cc.h"
#include	"objects.h"
#include	"userio.h"

#ifndef	NSIG
#define	NSIG	32
#endif

#define	CXVIEW		9
#define	CYVIEW		9

#define	NTILES		(Count_Floors + 4 * Count_Entities)

static int silence = FALSE;
static int			inrawmode = FALSE;
static struct sigaction		prevhandler[NSIG];
static struct sigaction		myhandler[NSIG];

char const     *programname = "a.out";
char const     *currentfilename = NULL;

static char const *floors[] = {
/* 00 empty space		*/	" ",
/* 12 force north		*/	"\033[32m^\033[0m",
/* 14 force west		*/	"\033[32m\253\033[0m",
/* 0D force south		*/	"\033[32mv\033[0m",
/* 13 force east		*/	"\033[32m\273\033[0m",
/* 32 force all directions	*/	"\033[32m+\033[0m",
/* 0C ice			*/	"\033[36m\033(U\260\033(B\033[0m",
/* 1A SE ice slide		*/	"\033[36m\016j\017\033[0m",
/* 1B SW ice slide		*/	"\033[36m\016m\017\033[0m",
/* 1C NE ice slide		*/	"\033[36m\016k\017\033[0m",
/* 1D NW ice slide		*/	"\033[36m\016l\017\033[0m",
/* 2D gravel			*/	"\033(U\261\033(B",
/* 0B dirt			*/	"\033(U\260\033(B",
/* 03 water			*/	"\033[36;44m=\033[0m",
/* 04 fire			*/	"\033[31mx\033[0m",
/* 2A bomb			*/	"\033[1;31m\362\033[0m",
/* 2B trap			*/	"\033[33m*\033[0m",
/* 21 thief			*/	"T",
/* 2F hint			*/	"?",
/* 28 blue button		*/	"\033[1;34m.\033[0m",
/* 23 green button		*/	"\033[32m.\033[0m",
/* 24 red button		*/	"\033[31m.\033[0m",
/* 27 brown button		*/	"\033[33m.\033[0m",
/* 29 teleport			*/	"\033[36m*\033[0m",
/* 01 wall			*/	"\033[7m \033[0m",
/* 06 blocked north		*/	"\016o\017",
/* 07 blocked west		*/	"\033(U\335\033(B",
/* 08 blocked south		*/	"_",
/* 09 blocked east		*/	"\033(U\336\033(B",
/* 30 blocked SE		*/	"\016j\017",
/* 05 invisible wall, perm.	*/	"\033[2m\033(U\261\033(B\033[0m",
/* 2C invisible wall, temp.	*/	"\033[2m\033(U\260\033(B\033[0m",
/* 1F blue block, wall		*/	"\033[7;34m \033[0m",
/* 1E blue block, tile		*/	"\033[7;34m\033(U\260\033(B\033[0m",
/* 26 switch block, open	*/	"\033[32m\201\033[0m",
/* 25 switch block, closed	*/	"\033[7;32m\201\033[0m",
/* 2E pass once			*/	"*",
/* 31 cloning machine		*/	"C",
/* 16 blue door			*/	"\033[36m0\033[0m",
/* 18 green door		*/	"\033[32m0\033[0m",
/* 17 red door			*/	"\033[31m0\033[0m",
/* 19 yellow door		*/	"\033[1;33m0\033[0m",
/* 22 socket			*/	"H",
/* 15 exit			*/	"\033[37;44m@\033[0m",
/* 02 chip			*/	"#",
/* 64 Blue key			*/	"\033[36m\245\033[0m",
/* 66 Green key			*/	"\033[32m\245\033[0m",
/* 65 Red key			*/	"\033[31m\245\033[0m",
/* 67 Yellow key		*/	"\033[1;33m\245\033[0m",
/* 6B Suction boots		*/	"\033[32m\244\033[0m",
/* 6A Ice skates		*/	"\033[36m\244\033[0m",
/* 68 Flippers			*/	"\033[34m\244\033[0m",
/* 69 Fire boots		*/	"\033[31m\244\033[0m",
};

static char const *entities[] = {
					" ",
/* 6C Chip N			*/	"\251",
/* 0A block			*/	"\201",
/* 4D Tank W			*/	"\033[35mH\033[0m",
/* 48 Pink ball N		*/	"\033[35mo\033[0m",
/* 53 Glider E			*/	"\033[35mG\033[0m",
/* 47 Fireball E		*/	"\033[35mF\033[0m",
/* 58 Walker N			*/	"\033[35mX\033[0m",
/* 5F Blob E			*/	"\033[35mb\033[0m",
/* 57 Teeth E			*/	"\033[35mT\033[0m",
/* 40 Bug N			*/	"\033[35mB\033[0m",
/* 60 Paramecium N		*/	"\033[35mP\033[0m",
};

/*
 *
 */

static void writemlstring(int xpos, int ypos, cx, int cy, char const *text)
{
    int	index, width, y, n;

    width = cx;
    index = 0;
    for (y = 0 ; y + 1 <= cy ; y += 1) {
	while (isspace(text[index]))
	    ++index;
	if (!text[index])
	    break;
	n = strlen(text + index);
	if (n > width) {
	    n = width;
	    while (!isspace(text[index + n]) && n >= 0)
		--n;
	    if (n < 0)
		n = width;
	}
	mvaddnstr(ypos, xpos, text + index, n);
	index += n;
    }
}

int displaygame(mapcell const *map, int chippos, int chipdir,
		int icchipsleft, int timeleft, int state,
		int levelnum, char const *title, char const *passwd,
		int besttime, char const *hinttext,
		short keys[4], short boots[4], char const *soundeffect)
{
    int	xfrom, yfrom, pos, x, y;

    x = chippos % CXGRID;
    y = chippos / CXGRID;
    xfrom = x - CXVIEW / 2;
    yfrom = y - CYVIEW / 2;
    if (xfrom < 0)			xfrom = 0;
    if (yfrom < 0)			yfrom = 0;
    if (xfrom > CXGRID - CXVIEW)	xfrom = CXGRID - CXVIEW;
    if (yfrom > CYGRID - CYVIEW)	yfrom = CYGRID - CYVIEW;

    erase();
    for (y = 0 ; y < CYVIEW ; ++y) {
	for (x = 0 ; x < CXVIEW ; ++x) {
	    pos = (yfrom + y) * CXGRID + (xfrom + x);
	    if (map[pos].entity)
		addch(entities[map[pos].entity]);
	    else if (pos == chippos)
		addch(entities[Chip]);
	    else
		addch(floors[map[pos].floor]);
	}
	addch('\n');
    }

    if (soundeffect && *soundeffect)
	addstr(soundeffect);
    addstr("\n\n");

    printw("Chips %3d\n", icchipsleft);
    if (timeleft < 0)
	addstr("Time  ---\n");
    else
	printw("Time  %3d\n", timeleft);

    if (!state) {
	addstr("Level %d\n%s\n", levelnum, title);
	addstr("Password: %s\n", passwd);
	if (besttime > 0) {
	    if (timeleft < 0)
		addstr("(Best time: %3d)\n", besttime);
	    else
		addstr("Best time: %3d\n", besttime);
	}
    } else if (map[chippos].floor == HintButton) {
	writemlstring(CYVIEW + 4, 0, 32, 10, hinttext);
    } else {
	printw("%s   ", keys[0] ? floors[Key_Blue] : " ");
	printw("%s   ", keys[1] ? floors[Key_Green] : " ");
	printw("%s   ", keys[2] ? floors[Key_Red] : " ");
	printw("%s   ", keys[3] ? floors[Key_Yellow] : " ");
	printw("%s   ", boots[0] ? floors[Boots_Slide] : " ");
	printw("%s   ", boots[1] ? floors[Boots_Ice] : " ");
	printw("%s   ", boots[2] ? floors[Boots_Water] : " ");
	printw("%s \n", boots[3] ? floors[Boots_Fire] : " ");
    }
    refresh();
    return TRUE;
}

int displayhelp(char const *title, objhelptext const *text, int textcount)
{
    int	y, x, n;

    erase();
    n = strlen(title);
    printw("%*s\n\n", 40 + n / 2, title);
    for (n = 0 ; n < textcount ; ++n) {
	if (text[n].isfloor)
	    printw("%s %s  ", floors[text[n].item1],floors[text[n].item2]);
	else
	    printw("%s %s  ", entities[text[n].item1],entities[text[n].item2]);
	getyx(stdscr, y, x);
	writemlstring(y, x, 74, 2, text[n].desc);
    }
    out("\nPress any key to continue");
    refresh();
    return TRUE;
}

int displayendmessage(int completed)
{
    if (completed > 0) {
	mvaddstr(22, 63, "Press ENTER");
	mvaddstr(23, 63, "to continue");
    } else {
	mvaddstr(22, 63, " \"Bummer.\" ");
	mvaddstr(23, 63, "Press ENTER");
    }
    refresh();
    return TRUE;
}

/*
 *
 */

int inputwait(void)
{
    fd_set	in;

    FD_ZERO(&in);
    FD_SET(STDIN_FILENO, &in);
    return select(STDIN_FILENO + 1, &in, NULL, NULL, NULL) > 0;
}

int input(void)
{
    int	ch;

    ch = getch();
    switch (ch) {
      case ERR:		return 0;
      case KEY_LEFT:	return 'h';
      case KEY_DOWN:	return 'j';
      case KEY_UP:	return 'k';
      case KEY_RIGHT:	return 'l';
      case KEY_ENTER:	return '\n';
    }
    return ch;
}

/*
 *
 */

static void shutdown(void)
{
    if (!isendwin)
	endwin();
}

static void chainandredraw(int signum)
{
    sigset_t	mask, prevmask;

    shutdown();
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    sigprocmask(SIG_UNBLOCK, &mask, &prevmask);
    sigaction(signum, prevhandler + signum, NULL);
    raise(signum);
    sigaction(signum, myhandler + signum, NULL);
    sigprocmask(SIG_SETMASK, &prevmask, NULL);
}

static void bailout(int signum)
{
    sigset_t	mask;

    shutdown();
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    sigaction(signum, prevhandler + signum, NULL);
    raise(signum);
}

int ioinitialize(int silenceflag)
{
    struct sigaction	act;

    silence = silenceflag;

    atexit(shutdown);

    if (!initscr())
	return FALSE;

    nonl();
    cbreak();
    nodelay(stdscr, TRUE);
    noecho();
    keypad(stdscr, TRUE);

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    act.sa_handler = chainandredraw;
    myhandler[SIGTSTP] = act;
    sigaction(SIGTSTP, &act, prevhandler + SIGTSTP);
    myhandler[SIGWINCH] = act;
    sigaction(SIGWINCH, &act, prevhandler + SIGWINCH);

    act.sa_handler = bailout;
    sigaction(SIGINT, &act, prevhandler + SIGINT);
    sigaction(SIGQUIT, &act, prevhandler + SIGQUIT);
    sigaction(SIGTERM, &act, prevhandler + SIGTERM);
    sigaction(SIGSEGV, &act, prevhandler + SIGSEGV);
    sigaction(SIGPIPE, &act, prevhandler + SIGPIPE);
    sigaction(SIGILL, &act, prevhandler + SIGILL);
    sigaction(SIGBUS, &act, prevhandler + SIGBUS);
    sigaction(SIGFPE, &act, prevhandler + SIGFPE);

    return TRUE;
}

/*
 * Miscellaneous interface functions
 */

/* Ring the bell.
 */
void ding(void)
{
    beep();
}

/* Display an appropriate error message on stderr; use msg if errno
 * is zero.
 */
int fileerr(char const *msg)
{
    shutdown();
    fputc('\n', stderr);
    fputs(currentfilename ? currentfilename : programname, stderr);
    fputs(": ", stderr);
    fputs(msg ? msg : errno ? strerror(errno) : "unknown error", stderr);
    fputc('\n', stderr);
    return FALSE;
}

/* Display a formatted message on stderr and exit cleanly.
 */
void die(char const *fmt, ...)
{
    va_list	args;

    shutdown();
    va_start(args, fmt);
    fprintf(stderr, "%s: ", programname);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}
