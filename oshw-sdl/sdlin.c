/* sdlin.c: Reading the keyboard.
 * 
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<string.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../defs.h"

/* Structure describing a mapping of a key event to a game command.
 */
typedef	struct keycmdmap {
    int		scancode;	/* the key's scan code */
    int		shift;		/* the shift key's state */
    int		ctl;		/* the ctrl key's state */
    int		alt;		/* the alt keys' state */
    int		cmd;		/* the command */
    int		hold;		/* TRUE for repeating joystick-mode keys */
} keycmdmap;

/* The states of keys.
 */
enum { KS_OFF = 0,		/* key is not currently pressed */
       KS_ON = 1,		/* key is down (shift-type keys only) */
       KS_DOWN,			/* key is being held down */
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
 * TRUE for keys that are to be forced to repeat. shift, ctl and alt
 * are positive if the key must be down, zero if the key must be up,
 * or negative if it doesn't matter.
 */
static keycmdmap const gamekeycmds[] = {
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
    { 'g',                        0, -1,  0,   CmdGotoLevel,  FALSE },
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
    { 'v',                       +1,  0,  0,   CmdVolumeUp,   FALSE },
    { 'v',                        0,  0,  0,   CmdVolumeDown, FALSE },
    { SDLK_RETURN,               -1, -1,  0,   CmdProceed,    FALSE },
    { SDLK_KP_ENTER,             -1, -1,  0,   CmdProceed,    FALSE },
    { ' ',                       -1, -1,  0,   CmdProceed,    FALSE },
    { 'd',                        0,  0,  0,   CmdDebugCmd1,  FALSE },
    { 'd',                       +1,  0,  0,   CmdDebugCmd2,  FALSE },
    { 'c',                        0, +1,  0,   CmdQuit,       FALSE },
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
    { 0, 0, 0, 0, 0, 0 }
};

static keycmdmap const inputkeycmds[] = {
    { SDLK_UP,                   -1, -1,  0,   CmdNorth,      FALSE },
    { SDLK_LEFT,                 -1, -1,  0,   CmdWest,       FALSE },
    { SDLK_DOWN,                 -1, -1,  0,   CmdSouth,      FALSE },
    { SDLK_RIGHT,                -1, -1,  0,   CmdEast,       FALSE },
    { '\b',                      -1, -1,  0,   CmdWest,       FALSE },
    { ' ',                       -1, -1,  0,   CmdEast,       FALSE },
    { SDLK_RETURN,               -1, -1,  0,   CmdProceed,    FALSE },
    { SDLK_KP_ENTER,             -1, -1,  0,   CmdProceed,    FALSE },
    { SDLK_ESCAPE,               -1, -1,  0,   CmdQuitLevel,  FALSE },
    { 'a',                       -1,  0,  0,   'a',           FALSE },
    { 'b',                       -1,  0,  0,   'b',           FALSE },
    { 'c',                       -1,  0,  0,   'c',           FALSE },
    { 'd',                       -1,  0,  0,   'd',           FALSE },
    { 'e',                       -1,  0,  0,   'e',           FALSE },
    { 'f',                       -1,  0,  0,   'f',           FALSE },
    { 'g',                       -1,  0,  0,   'g',           FALSE },
    { 'h',                       -1,  0,  0,   'h',           FALSE },
    { 'i',                       -1,  0,  0,   'i',           FALSE },
    { 'j',                       -1,  0,  0,   'j',           FALSE },
    { 'k',                       -1,  0,  0,   'k',           FALSE },
    { 'l',                       -1,  0,  0,   'l',           FALSE },
    { 'm',                       -1,  0,  0,   'm',           FALSE },
    { 'n',                       -1,  0,  0,   'n',           FALSE },
    { 'o',                       -1,  0,  0,   'o',           FALSE },
    { 'p',                       -1,  0,  0,   'p',           FALSE },
    { 'q',                       -1,  0,  0,   'q',           FALSE },
    { 'r',                       -1,  0,  0,   'r',           FALSE },
    { 's',                       -1,  0,  0,   's',           FALSE },
    { 't',                       -1,  0,  0,   't',           FALSE },
    { 'u',                       -1,  0,  0,   'u',           FALSE },
    { 'v',                       -1,  0,  0,   'v',           FALSE },
    { 'w',                       -1,  0,  0,   'w',           FALSE },
    { 'x',                       -1,  0,  0,   'x',           FALSE },
    { 'y',                       -1,  0,  0,   'y',           FALSE },
    { 'z',                       -1,  0,  0,   'z',           FALSE },
    { 'c',                        0, +1,  0,   CmdQuit,       FALSE },
    { SDLK_F4,                    0,  0, +1,   CmdQuit,       FALSE },
    { 0, 0, 0, 0, 0, 0 }
};

static int mergeable[CmdCount];

static keycmdmap const *keycmds = gamekeycmds;

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
	if (down) {
	    keystates[scancode] = keystates[scancode] == KS_OFF ? KS_PRESSED
								: KS_REPRESSED;
	} else {
	    keystates[scancode] = keystates[scancode] == KS_PRESSED ? KS_STRUCK
								    : KS_OFF;
	}
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
    /* The transition table for resetkeystates() in joystick behavior mode.
     */
    static char const joystick_trans[KS_count] = {
	/* KS_OFF         => */	KS_OFF,
	/* KS_ON          => */	KS_ON,
	/* KS_DOWN        => */	KS_DOWN,
	/* KS_STRUCK      => */	KS_OFF,
	/* KS_PRESSED     => */	KS_DOWN,
	/* KS_DOWNBUTOFF1 => */	KS_DOWN,
	/* KS_DOWNBUTOFF2 => */	KS_DOWN,
	/* KS_REPRESSED   => */	KS_DOWN
    };
    /* The transition table for resetkeystates() in keyboard behavior mode.
     */
    static char const keyboard_trans[KS_count] = {
	/* KS_OFF         => */	KS_OFF,
	/* KS_ON          => */	KS_ON,
	/* KS_DOWN        => */	KS_DOWN,
	/* KS_STRUCK      => */	KS_OFF,
	/* KS_PRESSED     => */	KS_DOWNBUTOFF1,
	/* KS_DOWNBUTOFF1 => */	KS_DOWNBUTOFF2,
	/* KS_DOWNBUTOFF2 => */	KS_DOWN,
	/* KS_REPRESSED   => */	KS_DOWN
    };

