/* sdlin.c: Reading the keyboard.
 * 
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<string.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../defs.h"

/* The states of keys.
 */
enum { KS_OFF = 0,		/* key is not currently pressed */
       KS_ON = 1,		/* key is down (shift-type keys only) */
       KS_DOWN,			/* key is down */
       KS_STRUCK,		/* key was pressed and released in one cycle */
       KS_PRESSED,		/* key was pressed in this cycle */
       KS_DOWNBUTOFF1,		/* key was pressed in the previous cycle */
       KS_DOWNBUTOFF2,		/* key was pressed two cycles ago */
       KS_REPRESSED,		/* key is down and is now repeating */
       KS_count
};

/* The array of key states.
 */
static char	keystates[SDLK_LAST];

/* TRUE if selected keys are to be treated as continuously repeating.
 */
static int	joystickstyle = FALSE;

/* The complete list of key commands recognized by the game. hold is
 * TRUE for keys that are to be forced to repeat.
 */
static struct { int scancode, shift, ctl, alt, cmd, hold; } const keycmds[] = {
    { SDLK_UP,                    0,  0,  0,   CmdNorth,      TRUE },
    { SDLK_LEFT,                  0,  0,  0,   CmdWest,       TRUE },
    { SDLK_DOWN,                  0,  0,  0,   CmdSouth,      TRUE },
    { SDLK_RIGHT,                 0,  0,  0,   CmdEast,       TRUE },
    { SDLK_KP8,                   0,  0,  0,   CmdNorth,      TRUE },
    { SDLK_KP4,                   0,  0,  0,   CmdWest,       TRUE },
    { SDLK_KP2,                   0,  0,  0,   CmdSouth,      TRUE },
    { SDLK_KP6,                   0,  0,  0,   CmdEast,       TRUE },
    { 'q',                        0,  0,  0,   CmdQuitLevel,  FALSE },
    { 'p',                        0, +1,  0,   CmdPrevLevel,  FALSE },
    { 'r',                        0, +1,  0,   CmdSameLevel,  FALSE },
    { 'n',                        0, +1,  0,   CmdNextLevel,  FALSE },
    { 'q',                       +1,  0,  0,   CmdQuit,       FALSE },
    { SDLK_PAGEUP,                0,  0,  0,   CmdPrev10,     FALSE },
    { 'p',                        0,  0,  0,   CmdPrev,       FALSE },
    { 'r',                        0,  0,  0,   CmdSame,       FALSE },
    { 'n',                        0,  0,  0,   CmdNext,       FALSE },
    { SDLK_PAGEDOWN,              0,  0,  0,   CmdNext10,     FALSE },
    { '\b',                      -1,  0,  0,   CmdPauseGame,  FALSE },
    { '/',                       +1,  0,  0,   CmdHelp,       FALSE },
    { SDLK_KP_DIVIDE,            -1,  0,  0,   CmdHelp,       FALSE },
    { SDLK_F1,                    0,  0,  0,   CmdHelp,       FALSE },
    { '\t',                       0, -1,  0,   CmdPlayback,   FALSE },
    { 'i',                        0, +1,  0,   CmdPlayback,   FALSE },
    { 's',                        0,  0,  0,   CmdSeeScores,  FALSE },
    { 'x',                       -1, +1,  0,   CmdKillSolution, FALSE },
    { SDLK_RETURN,               -1, -1,  0,   CmdProceed,    FALSE },
    { SDLK_KP_ENTER,             -1, -1,  0,   CmdProceed,    FALSE },
    { ' ',                       -1, -1,  0,   CmdProceed,    FALSE },
    { '.',                        0, -1,  0,   CmdProceed,    FALSE },
    { SDLK_KP_PERIOD,             0, -1,  0,   CmdProceed,    FALSE },
    { 'd',                        0,  0,  0,   CmdDebugCmd1,  FALSE },
    { 'd',                       +1,  0,  0,   CmdDebugCmd2,  FALSE },
    { 'c',                        0, +1,  0,   CmdQuit,       FALSE },
    { SDLK_BACKSLASH,             0, +1,  0,   CmdQuit,       FALSE },
    { SDLK_F4,                    0,  0, +1,   CmdQuit,       FALSE },
    { SDLK_UP,                   +1,  0,  0,   CmdCheatNorth,         TRUE },
    { SDLK_LEFT,                 +1,  0,  0,   CmdCheatWest,          TRUE },
    { SDLK_DOWN,                 +1,  0,  0,   CmdCheatSouth,         TRUE },
    { SDLK_RIGHT,                +1,  0,  0,   CmdCheatEast,          TRUE },
    { SDLK_HOME,                 +1,  0,  0,   CmdCheatHome,          FALSE },
    { SDLK_F2,                    0,  0,  0,   CmdCheatICChip,        FALSE },
    { SDLK_F3,                    0,  0,  0,   CmdCheatKeyRed,        FALSE },
    { SDLK_F4,                    0,  0,  0,   CmdCheatKeyBlue,       FALSE },
    { SDLK_F5,                    0,  0,  0,   CmdCheatKeyYellow,     FALSE },
    { SDLK_F6,                    0,  0,  0,   CmdCheatKeyGreen,      FALSE },
    { SDLK_F7,                    0,  0,  0,   CmdCheatBootsIce,      FALSE },
    { SDLK_F8,                    0,  0,  0,   CmdCheatBootsSlide,    FALSE },
    { SDLK_F9,                    0,  0,  0,   CmdCheatBootsFire,     FALSE },
    { SDLK_F10,                   0,  0,  0,   CmdCheatBootsWater,    FALSE },
#if 0
    { '\t',                       0, -1,  0,   CmdNextUndone, FALSE },
    { '\t',                      +1, -1,  0,   CmdPrevUndone, FALSE },
#endif
};

