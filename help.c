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

/* A list of the available keyboard commands.
 */
static char const *keys[] = {
    "\240\tDuring the Game",
    "\240\t\240",
    "arrows\tmove Chip",
    "2 4 6 8 (keypad)\talso move Chip",
    "q\tquit the current game",
    "Q\texit the program",
    "Ctrl-H (Bkspc)\tpause the game",
    "Ctrl-R\trestart the current level",
    "Ctrl-P\tjump to the previous level",
    "Ctrl-N\tjump to the next level",
    "\240\t\240",
    "\240\tInbetween Games",
    "\240\t\240",
    "q\texit the program",
    "p\tjump to the previous level",
    "n\tjump to the next level",
    "s\tsee the current score",
    "Ctrl-I (Tab)\tplayback saved solution",
};

/* Descriptions of the different surfaces of the levels.
 */
static objhelptext const floors[] = {
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
static objhelptext const walls[] = {
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
static objhelptext const objects[] = {
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
static objhelptext const tools[] = {
    { TRUE, ICChip, 0,
      "IC Chips are what Chip needs to collect in order to pass through"
      " the socket." },
    { TRUE, Key_Green, Key_Yellow,
      "Keys permit Chip to open doors of the matching color." },
    { TRUE, Boots_Water, Boots_Fire,
      "Boots allow Chip to get past fire and water, and to traverse"
      " ice and slide floors as if they were normal floors." },
    { TRUE, Block_Emergent, 0,
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
static objhelptext const monsters[] = {
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
    "\267\tTileWorld",
    "\tversion 0.5.1 (alpha)",
    "\tCopyright \251 2001 by Brian Raiter",
    "\tcompiled " __DATE__ " " __TIME__,
    "\240\t\240",
    "\267\tThis program is free software; you can redistribute it and/or",
    "\tmodify it under the terms of the GNU General Public License as",
    "\tpublished by the Free Software Foundation; either version 2 of",
    "\tthe License, or (at your option) any later version.",
    "\240\t\240",
    "\267\tThis program is distributed in the hope that it will be useful,",
    "\tbut WITHOUT ANY WARRANTY; without even the implied warranty of",
    "\tMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the",
    "\tGNU General Public License for more details.",
    "\240\t\240",
    "\267\t(The author requests that you voluntarily refrain from",
    "\tdistributing this particular version of the software, as it's",
    "\tstill pretty messy and would make me look dumb. Bug reports",
    "\tare appreciated, and can be sent to breadbox@muppetlabs.com,",
    "\tor posted to the annexcafe.chips.challenge newsgroup in an",
    "\tappropriate fashion.)"
};

/* Display the online help screens.
 */
void runhelp(void)
{
    displayhelp(HELP_TABTEXT, "KEYS",
		keys, sizeof keys / sizeof *keys);
    if (!anykey())
	return;
    displayhelp(HELP_OBJECTS, "FLOORS",
		floors, sizeof floors / sizeof *floors);
    if (!anykey())
	return;
    displayhelp(HELP_OBJECTS, "WALLS",
		walls, sizeof walls / sizeof *walls);
    if (!anykey())
	return;
    displayhelp(HELP_OBJECTS, "OBJECTS & MACHINES",
		objects, sizeof objects / sizeof *objects);
    if (!anykey())
	return;
    displayhelp(HELP_OBJECTS, "TOOLS",
		tools, sizeof tools / sizeof *tools);
    if (!anykey())
	return;
    displayhelp(HELP_OBJECTS, "MONSTERS",
		monsters, sizeof monsters / sizeof *monsters);
    if (!anykey())
	return;

    displayhelp(HELP_TABTEXT, "ABOUT TILEWORLD",
		about, sizeof about / sizeof *about);
    anykey();
}
