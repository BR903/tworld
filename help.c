/* help.c: Displaying online help.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"defs.h"
#include	"state.h"
#include	"oshw.h"
#include	"ver.h"
#include	"help.h"

#define	array(a)	a, (sizeof a / sizeof *a)

/* Help for command-line options.
 */
static char *yowzitch_items[] = {
    "3-Usage: tworld [-hvVdlstpqH] [-DLRS DIR] [NAME] [LEVEL]",
    "1-", "1+-D", "1-Read data files from DIR instead of the default.",
    "1-", "1+-L", "1-Read level sets from DIR instead of the default.",
    "1-", "1+-R", "1-Read resource files from DIR instead of the default.",
    "1-", "1+-S", "1-Save games in DIR instead of the default.",
    "1-", "1+-p", "1-Disable password checking.",
    "1-", "1+-q", "1-Run quietly.",
    "1-", "1+-H", "1-Produce histogram of idle time upon exit.",
    "1-", "1+-l", "1-Display the list of available data files and exit.",
    "1-", "1+-s", "1-Display scores for the selected data file and exit.",
    "1-", "1+-t", "1-Display times for the selected data file and exit.",
    "1-", "1+-h", "1-Display this help and exit.",
    "1-", "1+-d", "1-Display default directories and exit.",
    "1-", "1+-v", "1-Display version number and exit.",
    "1-", "1+-V", "1-Display version and license information and exit.",
    "3-NAME specifies which data file to use.",
    "3-LEVEL specifies which level to start at."
};
static tablespec const yowzitch_table = { 17, 3, 2, -1, yowzitch_items };
tablespec const *yowzitch = &yowzitch_table;

/* Version and license information.
 */
static char *vourzhon_items[] = {
    "1+=", "1-Tile World: version " VERSION,
    "1+",  "1-Copyright (C) 2001,2002 by Brian Raiter",
    "1+",  "1-compiled " __DATE__ " " __TIME__ " PST",
    "1+=", "1-This program is free software; you can redistribute it and/or",
    "1+",  "1-modify it under the terms of the GNU General Public License as",
    "1+",  "1-published by the Free Software Foundation; either version 2 of",
    "1+",  "1-the License, or (at your option) any later version.",
    "1+=", "1-This program is distributed in the hope that it will be useful,",
    "1+",  "1-but WITHOUT ANY WARRANTY; without even the implied warranty of",
    "1+",  "1-MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the",
    "1+",  "1-GNU General Public License for more details.",
    "1+=", "1-Bug reports are appreciated, and can be sent to",
    "1+",  "1-breadbox@muppetlabs.com."
};
static tablespec const vourzhon_table = { 13, 2, 1, -1, vourzhon_items };
tablespec const *vourzhon = &vourzhon_table;

/* Descriptions of the different surfaces of the levels.
 */
static tiletablerow const help_floors[] = {
    { TRUE, Fire, 0,
      "Fire is fatal unless Chip has firewalking boots." },
    { TRUE, Water, 0,
      "Chip drowns in Water without flippers." },
    { TRUE, Ice, 0,
      "Chip slides across Ice without ice skates." },
    { TRUE, Slide_East, 0,
      "Slide floors push Chip along unless he has the suction boots." },
    { TRUE, Gravel, 0,
      "Gravel is safe to walk on, and no one but Chip can touch it." },
    { TRUE, Dirt, 0,
      "Chip is also the only one that can walk on Dirt, but it"
      " turns into a normal floor when he steps on it." }
};

/* Descriptions of the various kinds of obstacles.
 */
static tiletablerow const help_walls[] = {
    { TRUE, Wall_North, Wall,
      "Walls can either take up an entire square,"
      " or just cut off one direction." },
    { TRUE, BlueWall_Fake, 0,
      "Blue Walls can either be real walls, or just mirages. They show"
      " their true nature when Chip tries to walk through them." },
    { TRUE, Empty, 0,
      "Hidden Walls will sometimes appear when Chip runs into them,"
      " and sometimes are permanently invisible." },
    { TRUE, PopupWall, 0,
      "Popup Walls spring up when Chip walks across them, blocking retreat." },
    { TRUE, Door_Green, Door_Red,
      "Doors can be opened if Chip has a matching key." }
};

/* Descriptions of various objects to be found.
 */
static tiletablerow const help_objects[] = {
    { TRUE, Bomb, 0,
      "A Bomb is always fatal to whatever steps on it." },
    { TRUE, CloneMachine, Button_Red,
      "Clone Machines produce duplicates of whatever is shown atop them"
      " at the touch of a Red Button." },
    { TRUE, Beartrap, Button_Brown,
      "A Bear Trap holds fast whatever steps on it. A Brown Button"
      " resets the trap and permits escape." },
    { TRUE, Teleport, 0,
      "A Teleport instantly transports you to another teleport." },
    { TRUE, SwitchWall_Open, Button_Green,
      "Switching Walls come and go when any Green Button is pressed." },
    { TRUE, Button_Blue, 0,
      "A Blue Button causes tanks to turn around." },
    { TRUE, HintButton, 0,
      "The Hint Button provides a brief suggestion on how to proceed." },
    { TRUE, Burglar, 0,
      "Burglars take back all your special footgear." }
};