    char const *newstate;
    int		n;

    newstate = joystickstyle ? joystick_trans : keyboard_trans;
    for (n = 0 ; n < SDLK_LAST ; ++n)
	keystates[n] = newstate[(int)keystates[n]];
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
		return n != 'q' && n != SDLK_ESCAPE;
    }
}

/* Poll the keyboard and return the command associated with the
 * selected key, if any. If no key is selected and wait is TRUE, block
 * until a key with an associated command is selected.
 */
int input(int wait)
{
    int	lingerflag = FALSE;
    int	cmd1, cmd;
    int	i, n;

    for (;;) {
	resetkeystates();
	eventupdate(wait);

	cmd1 = cmd = 0;
	for (i = 0 ; keycmds[i].scancode ; ++i) {
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

	    if (n == KS_PRESSED || (keycmds[i].hold && n == KS_DOWN)) {
		if (!cmd1) {
		    cmd1 = keycmds[i].cmd;
		    if (!joystickstyle || !mergeable[cmd1])
			return cmd1;
		} else {
		    if ((mergeable[cmd1] & keycmds[i].cmd) == keycmds[i].cmd)
			return cmd1 | keycmds[i].cmd;
		}
	    } else if (n == KS_STRUCK || n == KS_REPRESSED) {
		cmd = keycmds[i].cmd;
	    } else if (n == KS_DOWNBUTOFF1 || n == KS_DOWNBUTOFF2) {
		lingerflag = TRUE;
	    }
	}
	if (cmd1)
	    return cmd1;
	if (cmd || !wait)
	    break;
    }
    if (!cmd && lingerflag)
	cmd = CmdPreserve;
    return cmd;
}

/* Turn key-repeating on and off.
 */
int setkeyboardrepeat(int enable)
{
    if (enable)
	return SDL_EnableKeyRepeat(500, 75) == 0;
    else
	return SDL_EnableKeyRepeat(0, 0) == 0;
}

/* Turn joystick behavior mode on or off.
 */
int setkeyboardarrowsrepeat(int enable)
{
    joystickstyle = enable;
    restartkeystates();
    return TRUE;
}

/* Turn input mode on or off.
 */
int setkeyboardinputmode(int enable)
{
    keycmds = enable ? inputkeycmds : gamekeycmds;
    return TRUE;
}

/* Initialization.
 */
int _sdlinputinitialize(void)
{
    sdlg.keyeventcallbackfunc = _keyeventcallback;

    mergeable[CmdNorth] = mergeable[CmdSouth] = CmdWest | CmdEast;
    mergeable[CmdWest] = mergeable[CmdEast] = CmdNorth | CmdSouth;

    setkeyboardrepeat(TRUE);
    return TRUE;
}

/* Online help text for the keyboard.
 */
tablespec const *keyboardhelp(int which)
{
    static char *ingame_items[] = {
	"1-arrows", "1-move Chip",
	"1-2 4 6 8 (keypad)", "1-also move Chip",
	"1-q", "1-quit the current game",
	"1-Shift-Q", "1-exit the program",
	"1-Ctrl-h (Bkspc)", "1-pause the game",
	"1-Ctrl-r", "1-restart the current level",
	"1-Ctrl-p", "1-jump to the previous level",
	"1-Ctrl-n", "1-jump to the next level",
	"1-v", "1-decrease volume",
	"1-Shift-V", "1-increase volume"
    };
    static tablespec const keyhelp_ingame = { 8, 2, 4, 1, ingame_items };

    static char *twixtgame_items[] = {
	"1-p", "1-jump to the previous level",
	"1-n", "1-jump to the next level",
	"1-PgUp", "1-skip back ten levels",
	"1-PgDn", "1-skip ahead ten levels",
	"1-g", "1-go to a level with a password",
	"1-s", "1-see the current score",
	"1-q", "1-return to the file list",
	"1-Q", "1-exit the program",
	"1-Ctrl-i (Tab)", "1-playback saved solution",
	"1-Ctrl-x", "1-replace existing solution",
	"1-v", "1-decrease volume",
	"1-Shift-V", "1-increase volume"
    };
    static tablespec const keyhelp_twixtgame = { 8, 2, 4, 1, twixtgame_items };

    static char *scroll_items[] = {
	"1-up down", "1-move selection",
	"1-PgUp PgDn", "1-scroll selection",
	"1-Enter Space", "1-select",
	"1-q", "1-cancel",
	"1-Q", "1-exit the program"
    };
    static tablespec const keyhelp_scroll = { 4, 2, 4, 1, scroll_items };

    switch (which) {
      case KEYHELP_INGAME:	return &keyhelp_ingame;
      case KEYHELP_TWIXTGAMES:	return &keyhelp_twixtgame;
      case KEYHELP_FILELIST:	return &keyhelp_scroll;
      case KEYHELP_SCORELIST:	return &keyhelp_scroll;
    }

    return NULL;
}
