/* logic.c: The game logic
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<assert.h>
#include	"gen.h"
#include	"cc.h"
#include	"objects.h"
#include	"state.h"
#include	"random.h"
#include	"logic.h"

/*
 * Game state accessor macros.
 */

static gamestate *state;

#define	setstate(p)		(state = (p))

#define	chippos()		(state->currpos)
#define	chipdir()		(state->currdir)
#define	chipsneeded()		(state->chipsneeded)
#define	floorat(pos)		(state->map[pos].floor)
#define	iscreatureat(pos)	(state->map[pos].entity)
#define	isoccupied(pos)		(iscreatureat(pos) || (pos) == chippos())
#define isblockat(pos)		((state->map[pos].entity >> 2) == Block)
#define	setcreatureat(pos, id, dir) \
    (state->map[pos].entity = (((id) << 2) | ((uchar)(dir) == (uchar)NIL ? 0 : (dir))))

#define	isbuttondown(pos)	(state->map[pos].state & 0x10)
#define	pushbutton(pos)		(state->map[pos].state |= 0x10)
#define	resetbutton(pos)	(state->map[pos].state &= ~0x10)

#define	clonerlist()		(state->game->cloners)
#define	clonerlistsize()	(state->game->clonercount)
#define	traplist()		(state->game->traps)
#define	traplistsize()		(state->game->trapcount)
#define	creaturelist()		(state->creatures)

#define	currentinput()		(state->currentinput)
#define	displayflags()		(state->displayflags)
#define	completed()		(state->completed)

#define	addsoundeffect(str)	(state->soundeffect = (str))

#define	possession(obj)	(*_possession(obj))
static short *_possession(int obj)
{
    switch (obj) {
      case Key_Blue:		return &state->keys[0];
      case Key_Green:		return &state->keys[1];
      case Key_Red:		return &state->keys[2];
      case Key_Yellow:		return &state->keys[3];
      case Boots_Slide:		return &state->boots[0];
      case Boots_Ice:		return &state->boots[1];
      case Boots_Water:		return &state->boots[2];
      case Boots_Fire:		return &state->boots[3];
      case Door_Blue:		return &state->keys[0];
      case Door_Green:		return &state->keys[1];
      case Door_Red:		return &state->keys[2];
      case Door_Yellow:		return &state->keys[3];
      case Slide_North:		return &state->boots[0];
      case Slide_West:		return &state->boots[0];
      case Slide_South:		return &state->boots[0];
      case Slide_East:		return &state->boots[0];
      case Slide_Random:	return &state->boots[0];
      case Ice:			return &state->boots[1];
      case IceWall_Northwest:	return &state->boots[1];
      case IceWall_Northeast:	return &state->boots[1];
      case IceWall_Southwest:	return &state->boots[1];
      case IceWall_Southeast:	return &state->boots[1];
      case Water:		return &state->boots[2];
      case Fire:		return &state->boots[3];
    }
    die("Invalid object %d handed to possession()\n", obj);
    return NULL;
}

/*
 * Translating the map in the data file.
 */