/* Descriptions of things that Chip can use.
 */
static tiletablerow const help_tools[] = {
    { TRUE, ICChip, 0,
      "IC Chips are what Chip needs to collect in order to pass through"
      " the socket." },
    { TRUE, Key_Green, Key_Yellow,
      "Keys permit Chip to open doors of the matching color." },
    { TRUE, Boots_Water, Boots_Fire,
      "Boots allow Chip to get past fire and water, and to traverse"
      " ice and slide floors as if they were normal floors." },
    { TRUE, Block_Static, 0,
      "Blocks are obstacles, but they can be moved. When pushed into water,"
      " the water square turns into dirt." },
    { TRUE, Socket, 0,
      "The Socket can only be passed when Chip has acquired"
      " the necessary number of IC chips." },
    { TRUE, Exit, 0,
      "The Exit takes Chip out. Finding and reaching the Exit"
      " is the main goal of every level." }
};

/* Descriptions of the roaming creatures.
 */
static tiletablerow const help_monsters[] = {
    { FALSE, Tank, 0,
      "Tanks only move in one direction, until a blue button"
      " makes them turn around." },
    { FALSE, Ball, 0,
      "Balls bounce back and forth in a straight line." },
    { FALSE, Glider, 0,
      "Gliders fly straight until they hit an obstacle,"
      " whereupon they turn left." },
    { FALSE, Fireball, 0,
      "Fireballs turn to the right upon hitting an obstacle." },
    { FALSE, Walker, 0,
      "Walkers turn in a random direction when stopped." },
    { FALSE, Bug, 0,
      "Bugs march around things, keeping the wall to their left-hand side." },
    { FALSE, Paramecium, 0,
      "Paramecia move along with the wall on their right-hand side." },
    { FALSE, Blob, 0,
      "Blobs move about completely at random, albeit slowly." },
    { FALSE, Teeth, 0,
      "Finally, Seekers home in on you; like Blobs, they can be outrun." }
};

/*
 *
 */

static int helptilescreen(char const *title, tiletablerow const *table,
			  int count, int completed)
{
    displaytiletable(title, table, count, completed);
    return anykey();
}

/* Display the online help screens for the game.
 */
int gameplayhelp(void)
{
    int	ret;

    ret  = helptilescreen("FLOORS", array(help_floors), +1)
	&& helptilescreen("WALLS", array(help_walls), +1)
	&& helptilescreen("OBJECTS", array(help_objects), +1)
	&& helptilescreen("TOOLS", array(help_tools), +1)
	&& helptilescreen("MONSTERS", array(help_monsters), 0);

    cleardisplay();
    return ret;
}

static int scrollinputcallback(int *move)
{
    switch (input(TRUE)) {
      case CmdPrev10:		*move = SCROLL_UP;		break;
      case CmdNorth:		*move = SCROLL_UP;		break;
      case CmdPrev:		*move = SCROLL_UP;		break;
      case CmdPrevLevel:	*move = SCROLL_UP;		break;
      case CmdSouth:		*move = SCROLL_DN;		break;
      case CmdNext:		*move = SCROLL_DN;		break;
      case CmdNextLevel:	*move = SCROLL_DN;		break;
      case CmdNext10:		*move = SCROLL_DN;		break;
      case CmdProceed:		*move = TRUE;			return FALSE;
      case CmdQuitLevel:	*move = FALSE;			return FALSE;
      case CmdQuit:						exit(0);
    }
    return TRUE;
}

void onlinehelp(int topic)
{
    static char *items[] = {
	"2-",
	"1+\267", "1-Return to the program",
	"1+\267", "1-Key commands during the game",
	"1+\267", "1-Key commands inbetween games",
	"1+\267", "1-Objects of the game",
	"1+\267", "1-About Tile World"
    };
    static tablespec const table = { 6, 2, 4, 1, items };

    for (;;) {
	if (!displaylist("HELP", &table, &topic, scrollinputcallback)) {
	    cleardisplay();
	    return;
	}
	switch (topic) {
	  case Help_KeysDuringGame:
	    displaytable("KEYS - DURING THE GAME",
			 keyboardhelp(KEYHELP_INGAME), -1);
	    anykey();
	    break;
	  case Help_KeysBetweenGames:
	    displaytable("KEYS - INBETWEEN GAMES",
			 keyboardhelp(KEYHELP_TWIXTGAMES), -1);
	    anykey();
	    break;
	  case Help_ObjectsOfGame:
	    (void)(helptilescreen("FLOORS", array(help_floors), +1)
		&& helptilescreen("WALLS", array(help_walls), +1)
		&& helptilescreen("OBJECTS", array(help_objects), +1)
		&& helptilescreen("TOOLS", array(help_tools), +1)
		&& helptilescreen("MONSTERS", array(help_monsters), 0));
	    break;
	  case Help_AboutGame:
	    displaytable("ABOUT TILE WORLD", &vourzhon_table, -1);
	    anykey();
	    break;
	  default:
	    cleardisplay();
	    return;
	}
    }
}