/*
 * Running the keyboard's state machine.
 */

/* Change the recorded state of a key. Shift-type keys are always
 * either on or off. The other keys can be struck, pressed,
 * re-pressed, held down, or down but ignored, as appropriate to when
 * they were first pressed and the current behavior settings.
 */
static void _keyeventcallback(int scancode, int down)
{
    switch (scancode) {
      case KMOD_LSHIFT:
      case KMOD_RSHIFT:
      case KMOD_LCTRL:
      case KMOD_RCTRL:
      case KMOD_LALT:
      case KMOD_RALT:
      case KMOD_LMETA:
      case KMOD_RMETA:
      case KMOD_NUM:
      case KMOD_CAPS:
      case KMOD_MODE:
	keystates[scancode] = down ? KS_ON : KS_OFF;
	break;
      default:
#if 0
	if (down) {
	    keystates[scancode] = keystates[scancode] == KS_OFF ? KS_PRESSED
								: KS_REPRESSED;
	} else {
	    keystates[scancode] = keystates[scancode] == KS_PRESSED ? KS_STRUCK
								    : KS_OFF;
	}
#else
	if (down) {
	    if (keystates[scancode] == KS_OFF)
		keystates[scancode] = KS_PRESSED;
	    else
		keystates[scancode] = KS_REPRESSED;
	} else {
	    if (keystates[scancode] == KS_PRESSED)
		keystates[scancode] = KS_STRUCK;
	    else
		keystates[scancode] = KS_OFF;
	}
#endif
	break;
    }
}

/* Initialize (or re-initialize) all key states.
 */
static void restartkeystates(void)
{
    Uint8      *keyboard;
    int		count, n;

    memset(keystates, FALSE, sizeof keystates);
    keyboard = SDL_GetKeyState(&count);
    if (count > SDLK_LAST)
	count = SDLK_LAST;
    for (n = 0 ; n < count ; ++n)
	if (keyboard[n])
	    _keyeventcallback(n, TRUE);
}

/* Update the key states at the start of a polling cycle.
 */