static struct { int isfloor:1, id:12, dir:3; } const fileids[] = {
/* 00 empty space		*/	{ TRUE,  Empty,		    NIL },
/* 01 wall			*/	{ TRUE,  Wall,		    NIL },
/* 02 chip			*/	{ TRUE,  ICChip,	    NIL },
/* 03 water			*/	{ TRUE,  Water,		    NIL },
/* 04 fire			*/	{ TRUE,  Fire,		    NIL },
/* 05 invisible wall, perm.	*/	{ TRUE,  HiddenWall_Perm,   NIL },
/* 06 blocked north		*/	{ TRUE,  Wall_North,	    NIL },
/* 07 blocked west		*/	{ TRUE,  Wall_West,	    NIL },
/* 08 blocked south		*/	{ TRUE,  Wall_South,	    NIL },
/* 09 blocked east		*/	{ TRUE,  Wall_East,	    NIL },
/* 0A block			*/	{ FALSE, Block,		    NIL },
/* 0B dirt			*/	{ TRUE,  Dirt,		    NIL },
/* 0C ice			*/	{ TRUE,  Ice,		    NIL },
/* 0D force south		*/	{ TRUE,  Slide_South,	    NIL },
/* 0E cloning block north	*/	{ FALSE, Block,		    NORTH },
/* 0F cloning block west	*/	{ FALSE, Block,		    WEST },
/* 10 cloning block south	*/	{ FALSE, Block,		    SOUTH },
/* 11 cloning block east	*/	{ FALSE, Block,		    EAST },
/* 12 force north		*/	{ TRUE,  Slide_North,	    NIL },
/* 13 force east		*/	{ TRUE,  Slide_East,	    NIL },
/* 14 force west		*/	{ TRUE,  Slide_West,	    NIL },
/* 15 exit			*/	{ TRUE,  Exit,		    NIL },
/* 16 blue door			*/	{ TRUE,  Door_Blue,	    NIL },
/* 17 red door			*/	{ TRUE,  Door_Red,	    NIL },
/* 18 green door		*/	{ TRUE,  Door_Green,	    NIL },
/* 19 yellow door		*/	{ TRUE,  Door_Yellow,	    NIL },
/* 1A SE ice slide		*/	{ TRUE,  IceWall_Southeast, NIL },
/* 1B SW ice slide		*/	{ TRUE,  IceWall_Southwest, NIL },
/* 1C NW ice slide		*/	{ TRUE,  IceWall_Northwest, NIL },
/* 1D NE ice slide		*/	{ TRUE,  IceWall_Northeast, NIL },
/* 1E blue block, tile		*/	{ TRUE,  BlueWall_Fake,	    NIL },
/* 1F blue block, wall		*/	{ TRUE,  BlueWall_Real,	    NIL },
/* 20 not used			*/	{ TRUE,  NIL,		    NIL },
/* 21 thief			*/	{ TRUE,  Burglar,	    NIL },
/* 22 socket			*/	{ TRUE,  Socket,	    NIL },
/* 23 green button		*/	{ TRUE,  Button_Green,	    NIL },
/* 24 red button		*/	{ TRUE,  Button_Red,	    NIL },
/* 25 switch block, closed	*/	{ TRUE,  SwitchWall_Closed, NIL },
/* 26 switch block, open	*/	{ TRUE,  SwitchWall_Open,   NIL },
/* 27 brown button		*/	{ TRUE,  Button_Brown,	    NIL },
/* 28 blue button		*/	{ TRUE,  Button_Blue,	    NIL },
/* 29 teleport			*/	{ TRUE,  Teleport,	    NIL },
/* 2A bomb			*/	{ TRUE,  Bomb,		    NIL },
/* 2B trap			*/	{ TRUE,  Beartrap,	    NIL },
/* 2C invisible wall, temp.	*/	{ TRUE,  HiddenWall_Temp,   NIL },
/* 2D gravel			*/	{ TRUE,  Gravel,	    NIL },
/* 2E pass once			*/	{ TRUE,  PopupWall,	    NIL },
/* 2F hint			*/	{ TRUE,  HintButton,	    NIL },
/* 30 blocked SE		*/	{ TRUE,  Wall_Southeast,    NIL },
/* 31 cloning machine		*/	{ TRUE,  CloneMachine,	    NIL },
/* 32 force all directions	*/	{ TRUE,  Slide_Random,	    NIL },
/* 33 drowning Chip		*/	{ TRUE,  NIL,		    NIL },
/* 34 burned Chip		*/	{ TRUE,  NIL,		    NIL },
/* 35 burned Chip		*/	{ TRUE,  NIL,		    NIL },
/* 36 not used			*/	{ TRUE,  NIL,		    NIL },
/* 37 not used			*/	{ TRUE,  NIL,		    NIL },
/* 38 not used			*/	{ TRUE,  NIL,		    NIL },
/* 39 Chip in exit		*/	{ TRUE,  NIL,		    NIL },
/* 3A exit - end game		*/	{ TRUE,  Exit,		    NIL },
/* 3B exit - end game		*/	{ TRUE,  Exit,		    NIL },
/* 3C Chip swimming north	*/	{ TRUE,  NIL,		    NIL },
/* 3D Chip swimming west	*/	{ TRUE,  NIL,		    NIL },
/* 3E Chip swimming south	*/	{ TRUE,  NIL,		    NIL },
/* 3F Chip swimming east	*/	{ TRUE,  NIL,		    NIL },
/* 40 Bug N			*/	{ FALSE, Bug,		    NORTH },
/* 41 Bug W			*/	{ FALSE, Bug,		    WEST },
/* 42 Bug S			*/	{ FALSE, Bug,		    SOUTH },
/* 43 Bug E			*/	{ FALSE, Bug,		    EAST },
/* 44 Fireball N		*/	{ FALSE, Fireball,	    NORTH },
/* 45 Fireball W		*/	{ FALSE, Fireball,	    WEST },
/* 46 Fireball S		*/	{ FALSE, Fireball,	    SOUTH },
/* 47 Fireball E		*/	{ FALSE, Fireball,	    EAST },
/* 48 Pink ball N		*/	{ FALSE, Ball,		    NORTH },
/* 49 Pink ball W		*/	{ FALSE, Ball,		    WEST },
/* 4A Pink ball S		*/	{ FALSE, Ball,		    SOUTH },
/* 4B Pink ball E		*/	{ FALSE, Ball,		    EAST },
/* 4C Tank N			*/	{ FALSE, Tank,		    NORTH },
/* 4D Tank W			*/	{ FALSE, Tank,		    WEST },
/* 4E Tank S			*/	{ FALSE, Tank,		    SOUTH },
/* 4F Tank E			*/	{ FALSE, Tank,		    EAST },
/* 50 Glider N			*/	{ FALSE, Glider,	    NORTH },
/* 51 Glider W			*/	{ FALSE, Glider,	    WEST },
/* 52 Glider S			*/	{ FALSE, Glider,	    SOUTH },
/* 53 Glider E			*/	{ FALSE, Glider,	    EAST },
/* 54 Teeth N			*/	{ FALSE, Teeth,		    NORTH },
/* 55 Teeth W			*/	{ FALSE, Teeth,		    WEST },
/* 56 Teeth S			*/	{ FALSE, Teeth,		    SOUTH },
/* 57 Teeth E			*/	{ FALSE, Teeth,		    EAST },
/* 58 Walker N			*/	{ FALSE, Walker,	    NORTH },
/* 59 Walker W			*/	{ FALSE, Walker,	    WEST },
/* 5A Walker S			*/	{ FALSE, Walker,	    SOUTH },
/* 5B Walker E			*/	{ FALSE, Walker,	    EAST },
/* 5C Blob N			*/	{ FALSE, Blob,		    NORTH },
/* 5D Blob W			*/	{ FALSE, Blob,		    WEST },
/* 5E Blob S			*/	{ FALSE, Blob,		    SOUTH },
/* 5F Blob E			*/	{ FALSE, Blob,		    EAST },
/* 60 Paramecium N		*/	{ FALSE, Paramecium,	    NORTH },
/* 61 Paramecium W		*/	{ FALSE, Paramecium,	    WEST },
/* 62 Paramecium S		*/	{ FALSE, Paramecium,	    SOUTH },
/* 63 Paramecium E		*/	{ FALSE, Paramecium,	    EAST },
/* 64 Blue key			*/	{ TRUE,  Key_Blue,	    NIL },
/* 65 Red key			*/	{ TRUE,  Key_Red,	    NIL },
/* 66 Green key			*/	{ TRUE,  Key_Green,	    NIL },
/* 67 Yellow key		*/	{ TRUE,  Key_Yellow,	    NIL },
/* 68 Flippers			*/	{ TRUE,  Boots_Water,	    NIL },
/* 69 Fire boots		*/	{ TRUE,  Boots_Fire,	    NIL },
/* 6A Ice skates		*/	{ TRUE,  Boots_Ice,	    NIL },
/* 6B Suction boots		*/	{ TRUE,  Boots_Slide,	    NIL },
/* 6C Chip N			*/	{ FALSE, Chip,		    NORTH },
/* 6D Chip W			*/	{ FALSE, Chip,		    WEST },
/* 6E Chip S			*/	{ FALSE, Chip,		    SOUTH },
/* 6F Chip E			*/	{ FALSE, Chip,		    EAST }
};

