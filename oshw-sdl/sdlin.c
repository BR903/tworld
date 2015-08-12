/* sdlin.c: Reading the keyboard.
 * 
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
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

/* Structure describing mouse activity.
 */
typedef struct mouseaction {
    int		state;		/* state of mouse action (KS_*) */
    int		x, y;		/* position of the mouse */
    int		button;		/* which button generated the event */
} mouseaction;

/* The possible states of keys.
 */
enum { KS_OFF = 0,		/* key is not currently pressed */
       KS_ON = 1,		/* key is down (shift-type keys only) */
       KS_DOWN,			/* key is being held down */
       KS_STRUCK,		/* key was pressed and released in one tick */
       KS_PRESSED,		/* key was pressed in this tick */
       KS_DOWNBUTOFF1,		/* key has been down since the previous tick */
       KS_DOWNBUTOFF2,		/* key has been down since two ticks ago */
       KS_DOWNBUTOFF3,		/* key has been down since three ticks ago */
       KS_REPEATING,		/* key is down and is now repeating */
       KS_count
};

/* The complete array of key states.
 */
static char		keystates[SDLK_LAST];

/* The last mouse action.
 */
static mouseaction	mouseinfo;

/* TRUE if direction keys are to be treated as always repeating.
 */
static int		joystickstyle = FALSE;

/* The complete list of key commands recognized by the game while
 * playing. hold is TRUE for keys that are to be forced to repeat.
 * shift, ctl and alt are positive if the key must be down, zero if
 * the key must be up, or negative if it doesn't matter.
 */
static keycmdmap const gamekeycmds[] = {        
    { SDLK_UP,                    0,  0,  0,   CmdNorth,              TRUE },
    { SDLK_LEFT,                  0,  0,  0,   CmdWest,               TRUE },
    { SDLK_DOWN,                  0,  0,  0,   CmdSouth,              TRUE },
    { SDLK_RIGHT,                 0,  0,  0,   CmdEast,               TRUE },
    { SDLK_KP8,                   0,  0,  0,   CmdNorth,              TRUE },
    { SDLK_KP4,                   0,  0,  0,   CmdWest,               TRUE },
    { SDLK_KP2,                   0,  0,  0,   CmdSouth,              TRUE },
    { SDLK_KP6,                   0,  0,  0,   CmdEast,               TRUE },
    { 'q',                        0,  0,  0,   CmdQuitLevel,          FALSE },
    { 'p',                        0, +1,  0,   CmdPrevLevel,          FALSE },
    { 'r',                        0, +1,  0,   CmdSameLevel,          FALSE },
    { 'n',                        0, +1,  0,   CmdNextLevel,          FALSE },
    { 'g',                        0, -1,  0,   CmdGotoLevel,          FALSE },
    { 'q',                       +1,  0,  0,   CmdQuit,               FALSE },
    { SDLK_PAGEUP,               -1, -1,  0,   CmdPrev10,             FALSE },
    { 'p',                        0,  0,  0,   CmdPrev,               FALSE },
    { 'r',                        0,  0,  0,   CmdSame,               FALSE },
    { 'n',                        0,  0,  0,   CmdNext,               FALSE },
    { SDLK_PAGEDOWN,             -1, -1,  0,   CmdNext10,             FALSE },
    { '\b',                      -1, -1,  0,   CmdPauseGame,          FALSE },
    { '?',			 -1, -1,  0,   CmdHelp,               FALSE },
    { SDLK_F1,                   -1, -1,  0,   CmdHelp,               FALSE },
    { 'o',			  0,  0,  0,   CmdStepping,           FALSE },
    { 'o',			 +1,  0,  0,   CmdSubStepping,        FALSE },
    { 'f',			  0,  0,  0,   CmdRndSlideDir,        FALSE },
    { '\t',                       0, -1,  0,   CmdPlayback,           FALSE },
    { '\t',                      +1, -1,  0,   CmdCheckSolution,      FALSE },
    { 'x',                        0, +1,  0,   CmdReplSolution,       FALSE },
    { 'x',                       +1, +1,  0,   CmdKillSolution,       FALSE },
    { 's',                        0,  0,  0,   CmdSeeScores,          FALSE },
    { 's',			  0, +1,  0,   CmdSeeSolutionFiles,   FALSE },
    { 'v',                       +1,  0,  0,   CmdVolumeUp,           FALSE },
    { 'v',                        0,  0,  0,   CmdVolumeDown,         FALSE },
    { SDLK_RETURN,               -1, -1,  0,   CmdProceed,            FALSE },
    { SDLK_KP_ENTER,             -1, -1,  0,   CmdProceed,            FALSE },
    { ' ',                       -1, -1,  0,   CmdProceed,            FALSE },
    { 'd',                        0,  0,  0,   CmdDebugCmd1,          FALSE },
    { 'd',                       +1,  0,  0,   CmdDebugCmd2,          FALSE },
    { SDLK_UP,                   +1,  0,  0,   CmdCheatNorth,         TRUE },
    { SDLK_LEFT,                 +1,  0,  0,   CmdCheatWest,          TRUE },
    { SDLK_DOWN,                 +1,  0,  0,   CmdCheatSouth,         TRUE },
    { SDLK_RIGHT,                +1,  0,  0,   CmdCheatEast,          TRUE },
    { SDLK_HOME,                 +1,  0,  0,   CmdCheatHome,          FALSE },
    { SDLK_F10,                   0,  0,  0,   CmdCheatStuff,         FALSE },
    { '\003',                    -1, -1,  0,   CmdQuit,               FALSE },
    { SDLK_F4,                    0,  0, +1,   CmdQuit,               FALSE },
    { 0, 0, 0, 0, 0, 0 }
};