static void resetkeystates(void)
{
#if 0
    /* The transition table for resetkeystates() in joystick behavior mode.
     */
    static char const joystick_trans[KS_count] = {
	/* KS_OFF		=> */	KS_OFF,
	/* KS_ON		=> */	KS_ON,
	/* KS_DOWN		=> */	KS_DOWN,
	/* KS_STRUCK		=> */	KS_OFF,
	/* KS_PRESSED		=> */	KS_DOWN,
	/* KS_DOWNBUTOFF1	=> */	KS_DOWN,
	/* KS_DOWNBUTOFF2	=> */	KS_DOWN,
	/* KS_REPRESSED		=> */	KS_DOWN
    };
    /* The transition table for resetkeystates() in keyboard behavior mode.
     */
    static char const keyboard_trans[KS_count] = {
	/* KS_OFF		=> */	KS_OFF,
	/* KS_ON		=> */	KS_ON,
	/* KS_DOWN		=> */	KS_DOWN,
	/* KS_STRUCK		=> */	KS_OFF,
	/* KS_PRESSED		=> */	KS_DOWNBUTOFF1,
	/* KS_DOWNBUTOFF1	=> */	KS_DOWNBUTOFF2,
	/* KS_DOWNBUTOFF2	=> */	KS_DOWN,
	/* KS_REPRESSED		=> */	KS_DOWN
    };

    char       *newstate;
    int		n;

    newstate = joystickstyle ? joystick_trans : keyboard_trans;
    for (n = 0 ; n < SDLK_LAST ; ++n)
	keystates[n] = newstate[(int)keystates[n]];
#else
    int	n;

    if (joystickstyle) {
	for (n = 0 ; n < SDLK_LAST ; ++n) {
	    switch (keystates[n]) {
	      case KS_STRUCK:		keystates[n] = KS_OFF;		break;
	      case KS_PRESSED:		keystates[n] = KS_DOWN;		break;
	      case KS_DOWNBUTOFF1:	keystates[n] = KS_DOWN;		break;
	      case KS_DOWNBUTOFF2:	keystates[n] = KS_DOWN;		break;
	      case KS_REPRESSED:	keystates[n] = KS_DOWN;		break;
	    }
	}
    } else {
	for (n = 0 ; n < SDLK_LAST ; ++n) {
	    switch (keystates[n]) {
	      case KS_STRUCK:		keystates[n] = KS_OFF;		break;
	      case KS_PRESSED:		keystates[n] = KS_DOWNBUTOFF1;	break;
	      case KS_DOWNBUTOFF1:	keystates[n] = KS_DOWNBUTOFF2;	break;
	      case KS_DOWNBUTOFF2:	keystates[n] = KS_DOWN;		break;
	      case KS_REPRESSED:	keystates[n] = KS_DOWN;		break;
	    }
	}
    }
#endif
}

/*
 * Exported functions.
 */

/* Wait for any non-shift key to be pressed down. Return TRUE unless
 * the selected key suggests a desire to leave on the user's part.
 */
int anykey(void)
{
    int	n;

    resetkeystates();
    eventupdate(FALSE);
    for (;;) {
	resetkeystates();
	eventupdate(TRUE);
	for (n = 0 ; n < SDLK_LAST ; ++n)
	    if (keystates[n] == KS_STRUCK || keystates[n] == KS_PRESSED
					  || keystates[n] == KS_REPRESSED)
		goto finish;
    }
  finish:
    return n != 'q' && n != SDLK_ESCAPE;
}

/* Poll the keyboard and return the command associated with the
 * selected key, if any. If no key is selected and wait is TRUE, block
 * until a key with an associated command is selected.
 */
int input(int wait)
{
    int	lingerflag = FALSE;
    int	cmd, i, n;

    for (;;) {
	resetkeystates();
	eventupdate(wait);

	cmd = CmdNone;
	for (i = 0 ; i < (int)(sizeof keycmds / sizeof *keycmds) ; ++i) {
	    n = keystates[keycmds[i].scancode];
	    if (!n)
		continue;
	    if (keycmds[i].shift != -1)
		if (keycmds[i].shift != (keystates[SDLK_LSHIFT]
						|| keystates[SDLK_RSHIFT]))
		    continue;
	    if (keycmds[i].ctl != -1)
		if (keycmds[i].ctl != (keystates[SDLK_LCTRL]
						|| keystates[SDLK_RCTRL]))
		    continue;
	    if (keycmds[i].alt != -1)
		if (keycmds[i].alt != (keystates[SDLK_LALT]
						|| keystates[SDLK_RALT]))
		    continue;

	    if (n == KS_PRESSED || (keycmds[i].hold && n == KS_DOWN))
		return keycmds[i].cmd;
	    else if (n == KS_STRUCK || n == KS_REPRESSED)
		cmd = keycmds[i].cmd;
	    else if (n == KS_DOWNBUTOFF1 || n == KS_DOWNBUTOFF2)
		lingerflag = TRUE;
	}
	if (cmd != CmdNone || !wait)
	    break;
    }
    if (cmd == CmdNone && lingerflag)
	cmd = CmdPreserve;
    return cmd;
}

int setkeyboardrepeat(int enable)
{
    if (enable)
	return SDL_EnableKeyRepeat(500, 75) == 0;
    else
	return SDL_EnableKeyRepeat(0, 0) == 0;
}

int setkeyboardarrowsrepeat(int enable)
{
    joystickstyle = enable;
    restartkeystates();
    return TRUE;
}

/* Initialization.
 */
int _sdlinputinitialize(void)
{
    sdlg.keyeventcallbackfunc = _keyeventcallback;

    setkeyboardrepeat(TRUE);
    return TRUE;
}