int translatemapdata(gamestate *pstate)
{
    uchar	layer1[CXGRID * CYGRID];
    uchar	layer2[CXGRID * CYGRID];
    creature   *cr;
    gamesetup  *game;
    int		chipat = -1, pos, n;

    setstate(pstate);
    game = pstate->game;

    memset(layer1, 0, sizeof layer1);
    memset(layer2, 0, sizeof layer2);
    for (n = pos = 0 ; n < game->map1size ; ++n) {
	if (game->map1[n] == 0xFF) {
	    memset(layer1 + pos, game->map1[n + 2], game->map1[n + 1]);
	    pos += game->map1[n + 1];
	    n += 2;
	} else
	    layer1[pos++] = game->map1[n];
    }
    for (n = pos = 0 ; n < game->map2size ; ++n) {
	if (game->map2[n] == 0xFF) {
	    memset(layer2 + pos, game->map2[n + 2], game->map2[n + 1]);
	    pos += game->map2[n + 1];
	    n += 2;
	} else
	    layer2[pos++] = game->map2[n];
    }
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (layer2[pos] && layer2[pos] != layer1[pos]) {
	    floorat(pos) = fileids[layer2[pos]].id;
	    if (fileids[layer1[pos]].id == Chip) {
		chipat = pos;
		setcreatureat(pos, Nobody, NIL);
		/*creatureat(pos) = Nobody;*/
	    } else
		setcreatureat(pos, fileids[layer1[pos]].id,
				   fileids[layer1[pos]].dir);
		/*creatureat(pos) = fileids[layer1[pos]].id;*/
	} else if (fileids[layer1[pos]].isfloor) {
	    floorat(pos) = fileids[layer1[pos]].id;
	    setcreatureat(pos, Nobody, NIL);
	    /*creatureat(pos) = Nobody;*/
	} else {
	    floorat(pos) = Empty;
	    if (fileids[layer1[pos]].id == Chip) {
		chipat = pos;
		setcreatureat(pos, Nobody, NIL);
		/*creatureat(pos) = Nobody;*/
	    } else
		setcreatureat(pos, fileids[layer1[pos]].id,
				   fileids[layer1[pos]].dir);
		/*creatureat(pos) = fileids[layer1[pos]].id;*/
	}
    }
    for (n = 0, cr = creaturelist() ; n < game->creaturecount ; ++n, ++cr) {
	cr->pos = game->creatures[n];
	cr->id = fileids[layer1[cr->pos]].id;
	cr->dir = fileids[layer1[cr->pos]].dir;
	cr->waits = 0;
	cr->state = 0;
    }
    cr->pos = chipat;
    cr->id = Chip;
    cr->dir = fileids[layer1[cr->pos]].dir;
    if (cr->dir > EAST)
	die("Something's wrong with the map at (%d %d) [%02X].",
	    cr->pos % CXGRID, cr->pos / CXGRID, layer1[cr->pos]);
    cr->waits = 0;
    cr->state = 0;
    chippos() = cr->pos;
    chipdir() = cr->dir;
    ++cr;
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (isblockat(pos)) {
	    cr->pos = pos;
	    cr->id = Block;
	    cr->dir = fileids[layer1[pos]].dir;
	    cr->waits = 0;
	    cr->state = 0;
	    ++cr;
	}
    }
    cr->pos = -1;
    cr->id = Nobody;
    cr->dir = NIL;
    cr->waits = 0;
    cr->state = 0;

    chipsneeded() = game->chips;
    possession(Key_Blue) = possession(Key_Green)
			 = possession(Key_Red)
			 = possession(Key_Yellow) = 0;
    possession(Boots_Slide) = possession(Boots_Ice)
			    = possession(Boots_Water)
			    = possession(Boots_Fire) = 0;

    addsoundeffect(NULL);
    completed() = FALSE;

    return TRUE;
}