/* The list of key commands recognized when the program is obtaining
 * input from the user.
 */
static keycmdmap const inputkeycmds[] = {
    { SDLK_UP,                   -1, -1,  0,   CmdNorth,              FALSE },
    { SDLK_LEFT,                 -1, -1,  0,   CmdWest,               FALSE },
    { SDLK_DOWN,                 -1, -1,  0,   CmdSouth,              FALSE },
    { SDLK_RIGHT,                -1, -1,  0,   CmdEast,               FALSE },
    { '\b',                      -1, -1,  0,   CmdWest,               FALSE },
    { ' ',                       -1, -1,  0,   CmdEast,               FALSE },
    { SDLK_RETURN,               -1, -1,  0,   CmdProceed,            FALSE },
    { SDLK_KP_ENTER,             -1, -1,  0,   CmdProceed,            FALSE },
    { SDLK_ESCAPE,               -1, -1,  0,   CmdQuitLevel,          FALSE },
    { 'a',                       -1,  0,  0,   'a',                   FALSE },
    { 'b',                       -1,  0,  0,   'b',                   FALSE },
    { 'c',                       -1,  0,  0,   'c',                   FALSE },
    { 'd',                       -1,  0,  0,   'd',                   FALSE },
    { 'e',                       -1,  0,  0,   'e',                   FALSE },
    { 'f',                       -1,  0,  0,   'f',                   FALSE },
    { 'g',                       -1,  0,  0,   'g',                   FALSE },
    { 'h',                       -1,  0,  0,   'h',                   FALSE },
    { 'i',                       -1,  0,  0,   'i',                   FALSE },
    { 'j',                       -1,  0,  0,   'j',                   FALSE },
    { 'k',                       -1,  0,  0,   'k',                   FALSE },
    { 'l',                       -1,  0,  0,   'l',                   FALSE },
    { 'm',                       -1,  0,  0,   'm',                   FALSE },
    { 'n',                       -1,  0,  0,   'n',                   FALSE },
    { 'o',                       -1,  0,  0,   'o',                   FALSE },
    { 'p',                       -1,  0,  0,   'p',                   FALSE },
    { 'q',                       -1,  0,  0,   'q',                   FALSE },
    { 'r',                       -1,  0,  0,   'r',                   FALSE },
    { 's',                       -1,  0,  0,   's',                   FALSE },
    { 't',                       -1,  0,  0,   't',                   FALSE },
    { 'u',                       -1,  0,  0,   'u',                   FALSE },
    { 'v',                       -1,  0,  0,   'v',                   FALSE },
    { 'w',                       -1,  0,  0,   'w',                   FALSE },
    { 'x',                       -1,  0,  0,   'x',                   FALSE },
    { 'y',                       -1,  0,  0,   'y',                   FALSE },
    { 'z',                       -1,  0,  0,   'z',                   FALSE },
    { '\003',                    -1, -1,  0,   CmdQuit,               FALSE },
    { SDLK_F4,                    0,  0, +1,   CmdQuit,               FALSE },
    { 0, 0, 0, 0, 0, 0 }
};

