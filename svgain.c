#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<errno.h>
#include	<stdarg.h>
#include	<unistd.h>
#include	<vga.h>
#include	<vgakeyboard.h>
#include	"gen.h"
#include	"userin.h"

#define	SCANCODE_COUNT	128

enum { KS_OFF = 0, KS_ON = 1, KS_DOWN, KS_PRESSED, KS_STRUCK };

static char		keystates[SCANCODE_COUNT];

static int		rawkeys = FALSE;
static int		permitrawkeys = FALSE;

/*
 *
 */

static struct { int scancode, shift, ctl, alt, cmd, hold; } const keycmds[] = {
    { SCANCODE_CURSORBLOCKUP,     0,  0,  0,   CmdNorth,      TRUE },
    { SCANCODE_CURSORBLOCKLEFT,   0,  0,  0,   CmdWest,       TRUE },
    { SCANCODE_CURSORBLOCKDOWN,   0,  0,  0,   CmdSouth,      TRUE },
    { SCANCODE_CURSORBLOCKRIGHT,  0,  0,  0,   CmdEast,       TRUE },
    { SCANCODE_CURSORUP,          0,  0,  0,   CmdNorth,      TRUE },
    { SCANCODE_CURSORLEFT,        0,  0,  0,   CmdWest,       TRUE },
    { SCANCODE_CURSORDOWN,        0,  0,  0,   CmdSouth,      TRUE },
    { SCANCODE_CURSORRIGHT,       0,  0,  0,   CmdEast,       TRUE },
    { SCANCODE_KEYPAD8,           0,  0,  0,   CmdNorth,      TRUE },
    { SCANCODE_KEYPAD4,           0,  0,  0,   CmdWest,       TRUE },
    { SCANCODE_KEYPAD2,           0,  0,  0,   CmdSouth,      TRUE },
    { SCANCODE_KEYPAD6,           0,  0,  0,   CmdEast,       TRUE },
    { SCANCODE_Q,                 0,  0,  0,   CmdQuitLevel,  FALSE },
    { SCANCODE_P,                 0, +1,  0,   CmdPrevLevel,  FALSE },
    { SCANCODE_R,                 0, +1,  0,   CmdSameLevel,  FALSE },
    { SCANCODE_N,                 0, +1,  0,   CmdNextLevel,  FALSE },
    { SCANCODE_Q,                +1,  0,  0,   CmdQuit,       FALSE },
    { SCANCODE_P,                 0,  0,  0,   CmdPrev,       FALSE },
    { SCANCODE_R,                 0,  0,  0,   CmdSame,       FALSE },
    { SCANCODE_N,                 0,  0,  0,   CmdNext,       FALSE },
    { SCANCODE_BACKSPACE,        -1,  0,  0,   CmdPauseGame,  FALSE },
    { SCANCODE_SLASH,            +1,  0,  0,   CmdHelp,       FALSE },
    { SCANCODE_KEYPADDIVIDE,     -1,  0,  0,   CmdHelp,       FALSE },
    { SCANCODE_TAB,               0, -1,  0,   CmdPlayback,   FALSE },
    { SCANCODE_I,                 0, +1,  0,   CmdPlayback,   FALSE },
    { SCANCODE_ENTER,            -1, -1,  0,   CmdProceed,    FALSE },
    { SCANCODE_KEYPADENTER,      -1, -1,  0,   CmdProceed,    FALSE },
    { SCANCODE_SPACE,            -1, -1,  0,   CmdProceed,    FALSE },
    { SCANCODE_PERIOD,            0, -1,  0,   CmdProceed,    FALSE },
    { SCANCODE_D,                 0,  0,  0,   CmdDebugCmd1,  FALSE },
    { SCANCODE_D,                +1,  0,  0,   CmdDebugCmd2,  FALSE }
};

static struct { int keychar, cmd; } const charcmds[] = {
    { 'k',    CmdNorth },	{ 'K',    CmdNorth },
    { 'h',    CmdWest },	{ 'H',    CmdWest },
    { 'j',    CmdSouth },	{ 'J',    CmdSouth },
    { 'l',    CmdEast },	{ 'L',    CmdEast },
    { '8',    CmdNorth },
    { '4',    CmdWest },
    { '2',    CmdSouth },
    { '6',    CmdEast },
    { 'q',    CmdQuitLevel },
    { '\020', CmdPrevLevel },
    { '\022', CmdSameLevel },
    { '\016', CmdNextLevel },
    { 'Q',    CmdQuit },
    { 'p',    CmdPrev },	{ 'P',    CmdPrev },
    { 'r',    CmdSame },	{ 'R',    CmdSame },
    { 'n',    CmdNext }, 	{ 'N',    CmdNext },
    { '\010', CmdPauseGame },	{ '\177', CmdPauseGame },
    { '?',    CmdHelp },	{ '/',    CmdHelp },
    { '\011', CmdPlayback },
    { '\012', CmdProceed },	{ '\015', CmdProceed },
    { ' ',    CmdProceed },
    { '.',    CmdProceed },
    { 'd',    CmdDebugCmd1 },
    { 'D',    CmdDebugCmd2 }
};

/*
 *
 */

