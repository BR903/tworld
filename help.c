/* help.c: The online help screens
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"gen.h"
#include	"objects.h"
#include	"userin.h"
#include	"userout.h"

static objhelptext const floors[] = {
    { TRUE, Fire, 0,
      "Fire is fatal unless Chip has firewalking boots" },
    { TRUE, Water, 0,
      "Chip drowns in Water without flippers" },
    { TRUE, Ice, 0,
      "Chip slides across Ice without ice skates" },
    { TRUE, Slide_East, 0,
      "Slide floors push Chip along unless he has the suction boots" },
    { TRUE, Gravel, 0,
      "Not only is Gravel safe, no one but Chip can walk on it" },
    { TRUE, Dirt, 0,
      "Chip is also the only one that can walk in Dirt, but it"
      " turns into a normal floor when he steps on it" }
};

static objhelptext const walls[] = {
    { TRUE, Wall, Wall_East,
      "Walls can either take up an entire square,"
      " or just cut off one direction" },
    { TRUE, BlueWall_Fake, 0,
      "Blue Walls can either be real walls, or just mirages. They show"
      " their true nature when Chip tries to walk through them" },
    { TRUE, Empty, 0,
      "Hidden Walls will sometimes appear when Chip runs into them,"
      " and sometimes are permanently invisible" },
    { TRUE, PopupWall, 0,
      "Popup Walls spring up when Chip walks across them, blocking retreat" },
    { TRUE, Door_Green, Door_Red,
      "Doors can be opened if Chip has a matching key" }
};

static objhelptext const objects[] = {
    { TRUE, Bomb, 0,
      "A Bomb is always fatal to whatever steps on it" },
    { TRUE, CloneMachine, Button_Red,
      "Clone Machines produce duplicates of whatever is shown atop them"
      " at the touch of a Red Button" },
    { TRUE, Beartrap, Button_Brown,
      "A Bear Trap holds fast whatever steps on it; a Brown Button"
      " resets the trap and permits escape" },
    { TRUE, Teleport, 0,
      "A Teleport instantly transports you to another teleport" },
    { TRUE, SwitchWall_Open, Button_Green,
      "Switching Walls come and go when any Green Button is pressed" },
    { TRUE, Button_Blue, 0,
      "A Blue Button causes tanks to turn around" },
    { TRUE, HintButton, 0,
      "The Hint Button provides a brief suggestion on how to proceed" },
    { TRUE, Burglar, 0,
      "Burglars take back all your special footgear" }
};

static objhelptext const tools[] = {
    { TRUE, ICChip, 0,
      "IC Chips are what Chip needs to collect"
      " in order to pass through the socket" },
    { TRUE, Key_Green, Key_Yellow,
      "Keys permit Chip to open doors of the matching color" },
    { TRUE, Boots_Water, Boots_Fire,
      "Boots allow Chip to get past fire and water, and to traverse"
      " ice and slide floors as if they were normal floors" },
    { FALSE, Block, 0,
      "Blocks can be obstacles, but they can also be pushed into the water,"
      " turning the water square into dirt" },
    { TRUE, Socket, 0,
      "The Socket can only be passed when Chip has acquired"
      " the necessary number of IC chips" },
    { TRUE, Exit, 0,
      "The Exit takes Chip out; finding and reaching the Exit"
      " is the main goal of every level" }
};

static objhelptext const monsters[] = {
    { FALSE, Tank, 0,
      "Tanks only move in one direction, until a blue button"
      " makes them turn around" },
    { FALSE, Ball, 0,
      "Balls bounce back and forth in a straight line" },
    { FALSE, Glider, 0,
      "Gliders fly straight until they hit an obstacle,"
      " whereupon they turn left" },
    { FALSE, Fireball, 0,
      "Fireballs turn to the right upon hitting an obstacle" },
    { FALSE, Walker, 0,
      "Walkers turn in a random direction when stopped" },
    { FALSE, Bug, 0,
      "Bugs march around things, keeping the wall to their left-hand side" },
    { FALSE, Paramecium, 0,
      "Paramecia move along with the wall on their right-hand side" },
    { FALSE, Blob, 0,
      "Blobs move about completely at random" },
    { FALSE, Teeth, 0,
      "Finally, Seekers home in on you; unlike the others,"
      " they can be outrun" }
};

/*
 *
 */

void runhelp(void)
{
    displayhelp("FLOORS", floors, sizeof floors / sizeof *floors);
    anykey();
    displayhelp("WALLS", walls, sizeof walls / sizeof *walls);
    anykey();
    displayhelp("OBJECTS", objects, sizeof objects / sizeof *objects);
    anykey();
    displayhelp("TOOLS & MACHINES", tools, sizeof tools / sizeof *tools);
    anykey();
    displayhelp("MONSTERS", monsters, sizeof monsters / sizeof *monsters);
    anykey();
}