/*
 * Looking up button connections.
 */

static int trapfrombutton(int pos)
{
    xyconn     *traps;
    int		i;

    traps = traplist();
    for (i = traplistsize() ; i ; ++traps, --i)
	if (traps->from == pos)
	    return traps->to;
    return -1;
}

static int clonerfrombutton(int pos)
{
    xyconn     *cloners;
    int		i;

    cloners = clonerlist();
    for (i = clonerlistsize() ; i ; ++cloners, --i)
	if (cloners->from == pos)
	    return cloners->to;
    return -1;
}

static int istrapopen(int pos)
{
    xyconn     *traps;
    int		i;

    traps = traplist();
    for (i = traplistsize() ; i ; ++traps, --i)
	if (traps->to == pos && isoccupied(traps->from))
	    return TRUE;
    return FALSE;
}

/*
 * Maintaining the list of entities.
 */

#define	CS_CLONE		0x01
#define	CS_DEAD			0x02
#define	CS_RELEASED		0x04
#define	CS_TRAPJUMPING		0x08

static creature *lookupcreature(int pos, int includechip)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr)
	if (cr->pos == pos)
	    if (!(cr->state & CS_DEAD) && (cr->id != Chip || includechip))
		return cr;
/*
    fprintf(stderr, "Warn: looking for someone at (%d %d) but failed.\n",
	    pos % CXGRID, pos / CXGRID);
*/
    return NULL;
}

static void turntanks(void)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr)
	if (cr->id == Tank)
	    cr->dir = back(cr->dir);
}

static void releasecreatureat(int pos, int waits)
{
    creature   *cr;

    cr = lookupcreature(pos, TRUE);
    if (cr) {
	cr->state |= CS_RELEASED;
	if (cr->waits < waits)
	    cr->waits = waits;
    }
}

static void addclone(creature const *cr)
{
    creature   *new;

    for (new = creaturelist() ; new->id ; ++new) ;
    new->pos = cr->pos;
    new->id = cr->id;
    new->dir = cr->dir;
    new->waits = 0;
    new->state = CS_CLONE;
    ++new;
    new->id = Nobody;
}

static void removedead(void)
{
    creature   *old, *new;

    for (old = creaturelist() ; old->id && !(old->state & CS_DEAD) ; ++old) ;
    for (new = old ; old->id ; ++old)
	if (!(old->state & CS_DEAD))
	    *new++ = *old;
    new->id = Nobody;
}

/*
 * The rules of movement across the various floors.
 */

#define	DIR_IN(dir)	(1 << (dir))
#define	DIR_OUT(dir)	(16 << (dir))

#define	NORTH_IN	DIR_IN(NORTH)
#define	WEST_IN		DIR_IN(WEST)
#define	SOUTH_IN	DIR_IN(SOUTH)
#define	EAST_IN		DIR_IN(EAST)
#define	NORTH_OUT	DIR_OUT(NORTH)
#define	WEST_OUT	DIR_OUT(WEST)
#define	SOUTH_OUT	DIR_OUT(SOUTH)
#define	EAST_OUT	DIR_OUT(EAST)
#define	ALL_IN		(NORTH_IN | WEST_IN | SOUTH_IN | EAST_IN)
#define	ALL_OUT		(NORTH_OUT | WEST_OUT | SOUTH_OUT | EAST_OUT)
#define	ALL_IN_OUT	(ALL_IN | ALL_OUT)

static int const ydelta[] = { -1,  0, +1,  0 };
static int const xdelta[] = {  0, -1,  0, +1 };
static int const delta[] = { -CXGRID, -1, +CXGRID, +1 };

