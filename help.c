/* help.c: Displaying online help screens.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"defs.h"
#include	"state.h"
#include	"oshw.h"
#include	"help.h"

#define	array(a)	a, (sizeof a / sizeof *a)

/* A list of the available keyboard commands.
 */
static char const *gameplay_keys[] = {
    "\240\240\240\240During the Game\t\240",
    "arrows\tmove Chip",
    "2 4 6 8 (keypad)\talso move Chip",
    "q\tquit the current game",
    "Q\texit the program",
    "Ctrl-H (Bkspc)\tpause the game",
    "Ctrl-R\trestart the current level",
    "Ctrl-P\tjump to the previous level",
    "Ctrl-N\tjump to the next level",
    "\240\t\240",
    "\240\240\240\240Inbetween Games\t\240",
    "q\texit the program",
    "p\tjump to the previous level",
    "n\tjump to the next level",
    "s\tsee the current score",
    "Ctrl-I (Tab)\tplayback saved solution",
    "Ctrl-X\treplace existing solution"
};

/* Descriptions of the different surfaces of the levels.
 */
static objhelptext const gameplay_floors[] = {
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
static objhelptext const gameplay_walls[] = {
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
static objhelptext const gameplay_objects[] = {
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
static objhelptext const gameplay_tools[] = {
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
static objhelptext const gameplay_monsters[] = {
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

/* About this program.
 */
static char const *about[] = {
    "\267\tTile World: version " VERSION,
    "\tCopyright \251 2001 by Brian Raiter",
    "\tcompiled " __DATE__ " " __TIME__ " PST",
    "\267\tThis program is free software; you can redistribute it"
    " and/or modify it under the terms of the GNU General Public"
    " License as published by the Free Software Foundation; either"
    " version 2 of the License, or (at your option) any later version.",
    "\267\tThis program is distributed in the hope that it will be"
    " useful, but WITHOUT ANY WARRANTY; without even the implied"
    " warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."
    " See the GNU General Public License for more details.",
    "\267\tBug reports are appreciated, and can be sent to"
    " breadbox@muppetlabs.com, or posted to the"
    " annexcafe.chips.challenge newsgroup."
};

/*
 *
 */

static inline int helpscreen(int type, char const *title,
			     void const *text, int textcount, int completed)
{
    displayhelp(type, title, text, textcount, completed);
    return anykey();
}

/* Display the online help screens for the game.
 */
int gameplayhelp(void)
{
    return helpscreen(HELP_TABTEXT, "KEYS", array(gameplay_keys), +1)
	&& helpscreen(HELP_OBJECTS, "FLOORS", array(gameplay_floors), +1)
	&& helpscreen(HELP_OBJECTS, "WALLS", array(gameplay_walls), +1)
	&& helpscreen(HELP_OBJECTS, "OBJECTS", array(gameplay_objects), +1)
	&& helpscreen(HELP_OBJECTS, "TOOLS", array(gameplay_tools), +1)
	&& helpscreen(HELP_OBJECTS, "MONSTERS", array(gameplay_monsters), +1)
	&& helpscreen(HELP_TABTEXT, "ABOUT TILE WORLD", array(about), 0);
}