/* The current map of key commands.
 */
static keycmdmap const *keycmds = gamekeycmds;

/* A map of keys that can be held down simultaneously to produce
 * multiple commands.
 */
static int mergeable[CmdKeyMoveLast + 1];

/*
 * Running the keyboard's state machine.
 */

/* This callback is called whenever the state of any keyboard key
 * changes. It records this change in the keystates array. The key can
 * be recorded as being struck, pressed, repeating, held down, or down
 * but ignored, as appropriate to when they were first pressed and the
 * current behavior settings. Shift-type keys are always either on or
 * off.
 */
static void _keyeventcallback(int scancode, int down)
{
    switch (scancode) {
      case SDLK_LSHIFT:
      case SDLK_RSHIFT:
      case SDLK_LCTRL:
      case SDLK_RCTRL:
      case SDLK_LALT:
      case SDLK_RALT:
      case SDLK_LMETA:
      case SDLK_RMETA:
      case SDLK_NUMLOCK:
      case SDLK_CAPSLOCK:
      case SDLK_MODE:
	keystates[scancode] = down ? KS_ON : KS_OFF;
	break;
      default:
	if (scancode < SDLK_LAST) {
	    if (down) {
		keystates[scancode] = keystates[scancode] == KS_OFF ?
						KS_PRESSED : KS_REPEATING;
	    } else {
		keystates[scancode] = keystates[scancode] == KS_PRESSED ?
						KS_STRUCK : KS_OFF;
	    }
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

    memset(keystates, KS_OFF, sizeof keystates);
    keyboard = SDL_GetKeyState(&count);
    if (count > SDLK_LAST)
	count = SDLK_LAST;
    for (n = 0 ; n < count ; ++n)
	if (keyboard[n])
	    _keyeventcallback(n, TRUE);
}

/* Update the key states. This is done at the start of each polling
 * cycle. The state changes that occur depend on the current behavior
 * settings.
 */
static void resetkeystates(void)
{
    /* The transition table for keys in joystick behavior mode.
     */
    static char const joystick_trans[KS_count] = {
	/* KS_OFF         => */	KS_OFF,
	/* KS_ON          => */	KS_ON,
	/* KS_DOWN        => */	KS_DOWN,
	/* KS_STRUCK      => */	KS_OFF,
	/* KS_PRESSED     => */	KS_DOWN,
	/* KS_DOWNBUTOFF1 => */	KS_DOWN,
	/* KS_DOWNBUTOFF2 => */	KS_DOWN,
	/* KS_DOWNBUTOFF3 => */	KS_DOWN,
	/* KS_REPEATING   => */	KS_DOWN
    };
    /* The transition table for keys in keyboard behavior mode.
     */
    static char const keyboard_trans[KS_count] = {
	/* KS_OFF         => */	KS_OFF,
	/* KS_ON          => */	KS_ON,
	/* KS_DOWN        => */	KS_DOWN,
	/* KS_STRUCK      => */	KS_OFF,
	/* KS_PRESSED     => */	KS_DOWNBUTOFF1,
	/* KS_DOWNBUTOFF1 => */	KS_DOWNBUTOFF2,
	/* KS_DOWNBUTOFF2 => */	KS_DOWN,
	/* KS_DOWNBUTOFF3 => */	KS_DOWN,
	/* KS_REPEATING   => */	KS_DOWN
    };

    char const *newstate;
    int		n;

    newstate = joystickstyle ? joystick_trans : keyboard_trans;
    for (n = 0 ; n < SDLK_LAST ; ++n)
	keystates[n] = newstate[(int)keystates[n]];
}

/*
 * Mouse event functions.
 */

/* This callback is called whenever there is a state change in the
 * mouse buttons. Up events are ignored. Down events are stored to
 * be examined later.
 */
static void _mouseeventcallback(int xpos, int ypos, int button, int down)
{
    if (down) {
	mouseinfo.state = KS_PRESSED;
	mouseinfo.x = xpos;
	mouseinfo.y = ypos;
	mouseinfo.button = button;
    }
}

/* Return the command appropriate to the most recent mouse activity.
 */
static int retrievemousecommand(void)
{
    int	n;

    switch (mouseinfo.state) {
      case KS_PRESSED:
	mouseinfo.state = KS_OFF;
	if (mouseinfo.button == SDL_BUTTON_WHEELDOWN)
	    return CmdNext;
	if (mouseinfo.button == SDL_BUTTON_WHEELUP)
	    return CmdPrev;
	if (mouseinfo.button == SDL_BUTTON_LEFT) {
	    n = windowmappos(mouseinfo.x, mouseinfo.y);
	    if (n >= 0) {
		mouseinfo.state = KS_DOWNBUTOFF1;
		return CmdAbsMouseMoveFirst + n;
	    }
	}
	break;
      case KS_DOWNBUTOFF1:
	mouseinfo.state = KS_DOWNBUTOFF2;
	return CmdPreserve;
      case KS_DOWNBUTOFF2:
	mouseinfo.state = KS_DOWNBUTOFF3;
	return CmdPreserve;
      case KS_DOWNBUTOFF3:
	mouseinfo.state = KS_OFF;
	return CmdPreserve;
    }
    return 0;
}

/*
 * Exported functions.
 */

/* Wait for any non-shift key to be pressed down, ignoring any keys
 * that may be down at the time the function is called. Return FALSE
 * if the key pressed is suggestive of a desire to quit.
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
					  || keystates[n] == KS_REPEATING)
		return n != 'q' && n != SDLK_ESCAPE;
    }
}

/* Poll the keyboard and return the command associated with the
 * selected key, if any. If no key is selected and wait is TRUE, block
 * until a key with an associated command is selected. In keyboard behavior
 * mode, the function can return CmdPreserve, indicating that if the key
 * command from the previous poll has not been processed, it should still
 * be considered active. If two mergeable keys are selected, the return
 * value will be the bitwise-or of their command values.
 */
int input(int wait)
{
    keycmdmap const    *kc;
    int			lingerflag = FALSE;
    int			cmd1, cmd, n;

    for (;;) {
	resetkeystates();
	eventupdate(wait);

	cmd1 = cmd = 0;
	for (kc = keycmds ; kc->scancode ; ++kc) {
	    n = keystates[kc->scancode];
	    if (!n)
		continue;
	    if (kc->shift != -1)
		if (kc->shift !=
			(keystates[SDLK_LSHIFT] || keystates[SDLK_RSHIFT]))
		    continue;
	    if (kc->ctl != -1)
		if (kc->ctl !=
			(keystates[SDLK_LCTRL] || keystates[SDLK_RCTRL]))
		    continue;
	    if (kc->alt != -1)
		if (kc->alt != (keystates[SDLK_LALT] || keystates[SDLK_RALT]))
		    continue;

	    if (n == KS_PRESSED || (kc->hold && n == KS_DOWN)) {
		if (!cmd1) {
		    cmd1 = kc->cmd;
		    if (!joystickstyle || cmd1 > CmdKeyMoveLast
				       || !mergeable[cmd1])
			return cmd1;
		} else {
		    if (cmd1 <= CmdKeyMoveLast
				&& (mergeable[cmd1] & kc->cmd) == kc->cmd)
			return cmd1 | kc->cmd;
		}
	    } else if (n == KS_STRUCK || n == KS_REPEATING) {
		cmd = kc->cmd;
	    } else if (n == KS_DOWNBUTOFF1 || n == KS_DOWNBUTOFF2) {
		lingerflag = TRUE;
	    }
	}
	if (cmd1)
	    return cmd1;
	if (cmd)
	    return cmd;
	cmd = retrievemousecommand();
	if (cmd)
	    return cmd;
	if (!wait)
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

/* Turn joystick behavior mode on or off. In joystick-behavior mode,
 * the arrow keys are always returned from input() if they are down at
 * the time of the polling cycle. Other keys are only returned if they
 * are pressed during a polling cycle (or if they repeat, if keyboard
 * repeating is on). In keyboard-behavior mode, the arrow keys have a
 * special repeating behavior that is kept synchronized with the
 * polling cycle.
 */
int setkeyboardarrowsrepeat(int enable)
{
    joystickstyle = enable;
    restartkeystates();
    return TRUE;
}

/* Turn input mode on or off. When input mode is on, the input key
 * command map is used instead of the game key command map.
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
    sdlg.mouseeventcallbackfunc = _mouseeventcallback;

    mergeable[CmdNorth] = mergeable[CmdSouth] = CmdWest | CmdEast;
    mergeable[CmdWest] = mergeable[CmdEast] = CmdNorth | CmdSouth;

    setkeyboardrepeat(TRUE);
    SDL_EnableUNICODE(TRUE);
    return TRUE;
}

/* Online help texts for the keyboard commands.
 */
tablespec const *keyboardhelp(int which)
{
    static char *ingame_items[] = {
	"1-arrows", "1-move Chip",
	"1-2 4 6 8 (keypad)", "1-also move Chip",
	"1-Q", "1-quit the current game",
	"1-Bkspc", "1-pause the game",
	"1-Ctrl-R", "1-restart the current level",
	"1-Ctrl-P", "1-jump to the previous level",
	"1-Ctrl-N", "1-jump to the next level",
	"1-V", "1-decrease volume",
	"1-Shift-V", "1-increase volume",
	"1-Ctrl-C", "1-exit the program",
	"1-Alt-F4", "1-exit the program"
    };
    static tablespec const keyhelp_ingame = { 11, 2, 4, 1, ingame_items };

    static char *twixtgame_items[] = {
	"1-P", "1-jump to the previous level",
	"1-N", "1-jump to the next level",
	"1-PgUp", "1-skip back ten levels",
	"1-PgDn", "1-skip ahead ten levels",
	"1-G", "1-go to a level using a password",
	"1-S", "1-see the scores for each level",
	"1-Tab", "1-playback saved solution",
	"1-Shift-Tab", "1-verify saved solution",
	"1-Ctrl-X", "1-replace existing solution",
	"1-Shift-Ctrl-X", "1-delete existing solution",
	"1-Ctrl-S", "1-see the available solution files",
	"1-O", "1-toggle between even-step and odd-step offset",
	"1-Shift-O", "1-increment stepping offset (Lynx only)",
	"1-V", "1-decrease volume",
	"1-Shift-V", "1-increase volume",
	"1-Q", "1-return to the file list",
	"1-Ctrl-C", "1-exit the program",
	"1-Alt-F4", "1-exit the program"
    };
    static tablespec const keyhelp_twixtgame = { 18, 2, 2, 1,
						 twixtgame_items };

    static char *scorelist_items[] = {
	"1-up down", "1-move selection",
	"1-PgUp PgDn", "1-scroll selection",
	"1-Enter Space", "1-select level",
	"1-Ctrl-S", "1-change solution file",
	"1-Q", "1-return to the last level",
	"1-Ctrl-C", "1-exit the program",
	"1-Alt-F4", "1-exit the program"
    };
    static tablespec const keyhelp_scorelist = { 7, 2, 2, 1, scorelist_items };

    static char *scroll_items[] = {
	"1-up down", "1-move selection",
	"1-PgUp PgDn", "1-scroll selection",
	"1-Enter Space", "1-select",
	"1-Q", "1-cancel",
	"1-Ctrl-C", "1-exit the program",
	"1-Alt-F4", "1-exit the program"
    };
    static tablespec const keyhelp_scroll = { 6, 2, 2, 1, scroll_items };

    switch (which) {
      case KEYHELP_INGAME:	return &keyhelp_ingame;
      case KEYHELP_TWIXTGAMES:	return &keyhelp_twixtgame;
      case KEYHELP_SCORELIST:	return &keyhelp_scorelist;
      case KEYHELP_FILELIST:	return &keyhelp_scroll;
    }

    return NULL;
}