static struct { uchar chipmove, blockmove, creaturemove; } const movelaws[] = {
    /* Empty */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Slide_North */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Slide_West */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Slide_South */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Slide_East */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Slide_Random */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Ice */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* IceWall_Northwest */
    { NORTH_OUT | WEST_OUT | SOUTH_IN | EAST_IN,
      NORTH_OUT | WEST_OUT | SOUTH_IN | EAST_IN,
      NORTH_OUT | WEST_OUT | SOUTH_IN | EAST_IN },
    /* IceWall_Northeast */
    { NORTH_OUT | EAST_OUT | SOUTH_IN | WEST_IN,
      NORTH_OUT | EAST_OUT | SOUTH_IN | WEST_IN,
      NORTH_OUT | EAST_OUT | SOUTH_IN | WEST_IN },
    /* IceWall_Southwest */
    { SOUTH_OUT | WEST_OUT | NORTH_IN | EAST_IN,
      SOUTH_OUT | WEST_OUT | NORTH_IN | EAST_IN,
      SOUTH_OUT | WEST_OUT | NORTH_IN | EAST_IN },
    /* IceWall_Southeast */
    { SOUTH_OUT | EAST_OUT | NORTH_IN | WEST_IN,
      SOUTH_OUT | EAST_OUT | NORTH_IN | WEST_IN,
      SOUTH_OUT | EAST_OUT | NORTH_IN | WEST_IN },
    /* Gravel */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Dirt */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Water */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Fire */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Bomb */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Beartrap */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Burglar */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* HintButton */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Button_Blue */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Button_Green */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Button_Red */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Button_Brown */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Teleport */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Wall */
    { ALL_OUT, ALL_OUT, ALL_OUT },
    /* Wall_North */
    { NORTH_IN | WEST_IN | EAST_IN | WEST_OUT | SOUTH_OUT | EAST_OUT,
      NORTH_IN | WEST_IN | EAST_IN | WEST_OUT | SOUTH_OUT | EAST_OUT,
      NORTH_IN | WEST_IN | EAST_IN | WEST_OUT | SOUTH_OUT | EAST_OUT },
    /* Wall_West */
    { NORTH_IN | WEST_IN | SOUTH_IN | NORTH_OUT | SOUTH_OUT | EAST_OUT,
      NORTH_IN | WEST_IN | SOUTH_IN | NORTH_OUT | SOUTH_OUT | EAST_OUT,
      NORTH_IN | WEST_IN | SOUTH_IN | NORTH_OUT | SOUTH_OUT | EAST_OUT },
    /* Wall_South */
    { WEST_IN | SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT | EAST_OUT,
      WEST_IN | SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT | EAST_OUT,
      WEST_IN | SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT | EAST_OUT },
    /* Wall_East */
    { NORTH_IN | SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT | SOUTH_OUT,
      NORTH_IN | SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT | SOUTH_OUT,
      NORTH_IN | SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT | SOUTH_OUT },
    /* Wall_Southeast */
    { SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT,
      SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT,
      SOUTH_IN | EAST_IN | NORTH_OUT | WEST_OUT },
    /* HiddenWall_Perm */
    { ALL_OUT, ALL_OUT, ALL_OUT },
    /* HiddenWall_Temp */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* BlueWall_Real */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* BlueWall_Fake */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* SwitchWall_Open */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* SwitchWall_Closed */
    { ALL_OUT, ALL_OUT, ALL_OUT },
    /* PopupWall */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* CloneMachine */
    { ALL_OUT, ALL_OUT, ALL_OUT },
    /* Door_Blue */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Door_Green */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Door_Red */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Door_Yellow */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Socket */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Exit */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* ICChip */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Key_Blue */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Green */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Red */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Yellow */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Boots_Slide */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Boots_Ice */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Boots_Water */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Boots_Fire */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT }
};

static int canmakemove(creature const *cr, int dir, int noreally)
{
    int floorfrom, floorto;
    int	to, y, x;

    assert(cr);
    assert(dir != NIL);

    floorfrom = floorat(cr->pos);
    if (floorfrom == Beartrap || floorfrom == CloneMachine)
	if (!(cr->state & CS_RELEASED))
	    return FALSE;

    y = cr->pos / CXGRID + ydelta[dir];
    x = cr->pos % CXGRID + xdelta[dir];
    if (y < 0 || y >= CYGRID || x < 0 || x >= CXGRID)
	return FALSE;
    to = y * CXGRID + x;
    floorto = floorat(to);

    if (cr->id == Chip) {
	if (!(movelaws[floorfrom].chipmove & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].chipmove & DIR_IN(dir)))
	    return FALSE;
	if (isblockat(to))
	    if (!canmakemove(lookupcreature(to, FALSE), dir, noreally))
		return FALSE;
	if (floorto == Socket && chipsneeded() > 0)
	    return FALSE;
	if (isdoor(floorto) && !possession(floorto))
	    return FALSE;
	if (noreally) {
	    if (floorto == HiddenWall_Temp || floorto == BlueWall_Real) {
		if (noreally > 1) {
		    floorat(to) = Wall;
		    addsoundeffect("Mnphf!");
		}
		return FALSE;
	    }
	}
    } else if (cr->id == Block) {
	if (!(movelaws[floorfrom].blockmove & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].blockmove & DIR_IN(dir)))
	    return FALSE;
	if (iscreatureat(to))
	    return FALSE;
    } else {
	if (!(movelaws[floorfrom].creaturemove & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].creaturemove & DIR_IN(dir)))
	    return FALSE;
	if (iscreatureat(to))
	    return FALSE;
	if (floorto == Fire && (cr->id == Bug || cr->id == Walker))
	    return FALSE;
    }

    return TRUE;
}

/*
 * How everyone decides how to move.
 */

static int choosecreaturemove(creature const *cr)
{
    int		choices[4] = { NIL, NIL, NIL, NIL };
    int		pdir;
    int		floor;
    int		y, x, m, n;

    floor = floorat(cr->pos);
    if (isice(floor) || isslide(floor))
	return NIL;
    if ((floor == CloneMachine || floor == Beartrap)
				&& !(cr->state & CS_RELEASED))
	return NIL;

    pdir = cr->dir;

    if (floor == CloneMachine)
	choices[0] = pdir;
    else if (floor == Beartrap && (cr->id == Walker || cr->id == Ball
						    || cr->id == Fireball
						    || cr->id == Glider))
	choices[0] = pdir;
    else
	switch (cr->id) {
	  case Tank:
	    choices[0] = pdir;
	    break;
	  case Ball:
	    choices[0] = pdir;
	    choices[1] = back(pdir);
	    break;
	  case Glider:
	    choices[0] = pdir;
	    choices[1] = left(pdir);
	    choices[2] = right(pdir);
	    choices[3] = back(pdir);
	    break;
	  case Fireball:
	    choices[0] = pdir;
	    choices[1] = right(pdir);
	    choices[2] = left(pdir);
	    choices[3] = back(pdir);
	    break;
	  case Bug:
	    choices[0] = left(pdir);
	    choices[1] = pdir;
	    choices[2] = right(pdir);
	    choices[3] = back(pdir);
	    break;
	  case Paramecium:
	    choices[0] = right(pdir);
	    choices[1] = pdir;
	    choices[2] = left(pdir);
	    choices[3] = back(pdir);
	    break;
	  case Walker:
	    choices[0] = pdir;
	    choices[1] = left(pdir);
	    choices[2] = back(pdir);
	    choices[3] = right(pdir);
	    randomp3(choices + 1);
	    break;
	  case Blob:
	    choices[0] = pdir;
	    choices[1] = left(pdir);
	    choices[2] = back(pdir);
	    choices[3] = right(pdir);
	    randomp4(choices);
	    break;
	  case Teeth:
	    y = chippos() / CXGRID - cr->pos / CXGRID;
	    x = chippos() % CXGRID - cr->pos % CXGRID;
	    n = y < 0 ? NORTH : y > 0 ? SOUTH : NIL;
	    if (y < 0)
		y = -y;
	    m = x < 0 ? WEST : x > 0 ? EAST : NIL;
	    if (x < 0)
		x = -x;
	    if (x > y) {
		choices[0] = m;
		choices[1] = n;
	    } else {
		choices[0] = n;
		choices[1] = m;
	    }
	    pdir = choices[0];
	    break;
	}

    for (n = 0 ; n < 4 && choices[n] != NIL ; ++n)
	if (canmakemove(cr, choices[n], 0))
	    return choices[n];

    return pdir;
}