static void resetkeystates(void)
{
    int	n;

    for (n = 0 ; n < SCANCODE_COUNT ; ++n) {
	switch (keystates[n]) {
	  case KS_PRESSED:
	    keystates[n] = KS_DOWN;
	    break;
	  case KS_STRUCK:
	    keystates[n] = KS_OFF;
	    break;
	}
    }
}

static void keyeventcallback(int scancode, int down)
{
    switch (scancode) {
      case SCANCODE_LEFTSHIFT:
      case SCANCODE_RIGHTSHIFT:
      case SCANCODE_LEFTCONTROL:
      case SCANCODE_RIGHTCONTROL:
      case SCANCODE_LEFTALT:
      case SCANCODE_RIGHTALT:
      case SCANCODE_NUMLOCK:
      case SCANCODE_CAPSLOCK:
      case SCANCODE_SCROLLLOCK:
	keystates[scancode] = down ? KS_ON : KS_OFF;
	break;
      default:
	if (down)
	    keystates[scancode] = KS_PRESSED;
	else if (keystates[scancode] == KS_PRESSED)
	    keystates[scancode] = KS_STRUCK;
	else
	    keystates[scancode] = KS_OFF;
	break;
    }
}

/*
 *
 */

static int rawanykey(void)
{
    int	n;

    resetkeystates();
    keyboard_update();
    for (;;) {
	resetkeystates();
	keyboard_waitforupdate();
	for (n = 0 ; n < SCANCODE_COUNT ; ++n)
	    if (keystates[n] == KS_PRESSED || keystates[n] == KS_STRUCK)
		return TRUE;
    }
}

static int rawinput(int wait)
{
    int	cmd, i, n;

    for (;;) {
	resetkeystates();
	if (wait)
	    keyboard_waitforupdate();
	else
	    keyboard_update();

	cmd = CmdNone;
	for (i = 0 ; i < (int)(sizeof keycmds / sizeof *keycmds) ; ++i) {
	    n = keystates[keycmds[i].scancode];
	    if (!n)
		continue;
	    if (keycmds[i].shift != -1)
		if (keycmds[i].shift != (keystates[SCANCODE_LEFTSHIFT]
					|| keystates[SCANCODE_RIGHTSHIFT]))
		    continue;
	    if (keycmds[i].ctl != -1)
		if (keycmds[i].ctl != (keystates[SCANCODE_LEFTCONTROL]
					|| keystates[SCANCODE_RIGHTCONTROL]))
		    continue;
	    if (keycmds[i].alt != -1)
		if (keycmds[i].alt != (keystates[SCANCODE_LEFTALT]
					|| keystates[SCANCODE_RIGHTALT]))
		    continue;

	    if (n == KS_PRESSED || (keycmds[i].hold && n == KS_DOWN))
		return keycmds[i].cmd;
	    else if (n == KS_STRUCK)
		cmd = keycmds[i].cmd;
	}
	if (cmd != CmdNone || !wait)
	    break;
    }
    return cmd;
}

static int cookedinput(int wait)
{
    static int	lastkey = 0, penkey = 0;
    fd_set	in;
    int		key;
    int		i;

    if (wait) {
	FD_ZERO(&in);
	FD_SET(STDIN_FILENO, &in);
	if (select(STDIN_FILENO + 1, &in, NULL, NULL, NULL) <= 0)
	    return CmdQuit;
    }

    if (lastkey) {
	key = lastkey;
	lastkey = penkey;
	penkey = 0;
    } else {
	key = vga_getkey();
	if (key < 0) {
	    return CmdQuit;
	} else if (key == '\033') {
	    lastkey = vga_getkey();
	    if (lastkey == '[' || lastkey == 'O') {
		penkey = vga_getkey();
		switch (penkey) {
		  case 'A':	key = 'k'; lastkey = penkey = 0;	break;
		  case 'B':	key = 'j'; lastkey = penkey = 0;	break;
		  case 'C':	key = 'l'; lastkey = penkey = 0;	break;
		  case 'D':	key = 'h'; lastkey = penkey = 0;	break;
		}
	    }
	}
    }

    if (!key)
	return CmdPreserve;

    for (i = 0 ; i < (int)(sizeof charcmds / sizeof *charcmds) ; ++i)
	if (charcmds[i].keychar == key)
	    return charcmds[i].cmd;

    return CmdProceed;
}

static int cookedanykey(void)
{
    (void)cookedinput(TRUE);
    return TRUE;
}

int anykey(void)    { return rawkeys ? rawanykey() : cookedanykey(); }
int input(int wait) { return rawkeys ? rawinput(wait) : cookedinput(wait); }

/*
 *
 */

static void shutdown(void)
{
    if (rawkeys) {
	keyboard_close();
	rawkeys = FALSE;
    }
}

int inputinitialize(int permitrawkeysflag)
{
    atexit(shutdown);
    permitrawkeys = permitrawkeysflag;
    return TRUE;
}

int setkeypolling(int polling)
{
    if (polling) {
	if (!rawkeys && permitrawkeys) {
	    if (!keyboard_init()) {
		rawkeys = TRUE;
		keyboard_seteventhandler(keyeventcallback);
	    }
	}
    } else {
	if (rawkeys) {
	    keyboard_close();
	    rawkeys = FALSE;
	}
    }
    return rawkeys;
}