static int choosechipmove(void)
{
    int	floor, dir;

    dir = currentinput();
    currentinput() = NIL;
    if (dir == NIL)
	return NIL;
    recordmove(dir);

    floor = floorat(chippos());
    if (isice(floor) && !possession(Boots_Ice))
	return NIL;
    if (isslide(floor) && !possession(Boots_Slide)) {
	switch (floor) {
	  case Slide_North:	if (dir == NORTH)  return NIL;		break;
	  case Slide_West:	if (dir == WEST)   return NIL;		break;
	  case Slide_South:	if (dir == SOUTH)  return NIL;		break;
	  case Slide_East:	if (dir == EAST)   return NIL;		break;
	}
    }

    return dir;
}

/*
 * When something actually moves.
 */

static int pushblock(int from, int dir);

static int moveoutof(creature *cr, int dir)
{
    switch (floorat(cr->pos)) {
      case Beartrap:
	assert(cr->state & CS_RELEASED);
	cr->state &= ~CS_RELEASED;
	break;
      case CloneMachine:
	assert(cr->state & CS_RELEASED);
	cr->state &= ~CS_RELEASED;
	addclone(cr);
	return TRUE;
    }

    if (cr->id != Chip)
	setcreatureat(cr->pos, Nobody, NIL);

    return TRUE;
}

static int moveinto(creature *cr)
{
    int	floor, pos;

    floor = floorat(cr->pos);

    if (cr->id == Chip) {
	if (isblockat(cr->pos))
	    pushblock(cr->pos, cr->dir);
	switch (floor) {
	  case ICChip:
	    if (chipsneeded())
		--chipsneeded();
	    floorat(cr->pos) = Empty;
	    addsoundeffect("Chack!");
	    break;
	  case Door_Blue:
	  case Door_Green:
	  case Door_Red:
	  case Door_Yellow:
	    assert(possession(floor));
	    if (floor != Door_Green)
		--possession(floor);
	    floorat(cr->pos) = Empty;
	    addsoundeffect("Spang!");
	    break;
	  case Boots_Slide:
	  case Boots_Ice:
	  case Boots_Water:
	  case Boots_Fire:
	  case Key_Blue:
	  case Key_Green:
	  case Key_Red:
	  case Key_Yellow:
	    ++possession(floor);
	    floorat(cr->pos) = Empty;
	    addsoundeffect("Slurp!");
	    break;
	  case Burglar:
	    possession(Boots_Slide) = 0;
	    possession(Boots_Ice) = 0;
	    possession(Boots_Water) = 0;
	    possession(Boots_Fire) = 0;
	    addsoundeffect("Swipe!");
	    break;
	  case Socket:
	    assert(chipsneeded() == 0);
	    floorat(cr->pos) = Empty;
	    addsoundeffect("Chack!");
	    break;
	  case Dirt:
	  case BlueWall_Fake:
	    floorat(cr->pos) = Empty;
	    break;
	  case PopupWall:
	    floorat(cr->pos) = Wall;
	    break;
	}
    } else if (cr->id == Block) {
	switch (floor) {
	  case Water:
	    floorat(cr->pos) = Dirt;
	    addsoundeffect("Plash!");
	    cr->state |= CS_DEAD;
	    return FALSE;
	  case Bomb:
	    floorat(cr->pos) = Empty;
	    addsoundeffect("Booom!");
	    cr->state |= CS_DEAD;
	    return FALSE;
	}
	setcreatureat(cr->pos, cr->id, cr->dir);
	/*creatureat(cr->pos) = cr->id;*/
    } else {
	switch (floor) {
	  case Water:
	    if (cr->id != Glider) {
		cr->state |= CS_DEAD;
		return FALSE;
	    }
	    break;
	  case Fire:
	    if (cr->id != Fireball) {
		cr->state |= CS_DEAD;
		return FALSE;
	    }
	    break;
	  case Bomb:
	    floorat(cr->pos) = Empty;
	    cr->state |= CS_DEAD;
	    addsoundeffect("Booom!");
	    return FALSE;
	}
	setcreatureat(cr->pos, cr->id, cr->dir);
	/*creatureat(cr->pos) = cr->id;*/
    }

    switch (floor) {
      case Button_Blue:
	turntanks();
	addsoundeffect("Klick!");
	break;
      case Button_Green:
	for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos)
	    if (floorat(pos) == SwitchWall_Open
				|| floorat(pos) == SwitchWall_Closed)
		floorat(pos) ^= SwitchWall_Open ^ SwitchWall_Closed;
	break;
      case Button_Red:
	pushbutton(clonerfrombutton(cr->pos));
	addsoundeffect("Thock!");
	break;
      case Button_Brown:
	releasecreatureat(trapfrombutton(cr->pos), 0);
	addsoundeffect("Plock!");
	break;
    case Beartrap:
	if (istrapopen(cr->pos))
	    cr->state |= CS_RELEASED;
	break;
    }

    return TRUE;
}

static int makemove(creature *cr, int dir)
{
    assert(dir >= NORTH && dir <= EAST);

    if (floorat(cr->pos) == Beartrap || floorat(cr->pos) == CloneMachine)
	assert(cr->state & CS_RELEASED);

    moveoutof(cr, dir);

    cr->dir = dir;
    cr->pos += delta[dir];
    if (cr->id == Chip) {
	chipdir() = cr->dir;
	chippos() = cr->pos;
    }

    return moveinto(cr);
}

static int pushblock(int pos, int dir)
{
    creature   *cr;

    cr = lookupcreature(pos, FALSE);
    assert(cr);
    assert(cr->id == Block);

    if (floorat(cr->pos) == Beartrap || floorat(cr->pos) == CloneMachine)
	assert(cr->state & CS_RELEASED);

    moveoutof(cr, dir);
    cr->dir = dir;
    cr->pos += delta[dir];
    return moveinto(cr);
}

/*
 * Automatic activities.
 */

static int teleports(creature *cr)
{
    int pos, origpos, n;

    if (floorat(cr->pos) != Teleport)
	return TRUE;

    origpos = pos = cr->pos;

    for (;;) {
	--pos;
	if (pos < 0)
	    pos += CXGRID * CYGRID;
	if (floorat(pos) == Teleport) {
	    cr->pos = pos;
	    n = canmakemove(cr, cr->dir, 1);
	    cr->pos = origpos;
	    if (n)
		break;
	    if (pos == origpos) {
		if (cr->id != Chip)
		    return TRUE;
		cr->dir = back(cr->dir);
		addsoundeffect("Mnphf!");
		break;
	    }
	}
    }

    if (pos != origpos) {
	moveoutof(cr, cr->dir);
	cr->pos = pos;
	if (cr->id == Chip)
	    chippos() = pos;
	moveinto(cr);
	addsoundeffect("Whing!");
    }
    return makemove(cr, cr->dir);
}

static int floormovement(creature *cr)
{
    int floor, dir, rid;
    int	r = TRUE;

    floor = floorat(cr->pos);

    if (isice(floor)) {
	if (cr->id == Chip && possession(Boots_Ice))
	    return TRUE;
	dir = cr->dir;
	if (dir == NIL)
	    return TRUE;
	rid = back(dir);
	switch (floor) {
	  case IceWall_Northeast:	dir ^= 1;	break;
	  case IceWall_Southwest:	dir ^= 1;	break;
	  case IceWall_Northwest:	dir ^= 3;	break;
	  case IceWall_Southeast:	dir ^= 3;	break;
	}
	if (canmakemove(cr, dir, 2))
	    r = makemove(cr, dir);
	else if (canmakemove(cr, rid, 2))
	    r = makemove(cr, rid);
	if (cr->id == Chip)
	    cr->waits = 0;
	else if (cr->id == Block)
	    cr->state |= CS_TRAPJUMPING;
    } else if (isslide(floor)) {
	if (cr->id == Chip && possession(Boots_Slide))
	    return TRUE;
	switch (floor) {
	  case Slide_North:	dir = NORTH;		break;
	  case Slide_West:	dir = WEST;		break;
	  case Slide_South:	dir = SOUTH;		break;
	  case Slide_East:	dir = EAST;		break;
	  case Slide_Random:	dir = random4();	break;
	}
	if (canmakemove(cr, dir, 2))
	    r = makemove(cr, dir);
	if (cr->id == Chip)
	    cr->waits = 0;
	else if (cr->id == Block)
	    cr->state |= CS_TRAPJUMPING;
    } else if (floor == Beartrap && (cr->state & CS_TRAPJUMPING)) {
	if (canmakemove(cr, cr->dir, 2))
	    r = makemove(cr, cr->dir);
    }


    return r;
}

static void housekeeping(void)
{
    creature   *cr;
    int		floor, pos;

    removedead();

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->state & CS_CLONE)
	    cr->state &= ~CS_CLONE;
	if (cr->id == Block && cr->dir != (uchar)NIL) {
	    floor = floorat(cr->pos);
	    if (!isice(floor) && !isslide(floor)
			      && floor != Beartrap && floor != CloneMachine
			      && floor != Teleport) {
		cr->state &= ~CS_TRAPJUMPING;
		cr->dir = NIL;
	    }
	}
    }
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (floorat(pos) == CloneMachine && isbuttondown(pos)) {
	    releasecreatureat(pos, 1);
	    resetbutton(pos);
	}
    }
    cr->id = Nobody;

    displayflags() = 0;
    if (floorat(chippos()) == HintButton)
	displayflags() |= DF_SHOWHINT;
}

/*
 * Ending the game.
 */

static int checkforending(void)
{
    int	floor;

    floor = floorat(chippos());
    if (floor == Exit) {
	completed() = TRUE;
	addsoundeffect("Tadaa!");
	return +1;
    } else if (floor == Bomb) {
	addsoundeffect("Booom!");
	return -1;
    } else if (floor == Water && !possession(Boots_Water)) {
	addsoundeffect("Glugg!");
	return -2;
    } else if (floor == Fire && !possession(Boots_Fire)) {
	addsoundeffect("Youch!");
	return -3;
    } else if (iscreatureat(chippos())) {
	addsoundeffect("Smack!");
	return -4;
    }
/*
    if (chipdir() == NORTH) {
	 creature *cr;
	 fprintf(stderr, "*** %d\n", state->currenttime);
	 fprintf(stderr, "Chip at (%d %d)\n",
		 chippos() % CXGRID, chippos() / CXGRID);
	 fprintf(stderr, "Creatures at:");
	 for (cr = creaturelist() ; cr->id ; ++cr)
	      fprintf(stderr, " (%d %d)",
		      cr->pos % CXGRID, cr->pos / CXGRID);
	 fprintf(stderr, "\n");
    }
*/
    return 0;
}

/*
 * Debugging.
 */

static void dumpmap(void)
{
    creature   *cr;
    int		y, x;

    for (y = 0 ; y < CXGRID * CYGRID ; y += CXGRID) {
	for (x = 0 ; x < CXGRID ; ++x)
	    fprintf(stderr, "%c%02X%02X",
		    (state->map[y + x].state ? ':' : '.'),
		    state->map[y + x].floor, state->map[y + x].entity);
	fputc('\n', stderr);
    }
    fputc('\n', stderr);
    for (cr = creaturelist() ; cr->id ; ++cr)
	fprintf(stderr, "%c%c%1X%02X\n",
		(cr->waits ? '+' : '-'), (cr->state ? ':' : '.'),
		(cr->dir & 0x0F), cr->id);
}

static void verifymap(void)
{
    creature   *cr;
    int		pos;

    if (state->currpos < 0 || state->currpos >= CXGRID * CYGRID)
	die("%d: Chip has left the map: %04X",
	    state->currenttime, state->currpos);
    if (state->currdir < NORTH || state->currdir > EAST)
	die("%d: Chip lacks direction: (%d)",
	    state->currenttime, state->currdir);
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (state->map[pos].floor >= Count_Floors)
	    die("%d: Undefined floor %d at (%d %d)",
		state->currenttime, state->map[pos].floor,
		pos % CXGRID, pos / CXGRID);
	if (state->map[pos].entity >= Count_Entities * 4)
	    die("%d: Undefined entity at (%d %d)",
		state->currenttime, state->map[pos].entity,
		pos % CXGRID, pos / CXGRID);
    }
    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->id >= Count_Entities)
	    die("%d: Undefined creature %d at (%d %d)",
		state->currenttime, cr->id,
		cr->pos % CXGRID, cr->pos / CXGRID);
	if (cr->pos < 0 || cr->pos >= CXGRID * CYGRID)
	    die("%d: Creature %d:%d has left the map: %04X",
		state->currenttime, cr - creaturelist(), cr->id, cr->pos);
	if (cr->dir > EAST && (cr->dir != (uchar)NIL || cr->id != Block))
	    die("%d: Creature %d:%d lacks direction (%d)",
		state->currenttime, cr - creaturelist(), cr->id, cr->dir);
	if (cr->waits > 3)
	    die("%d: Creature %d:%d has a waiting time of %d",
		state->currenttime, cr - creaturelist(), cr->id, cr->waits);
	if (cr->state & 0xF0)
	    die("%d: Creature %d:%d is in an undefined state: %X",
		state->currenttime, cr - creaturelist(), cr->id, cr->state);
    }
}

/*
 * The central function.
 */

int advancegame(gamestate *pstate)
{
    creature   *cr;
    int		dir, n;

    setstate(pstate);

    if (state->currentinput == CmdDebugDumpMap) {
	dumpmap();
	exit(0);
    }
    verifymap();

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->state & (CS_CLONE | CS_DEAD))
	    continue;
	floormovement(cr);
	teleports(cr);
    }
    if ((n = checkforending()))
	return n;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->state & (CS_CLONE | CS_DEAD))
	    continue;
	if (cr->id == Block && floorat(cr->pos) != CloneMachine)
	    continue;
	if (cr->waits) {
	    --cr->waits;
	    continue;
	}
	if (cr->id == Chip)
	    dir = choosechipmove();
	else
	    dir = choosecreaturemove(cr);
	if (dir != NIL && canmakemove(cr, dir, 2)) {
	    makemove(cr, dir);
	    cr->waits = cr->id == Teeth ? 3 : 1;
	} else {
	    if (floorat(cr->pos) == CloneMachine)
		cr->state &= ~CS_RELEASED;
	    else if (dir != NIL) {
		cr->dir = dir;
		if (cr->id == Chip) {
		    chipdir() = dir;
		    addsoundeffect("Mnphf!");
		} else
		    setcreatureat(cr->pos, cr->id, cr->dir);
	    }
	}
	if ((n = checkforending()))
	    return n;
    }

    housekeeping();

    return 0;
}
