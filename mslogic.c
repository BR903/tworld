/* mslogic.c: The game logic for the MS ruleset.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<assert.h>
#include	"defs.h"
#include	"err.h"
#include	"state.h"
#include	"random.h"
#include	"mslogic.h"

/* An otherwise-invalid floor value used to indicate that the floor
 * is actually an inert creature.
 */
#define	Buried_Creature		Count_Floors

static int advancecreature(creature *cr, int dir);

/* A pointer to the game state, used so that it doesn't have to be
 * passed to every single function.
 */
static gamestate       *state;

/* The list of creatures currently undergoing forced movement.
 */
static creature	       *slipping[CXGRID * CYGRID];

/*
 * Accessor macros for various fields in the game state. Many of the
 * macros can be used as an lvalue.
 */

#define	setstate(p)		(state = (p))

#define	creaturelist()		(state->creatures)

#define	getchip()		(creaturelist())
#define	chippos()		(getchip()->pos)
#define	chipdir()		(getchip()->dir)

#define	chipsneeded()		(state->chipsneeded)
#define	floorat(pos)		(state->map[pos].floor)
#define	hfloorat(pos)		(state->map[pos].hfloor)

#define	claimlocation(pos)	(state->map[pos].state |= 0x40)
#define	removeclaim(pos)	(state->map[pos].state &= ~0x40)
#define	issomeoneat(pos)	(state->map[pos].state & 0x40)
#define	isoccupied(pos)		((pos) == chippos() || issomeoneat(pos))
#define	isbuttondown(pos)	(state->map[pos].state & 0x20)
#define	pushbutton(pos)		(state->map[pos].state |= 0x20)
#define	resetbutton(pos)	(state->map[pos].state &= ~0x20)
#define	sticktoggle(pos)	(state->map[pos].state |= 0x10)
#define	unsticktoggle(pos)	(state->map[pos].state &= ~0x10)
#define	istogglestuck(pos)	(state->map[pos].state & 0x10)

#define	clonerlist()		(state->game->cloners)
#define	clonerlistsize()	(state->game->clonercount)
#define	traplist()		(state->game->traps)
#define	traplistsize()		(state->game->trapcount)

#define	currenttime()		(state->currenttime)
#define	currentinput()		(state->currentinput)
#define	displayflags()		(state->displayflags)

#define	mainprng()		(&state->mainprng)

#define	iscompleted()		(state->statusflags & SF_COMPLETED)
#define	setcompleted()		(state->statusflags |= SF_COMPLETED)

#define	lastmove()		(state->lastmove)

#define	addsoundeffect(str)	(state->soundeffect = (str))

#define	possession(obj)	(*_possession(obj))
static short *_possession(int obj)
{
    switch (obj) {
      case Key_Red:		return &state->keys[0];
      case Key_Blue:		return &state->keys[1];
      case Key_Yellow:		return &state->keys[2];
      case Key_Green:		return &state->keys[3];
      case Boots_Ice:		return &state->boots[0];
      case Boots_Slide:		return &state->boots[1];
      case Boots_Fire:		return &state->boots[2];
      case Boots_Water:		return &state->boots[3];
      case Door_Red:		return &state->keys[0];
      case Door_Blue:		return &state->keys[1];
      case Door_Yellow:		return &state->keys[2];
      case Door_Green:		return &state->keys[3];
      case Ice:			return &state->boots[0];
      case IceWall_Northwest:	return &state->boots[0];
      case IceWall_Northeast:	return &state->boots[0];
      case IceWall_Southwest:	return &state->boots[0];
      case IceWall_Southeast:	return &state->boots[0];
      case Slide_North:		return &state->boots[1];
      case Slide_West:		return &state->boots[1];
      case Slide_South:		return &state->boots[1];
      case Slide_East:		return &state->boots[1];
      case Slide_Random:	return &state->boots[1];
      case Fire:		return &state->boots[2];
      case Water:		return &state->boots[3];
    }
    warn("Invalid object %d handed to possession()\n", obj);
    assert(!"possession() called with an invalid object");
    return NULL;
}

/*
 * Simple floor functions.
 */

/* Translate a slide floor into the direction it points in. In the
 * case of a random slide floor, if advance is TRUE a new direction
 * shall be selected; otherwise the current direction is used.
 */
static int getslidedir(int floor, int advance)
{
    switch (floor) {
      case Slide_North:		return NORTH;
      case Slide_West:		return WEST;
      case Slide_South:		return SOUTH;
      case Slide_East:		return EAST;
      case Slide_Random:
	if (advance)
	    state->rndslidedir = 1 << random4(mainprng());
	return state->rndslidedir;
    }
    return NIL;
}

/* Alter a creature's direction if they are at an ice wall.
 */
static int icewallturn(int floor, int dir)
{
    switch (floor) {
      case IceWall_Northeast:
	return dir == SOUTH ? EAST : dir == WEST ? NORTH : dir;
      case IceWall_Southwest:
	return dir == NORTH ? WEST : dir == EAST ? SOUTH : dir;
      case IceWall_Northwest:
	return dir == SOUTH ? WEST : dir == EAST ? NORTH : dir;
      case IceWall_Southeast:
	return dir == NORTH ? EAST : dir == WEST ? SOUTH : dir;
    }
    return dir;
}

/* Find the location of a bear trap from one of its buttons.
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

/* Find the location of a clone machine from one of its buttons.
 */
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

/* Return TRUE if a bear trap is currently passable.
 */
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

/* Flip-flop the state of any toggle walls.
 */
static void togglewalls(void)
{
    int	pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos)
	if (floorat(pos) == SwitchWall_Open
			|| floorat(pos) == SwitchWall_Closed)
	    if (!istogglestuck(pos))
		floorat(pos) ^= SwitchWall_Open ^ SwitchWall_Closed;
}

/*
 * Functions that manage the list of entities.
 */

/* Creature state flags.
 */
#define	CS_DEAD			0x01	/* is dead */
#define	CS_INERT		0x02	/* is present but will never move */
#define	CS_RELEASED		0x04	/* can leave a beartrap/cloner */
#define	CS_HASMOVED		0x08	/* already used current move */
#define	CS_SLIP			0x10	/* is on the slip list */
#define	CS_SLIDE		0x20	/* is on the slip list but can move */
#define	CS_TURNING		0x40	/* is turning around */
#define	CS_NOCONTROLLER		0x80	/* has no controller */

/* Return the creature located at pos. Ignores Chip unless includechip
 * is TRUE.
 */
static creature *lookupcreature(int pos, int includechip)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->hidden)
	    continue;
	if (cr->pos == pos)
	    if (cr->id != Chip || includechip)
		return cr;
    }
    return NULL;
}

/* Return the creature located at pos and currently underneath a floor
 * tile.
 */
static creature *lookupburiedcreature(int pos)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->state & CS_DEAD)
	    continue;
	if (cr->hidden && cr->pos == pos)
	    return cr;
    }
    return NULL;
}

/* Extract a creature that was originally underneath a floor tile.
 * The creature will still be inert.
 */
static void disintercreature(int pos)
{
    creature   *cr;

    assert(floorat(pos) == Buried_Creature);
    floorat(pos) = Empty;
    cr = lookupburiedcreature(pos);
    assert(cr);
    if (cr->id == Block) {
	floorat(pos) = Block_Emergent;
    } else {
	cr->state |= CS_INERT;
	cr->hidden = FALSE;
	claimlocation(pos);
    }
}

/* Vivify an inert block.
 */
static void excavateblock(int pos)
{
    creature   *cr;

    assert(floorat(pos) == Block_Emergent);
    floorat(pos) = Empty;
    cr = lookupburiedcreature(pos);
    assert(cr && cr->id == Block);
    cr->hidden = FALSE;
    claimlocation(pos);
}

/* Given a creature (presumably in an open beartrap), return the
 * direction of its controller, or NIL if the creature has no living
 * controller.
 */
static int getcontrollerdir(creature const *cr)
{
    assert(cr != getchip());
    if (cr->state & CS_NOCONTROLLER)
	return NIL;
    while (--cr > creaturelist())
	if (!(cr->state & CS_INERT) && cr->id != Block && cr->id != Chip)
	    break;
    if (cr == getchip() || (cr->state & CS_DEAD))
	return NIL;
    return cr->dir;
}

/* If act is FALSE, return the direction of the current boss, if
 * any. If act is TRUE, make the given creature the current boss.
 */
static int clonerboss(creature *cr, int act)
{
    static creature    *boss = NULL;

    if (act)
	boss = cr;
    if (boss && boss->state & CS_DEAD)
	boss = NULL;
    return boss ? boss->dir : NIL;
}

#define	setboss(cr)	(clonerboss(cr, TRUE))
#define	getbossdir()	(clonerboss(NULL, FALSE))

/* Create a new creature as a clone of the given creature.
 */
static creature *addclone(creature const *old)
{
    creature   *new;

    for (new = creaturelist() + 1 ; new->id ; ++new)
	if (new->state & CS_DEAD)
	    break;

    if (new - creaturelist() + 1 >= CXGRID * CYGRID)
	die("Ran out of room in the creatures array!");

    if (!new->id) {
	new[1].id = Nobody;
	new[1].pos = -1;
	new[1].dir = NIL;
    }

    *new = *old;
    new->state = 0;

    return new;
}

/* Mark a creature as dead.
 */
static void removecreature(creature *cr)
{
    cr->state |= CS_DEAD;
    cr->state &= ~(CS_SLIP | CS_SLIDE);
    (void)getbossdir();
    if (cr->id != Chip) {
	removeclaim(cr->pos);
	cr->hidden = TRUE;
    }
}

/* Turn around any and all tanks.
 */
static void turntanks(void)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->hidden) /*???*/
	    continue;
	if (cr->state & CS_INERT)
	    continue;
	if (cr->id == Tank) {
	    if (cr->state & CS_TURNING) {
		cr->dir = back(cr->dir);
	    } else {
		cr->dir = left(cr->dir);
		cr->state |= CS_TURNING;
	    }
	}
    }
}

/*
 * Maintaining the slip list.
 */

/* Add the given creature to the slip list if it is not already on it.
 * If nosliding is FALSE, the creature is still permitted to slide.
 */
static void addtosliplist(creature *cr, int nosliding)
{
    int	n;

    cr->state &= ~(CS_SLIP | CS_SLIDE);
    cr->state |= nosliding ? CS_SLIP : CS_SLIDE;
    for (n = 0 ; slipping[n] ; ++n)
	if (slipping[n] == cr)
	    return;
    if (n >= (int)(sizeof slipping / sizeof *slipping))
	die("Ran out of room in the sliplist!");
    if (cr->id == Chip) {
	do
	    slipping[n + 1] = slipping[n];
	while (n--);
	slipping[0] = cr;
    } else {
	slipping[n++] = cr;
	slipping[n] = NULL;
    }
}

/* Remove the given creature from the slip list.
 */
static void removefromsliplist(creature *cr)
{
    int	n;

    cr->state &= ~(CS_SLIP | CS_SLIDE);
    for (n = 0 ; slipping[n] ; ++n)
	if (slipping[n] == cr)
	    break;
    for ( ; slipping[n] ; ++n)
	slipping[n] = slipping[n + 1];
}

/*
 * The laws of movement across the various floors.
 *
 * Chip, blocks, and other creatures all have slightly different rules
 * about what sort of tiles they are permitted to move into and out
 * of. The following lookup table encapsulates these rules. Note that
 * these rules are only the first check; a creature may be generally
 * permitted a particular type of move but still prevented in a
 * specific situation.
 */

#define	DIR_IN(dir)	(dir)
#define	DIR_OUT(dir)	((dir) << 4)

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

static struct { unsigned char chip, block, creature; } const movelaws[] = {
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
    /* Door_Red */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Door_Blue */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Door_Yellow */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Door_Green */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Socket */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Exit */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* ICChip */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Key_Red */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Blue */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Yellow */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Green */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Boots_Ice */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Boots_Slide */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Boots_Fire */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Boots_Water */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_OUT },
    /* Water_Splash */
    { 0, 0, 0 },
    /* Dirt_Splash */
    { 0, 0, 0 },
    /* Bomb_Explosion */
    { 0, 0, 0 },
    /* Block_Emergent */
    { ALL_OUT, ALL_OUT, ALL_OUT }
};

static int canpushblock(creature *cr, int dir, int noreally);

/* Return TRUE if the given creature is allowed to attempt to move in
 * the given direction. noreally defines how accurate a check is
 * needed. A value of zero indicates that the check should not have
 * any side effects. A value of one permits the check to have side
 * effects only if the move cannot be made. A value of two permits any
 * side effects which might be necessary to determine a correct
 * answer. (The possible side effects are exposing a blue/hidden wall,
 * and causing a block to be pushed.)
 */
static int canmakemove(creature const *cr, int dir, int noreally)
{
    creature   *other;
    int		floorfrom, floorto;
    int		to, y, x;

    assert(cr);
    assert(dir != NIL);

    y = cr->pos / CXGRID;
    x = cr->pos % CXGRID;
    y += dir == NORTH ? -1 : dir == SOUTH ? +1 : 0;
    x += dir == WEST ? -1 : dir == EAST ? +1 : 0;
    if (y < 0 || y >= CYGRID || x < 0 || x >= CXGRID)
	return FALSE;
    to = y * CXGRID + x;

    floorfrom = floorat(cr->pos);
    floorto = floorat(to);

    if (floorfrom == Beartrap || floorfrom == CloneMachine)
	if (!(cr->state & CS_RELEASED))
	    return FALSE;

    if (cr->id == Chip) {
	if (!(movelaws[floorfrom].chip & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].chip & DIR_IN(dir)))
	    return FALSE;
	if (floorto == Socket && chipsneeded() > 0)
	    return FALSE;
	if (isdoor(floorto) && !possession(floorto))
	    return FALSE;
	other = lookupcreature(to, FALSE);
	if (other && other->id == Block)
	    if (!canpushblock(other, dir, noreally))
		return FALSE;
	if (noreally) {
	    if (floorto == HiddenWall_Temp || floorto == BlueWall_Real) {
		if (noreally)
		    floorat(to) = Wall;
		return FALSE;
	    }
	}
    } else if (cr->id == Block) {
	if (!(movelaws[floorfrom].block & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].block & DIR_IN(dir)))
	    return FALSE;
	if (issomeoneat(to))
	    return FALSE;
    } else {
	if (!(movelaws[floorfrom].creature & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].creature & DIR_IN(dir)))
	    return FALSE;
	if (issomeoneat(to))
	    return FALSE;
	if (floorto == Fire && (cr->id == Bug || cr->id == Walker))
	    return FALSE;
    }

    return TRUE;
}

/* Return TRUE if the given block is allowed to be moved in the given
 * direction. If noreally is two or more, then the indicated movement
 * of the block will be initiated.
 */
static int canpushblock(creature *cr, int dir, int noreally)
{
    assert(cr && cr->id == Block);
    assert(floorat(cr->pos) != CloneMachine);
    assert(dir != NIL);

    if (noreally) {
	if (noreally > 1) {
	    removefromsliplist(cr);
	    return advancecreature(cr, dir);
	} else
	    return canmakemove(cr, dir, noreally);
    } else
	return TRUE;
}

/*
 * How everyone selects their move.
 */

/* This function embodies the movement behavior of all the creatures.
 * Given a creature, this function enumerates its desired direction
 * of movement and selects the first one that is permitted.
 */
static void choosecreaturemove(creature *cr)
{
    int		choices[4] = { NIL, NIL, NIL, NIL };
    int		dir, pdir;
    int		floor;
    int		y, x, m, n;

    cr->tdir = NIL;

    if (cr->hidden)
	return;
    if (cr->id == Block)
	return;
    if (!currenttime())
	return;
    if (currenttime() & (cr->id == Teeth || cr->id == Blob ? 6 : 2))
	return;

    if (cr->state & CS_TURNING) {
	cr->dir = left(cr->dir);
	cr->state &= ~(CS_TURNING | CS_HASMOVED);
    }
    if (cr->state & (CS_INERT | CS_HASMOVED | CS_SLIP | CS_SLIDE))
	return;

    floor = floorat(cr->pos);

    pdir = dir = cr->dir;

    if (floor == CloneMachine || floor == Beartrap) {
	if (!(cr->state & CS_RELEASED))
	    return;
	switch (cr->id) {
	  case Tank:
	  case Ball:
	  case Glider:
	  case Fireball:
	  case Walker:
	    choices[0] = dir;
	    break;
	  case Blob:
	    choices[0] = dir;
	    choices[1] = left(dir);
	    choices[2] = back(dir);
	    choices[3] = right(dir);
	    randomp4(mainprng(), choices);
	    break;
	  case Bug:
	  case Paramecium:
	  case Teeth:
	    choices[0] = getcontrollerdir(cr);
	    choices[0] = floor == CloneMachine ? getbossdir()
					       : getcontrollerdir(cr);
	    pdir = NIL;
	    break;
	  default:
	    warn("Non-creature %02X trying to move", cr->id);
	    assert(!"Unknown creature trying to move");
	    break;
	}
    } else {
	switch (cr->id) {
	  case Tank:
	    choices[0] = dir;
	    break;
	  case Ball:
	    choices[0] = dir;
	    choices[1] = back(dir);
	    break;
	  case Glider:
	    choices[0] = dir;
	    choices[1] = left(dir);
	    choices[2] = right(dir);
	    choices[3] = back(dir);
	    break;
	  case Fireball:
	    choices[0] = dir;
	    choices[1] = right(dir);
	    choices[2] = left(dir);
	    choices[3] = back(dir);
	    break;
	  case Walker:
	    choices[0] = dir;
	    choices[1] = left(dir);
	    choices[2] = back(dir);
	    choices[3] = right(dir);
	    randomp3(mainprng(), choices + 1);
	    break;
	  case Blob:
	    choices[0] = dir;
	    choices[1] = left(dir);
	    choices[2] = back(dir);
	    choices[3] = right(dir);
	    randomp4(mainprng(), choices);
	    break;
	  case Bug:
	    choices[0] = left(dir);
	    choices[1] = dir;
	    choices[2] = right(dir);
	    choices[3] = back(dir);
	    break;
	  case Paramecium:
	    choices[0] = right(dir);
	    choices[1] = dir;
	    choices[2] = left(dir);
	    choices[3] = back(dir);
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
	  default:
	    warn("Non-creature %02X trying to move", cr->id);
	    assert(!"Unknown creature trying to move");
	    break;
	}
    }

    for (n = 0 ; n < 4 && choices[n] != NIL ; ++n) {
	cr->tdir = choices[n];
	if (canmakemove(cr, choices[n], 0))
	    return;
    }

    if (cr->id == Tank)
	cr->state |= CS_HASMOVED;

    cr->tdir = pdir;
}

/* Determine the direction of Chip's next move. If discard is TRUE,
 * then Chip is not currently permitted to select a direction of
 * movement, and the player's input should not be retained.
 */
static void choosechipmove(creature *cr, int discard)
{
    int	floor, dir;

    cr->tdir = NIL;

    if (cr->hidden)
	return;
    if (cr->state & CS_INERT)
	return;

    if (!(currenttime() & 3))
	cr->state &= ~CS_HASMOVED;
    if (cr->state & CS_HASMOVED)
	return;

    dir = currentinput();
    currentinput() = NIL;
    if (dir == NIL || discard)
	return;

    if (cr->state & CS_SLIDE) {
	floor = floorat(cr->pos);
	if (isslide(floor) && !possession(Boots_Slide)
			   && dir == getslidedir(floor, FALSE))
	    return;
    }

    lastmove() = dir;
    cr->tdir = dir;
}

/* Teleport the given creature instantaneously from one teleport tile
 * to another.
 */
static void teleportcreature(creature *cr)
{
    int pos, origpos, n;

    assert(floorat(cr->pos) == Teleport);
    if (cr->hidden)
	return;

    origpos = pos = cr->pos;

    for (;;) {
	--pos;
	if (pos < 0)
	    pos += CXGRID * CYGRID;
	if (floorat(pos) == Teleport) {
	    cr->pos = pos;
	    if (cr->dir == NIL)
		warn("%d: directionless creature %d:%d on teleport at (%d %d)",
		     currenttime(), cr - creaturelist(), cr->id,
		     cr->pos % CXGRID, cr->pos / CXGRID);
	    n = canmakemove(cr, cr->dir, 1);
	    cr->pos = origpos;
	    if (n)
		break;
	    if (pos == origpos) {
		if (cr->id != Chip)
		    return;
		cr->dir = back(cr->dir);
		break;
	    }
	}
    }

    if (cr->id != Chip)
	removeclaim(cr->pos);
    cr->pos = pos;
    if (cr->id != Chip)
	claimlocation(cr->pos);
    if (cr->id == Chip)
	addsoundeffect("Whing!");
}

/* This function determines if the given creature is currently being
 * forced to move. (Ice, slide floors, and teleports all cause forced
 * movements; bear traps can also force blocks to move, if they
 * entered the trap via one of the other three causes.) If so, the
 * direction is stored in the creature's fdir field, and TRUE is
 * returned unless the creature can override the forced move.
 */
static int getslipmove(creature *cr)
{
    int floor, dir;

    cr->fdir = NIL;

    floor = floorat(cr->pos);

    if (isice(floor)) {
	if (cr->id == Chip && possession(Boots_Ice))
	    return FALSE;
	dir = cr->dir;
	if (dir == NIL)
	    return FALSE;
	dir = icewallturn(floor, dir);
	if (!canmakemove(cr, dir, 1)) {
	    dir = back(cr->dir);
	    if (!canmakemove(cr, dir, 1))
		return TRUE;
	    cr->dir = dir;
	}
	cr->fdir = dir;
	return TRUE;
    } else if (isslide(floor)) {
	if (cr->id == Chip && possession(Boots_Slide))
	    return FALSE;
	cr->fdir = getslidedir(floor, TRUE);
	return cr->id != Chip;
    } else if (floor == Teleport) {
	cr->fdir = cr->dir;
	return TRUE;
    } else if (floor == Beartrap) {
	cr->fdir = cr->dir;
	return TRUE;
    }

    return FALSE;
}

/* Determine the move(s) a creature will make on the current tick.
 */
static void choosemove(creature *cr)
{
    if (cr->id == Chip) {
	choosechipmove(cr, cr->state & CS_SLIP);
    } else {
	if (cr->state & CS_SLIP)
	    cr->tdir = NIL;
	else
	    choosecreaturemove(cr);
    }
}

/* Initiate the cloning of a creature.
 */
static void activatecloner(int buttonpos)
{
    int	pos;

    pos = clonerfrombutton(buttonpos);
    if (pos < 0)
	return;
    assert(floorat(pos) == CloneMachine);
    if (!lookupcreature(pos, TRUE))
	return;
    pushbutton(pos);
    addsoundeffect("Thock!");
}

/* Open a bear trap. Any creature already in the trap is released.
 */
static void springtrap(int buttonpos)
{
    creature   *cr;
    int		pos;

    pos = trapfrombutton(buttonpos);
    if (pos < 0)
	return;
    assert(floorat(pos) == Beartrap);
    cr = lookupcreature(pos, TRUE);
    if (cr)
	cr->state |= CS_RELEASED;
    addsoundeffect("Plock!");
}

/*
 * When something actually moves.
 */

/* Initiate a move by the given creature in the given direction.
 */
static int startmovement(creature *cr, int dir)
{
    assert(dir != NIL);
    assert(!(cr->state & CS_INERT));

    if (!canmakemove(cr, dir, 2)) {
	if (cr->id == Chip || (floorat(cr->pos) != Beartrap
			       && floorat(cr->pos) != CloneMachine))
	    cr->dir = dir;
	return FALSE;
    }

    if (cr->id != Chip)
	removeclaim(cr->pos);

    switch (floorat(cr->pos)) {
      case Beartrap:
	assert(cr->state & CS_RELEASED);
	cr->state &= ~CS_RELEASED;
	break;
      case CloneMachine:
	assert(cr->state & CS_RELEASED);
	cr->state &= ~CS_RELEASED;
	addclone(cr);
	claimlocation(cr->pos);
	setboss(cr);
	break;
      case Block_Emergent:
	excavateblock(cr->pos);
	break;
    }

    cr->dir = dir;

    return TRUE;
}

/* Remove the top floor layer and expose the lower layer.
 */
static void collectfloor(int pos)
{
    floorat(pos) = hfloorat(pos);
    hfloorat(pos) = Empty;
    if (floorat(pos) == Buried_Creature)
	disintercreature(pos);
}

/* Complete the movement of the given creature. Most side effects
 * produced by moving onto a tile occur at this point. This function
 * is also the only place where a creature can be added to the slip
 * list.
 */
static void endmovement(creature *cr)
{
    int	floor;

    floor = floorat(cr->pos);

    if (cr->id == Chip) {
	switch (floor) {
	  case Empty:
	    collectfloor(cr->pos);
	    break;
	  case Water:
	    if (!possession(Boots_Water)) {
		removecreature(cr);
		floorat(cr->pos) = Water_Splash;
		addsoundeffect("Glugg!");
	    }
	    break;
	  case Fire:
	    if (!possession(Boots_Fire)) {
		removecreature(cr);
		addsoundeffect("Yowch!");
	    }
	    break;
	  case Dirt:
	    collectfloor(cr->pos);
	    break;
	  case BlueWall_Fake:
	    collectfloor(cr->pos);
	    break;
	  case PopupWall:
	    floorat(cr->pos) = Wall;
	    break;
	  case Door_Red:
	  case Door_Blue:
	  case Door_Yellow:
	  case Door_Green:
	    assert(possession(floor));
	    if (floor != Door_Green)
		--possession(floor);
	    addsoundeffect("Spang!");
	    collectfloor(cr->pos);
	    break;
	  case Boots_Ice:
	  case Boots_Slide:
	  case Boots_Fire:
	  case Boots_Water:
	  case Key_Red:
	  case Key_Blue:
	  case Key_Yellow:
	  case Key_Green:
	    ++possession(floor);
	    addsoundeffect("Slurp!");
	    collectfloor(cr->pos);
	    break;
	  case Burglar:
	    possession(Boots_Ice) = 0;
	    possession(Boots_Slide) = 0;
	    possession(Boots_Fire) = 0;
	    possession(Boots_Water) = 0;
	    addsoundeffect("Swipe!");
	    break;
	  case ICChip:
	    if (chipsneeded())
		--chipsneeded();
	    addsoundeffect("Chack!");
	    collectfloor(cr->pos);
	    break;
	  case Socket:
	    assert(chipsneeded() == 0);
	    addsoundeffect("Chack!");
	    collectfloor(cr->pos);
	    break;
	  case Exit:
	    setcompleted();
	    addsoundeffect("Tadaa!");
	    break;
	}
    } else if (cr->id == Block) {
	claimlocation(cr->pos);
	switch (floor) {
	  case Empty:
	    collectfloor(cr->pos);
	    break;
	  case Water:
	    floorat(cr->pos) = Dirt;
	    addsoundeffect("Plash!");
	    removecreature(cr);
	    break;
	}
    } else {
	claimlocation(cr->pos);
	hfloorat(cr->pos) = Empty;
	switch (floor) {
	  case Water:
	    if (cr->id != Glider)
		removecreature(cr);
	    break;
	  case Fire:
	    if (cr->id != Fireball)
		removecreature(cr);
	    break;
	}
    }

    switch (floor) {
      case Bomb:
	removecreature(cr);
	if (cr->id != Chip)
	    floorat(cr->pos) = Empty;
	addsoundeffect("Booom!");
	break;
      case Button_Blue:
	turntanks();
	addsoundeffect("Klick!");
	break;
      case Button_Green:
	togglewalls();
	break;
      case Button_Red:
	activatecloner(cr->pos);
	break;
      case Button_Brown:
	springtrap(cr->pos);
	break;
      case Beartrap:
	if (istrapopen(cr->pos))
	    cr->state |= CS_RELEASED;
	break;
    }

    if (!(cr->state & CS_DEAD)) {
	if (floor == Teleport)
	    addtosliplist(cr, TRUE);
	else if (isice(floor)
			&& (cr->id != Chip || !possession(Boots_Ice)))
	    addtosliplist(cr, TRUE);
	else if (isslide(floor)
			&& (cr->id != Chip || !possession(Boots_Slide)))
	    addtosliplist(cr, cr->id != Chip);
	else if (floor != Beartrap || cr->id != Block)
	    cr->state &= ~(CS_SLIP | CS_SLIDE);
    }

    if (cr->id == Chip) {
	if (lookupcreature(cr->pos, FALSE))
	    removecreature(cr);
    } else {
	if (cr->pos == chippos())
	    removecreature(getchip());
    }
}

/* Move the given creature in the given direction.
 */
static int advancecreature(creature *cr, int dir)
{
    static int const	delta[] = { 0, -CXGRID, -1, 0, +CXGRID, 0, 0, 0, +1 };

    if (dir == NIL)
	return TRUE;

    if (!startmovement(cr, dir))
	return FALSE;
    cr->pos += delta[dir];
    endmovement(cr);
    return TRUE;
}

/* Return TRUE if gameplay is over.
 */
static int checkforending(void)
{
    if (getchip()->state & CS_DEAD)
	return -1;
    if (iscompleted())
	return +1;
    return 0;
}

/*
 * Automatic activities.
 */

/* Execute all forced moves for creatures on the slip list.
 */
static void floormovements(void)
{
    creature   *cr;
    int		n;

    for (n = 0 ; slipping[n] ; ++n) {
	cr = slipping[n];
	getslipmove(cr);
	if (cr->fdir != NIL) {
	    if (advancecreature(cr, cr->fdir))
		if (cr->id == Chip)
		    cr->state &= ~CS_HASMOVED;
/*
	} else {
	    if (slipping[n + 1] && slipping[n + 1]->id != Chip) {
		removefromsliplist(cr);
		addtosliplist(cr, TRUE);
	    }
*/
	}
	if (checkforending())
	    return;
    }
    for (n = 0 ; slipping[n] ; ++n) ;
    for (--n ; n >= 0 ; --n) {
	cr = slipping[n];
	if (!(cr->state & (CS_SLIP | CS_SLIDE)))
	    removefromsliplist(cr);
    }
}

/* Teleport any creatures newly arrived on teleport tiles.
 */
static void teleportations(void)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->hidden)
	    continue;
	if (floorat(cr->pos) != Teleport)
	    continue;
	teleportcreature(cr);
    }
}

/*
 * Debugging functions.
 */

/* Print out a rough image of the level and the list of creatures.
 */
static void dumpmap(void)
{
    creature   *cr;
    int		y, x;

    for (y = 0 ; y < CXGRID * CYGRID ; y += CXGRID) {
	for (x = 0 ; x < CXGRID ; ++x)
	    fprintf(stderr, "%c%02X",
		    (state->map[y + x].state ? ':' : '.'),
		    state->map[y + x].floor);
	fputc('\n', stderr);
    }
    fputc('\n', stderr);
    for (cr = creaturelist() ; cr->id ; ++cr) {
	fprintf(stderr, "%02X%c (%d %d)",
			cr->id, "-^<?v???>"[(int)cr->dir],
			cr->pos % CXGRID, cr->pos / CXGRID);
	for (x = 0 ; slipping[x] ; ++x) {
	    if (cr == slipping[x]) {
		fprintf(stderr, " [%d]", x + 1);
		break;
	    }
	}
	fprintf(stderr, "%s%s%s%s%s%s%s\n",
			cr->hidden ? " hidden" : "",
			cr->state & CS_DEAD ? " dead" : "",
			cr->state & CS_INERT ? " inert" : "",
			cr->state & CS_RELEASED ? " released" : "",
			cr->state & CS_HASMOVED ? " has-moved" : "",
			cr->state & CS_SLIP ? " slipping" : "",
			cr->state & CS_SLIDE ? " sliding" : "");
    }
}

/* Run various sanity checks on the current game state.
 */
static void verifymap(void)
{
    creature   *cr;
    int		pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (state->map[pos].floor >= Count_Floors)
	    die("%d: Undefined floor %d at (%d %d)",
		state->currenttime, state->map[pos].floor,
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
	if (cr->dir > EAST && (cr->dir != NIL || cr->id != Block))
	    die("%d: Creature %d:%d lacks direction (%d)",
		state->currenttime, cr - creaturelist(), cr->id, cr->dir);
    }
}

/*
 * Per-tick maintenance functions.
 */

/* Actions and checks that occur at the start of every tick.
 */
static void initialhousekeeping(void)
{
    if (currentinput() == CmdDebugCmd2) {
	dumpmap();
	exit(0);
    } else if (currentinput() == CmdDebugCmd1) {
	static int mark = 0;
	warn("Mark %d.", ++mark);
	currentinput() = NIL;
    }

    verifymap();
}

/* Actions and checks that occur at the end of every tick.
 */
static void finalhousekeeping(void)
{
    creature   *cr;
    int		pos;

    if (!(currenttime() & 1)) {
	for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	    if (floorat(pos) == CloneMachine && isbuttondown(pos)) {
		resetbutton(pos);
		cr = lookupcreature(pos, TRUE);
		if (!cr)
		    continue;
		cr->state |= CS_RELEASED;
		if (cr->id == Block) {
		    if (cr->dir == NIL)
			warn("Directionless block on clone machine at (%d %d)",
			     cr->pos % CXGRID, cr->pos / CXGRID);
		    if (canmakemove(cr, cr->dir, 2))
			advancecreature(cr, cr->dir);
		}
	    }
	}
    }

    if (floorat(chippos()) == HintButton)
	displayflags() |= DF_SHOWHINT;
    else
	displayflags() &= ~DF_SHOWHINT;
}

/*
 * The exported functions.
 */

/* Translation table for the codes used by the data file to define the
 * initial state of a level.
 */
static struct { unsigned char isfloor, id, dir; } const fileids[] = {
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
/* 20 not used			*/	{ TRUE,  -1,		    NIL },
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
/* 33 drowning Chip		*/	{ TRUE,  Water_Splash,	    NIL },
/* 34 burned Chip		*/	{ TRUE,  -1,		    NIL },
/* 35 burned Chip		*/	{ TRUE,  -1,		    NIL },
/* 36 not used			*/	{ TRUE,  -1,		    NIL },
/* 37 not used			*/	{ TRUE,  -1,		    NIL },
/* 38 not used			*/	{ TRUE,  -1,		    NIL },
/* 39 Chip in exit		*/	{ TRUE,  -1,		    NIL },
/* 3A exit - end game		*/	{ TRUE,  Exit,		    NIL },
/* 3B exit - end game		*/	{ TRUE,  Exit,		    NIL },
/* 3C Chip swimming north	*/	{ TRUE,  -1,		    NIL },
/* 3D Chip swimming west	*/	{ TRUE,  -1,		    NIL },
/* 3E Chip swimming south	*/	{ TRUE,  -1,		    NIL },
/* 3F Chip swimming east	*/	{ TRUE,  -1,		    NIL },
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

/* Initialize the gamestate structure to the state at the beginning of
 * the level, using the data in the associated gamesetup structure.
 * The level map is decoded and assembled, the lists of beartraps,
 * clone machines, and active creatures are drawn up, and other
 * miscellaneous initializations are performed.
 */
int ms_initgame(gamestate *pstate)
{
    unsigned char	layer1[CXGRID * CYGRID];
    unsigned char	layer2[CXGRID * CYGRID];
    creature	       *cr;
    creature	       *chip;
    gamesetup	       *game;
    int			pos, n;

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
	if (!layer2[pos]) {
	    if (fileids[layer1[pos]].isfloor) {
		floorat(pos) = fileids[layer1[pos]].id;
		layer1[pos] = Nobody;
	    } else {
		floorat(pos) = Empty;
	    }
	    hfloorat(pos) = Empty;
	} else {
	    if (fileids[layer2[pos]].isfloor) {
		if (fileids[layer1[pos]].isfloor) {
		    floorat(pos) = fileids[layer1[pos]].id;
		    hfloorat(pos) = fileids[layer2[pos]].id;
		    if (fileids[layer2[pos]].id == SwitchWall_Closed)
			sticktoggle(pos);
		    else if (fileids[layer2[pos]].id == SwitchWall_Open)
			sticktoggle(pos);
		    layer1[pos] = Nobody;
		} else {
		    floorat(pos) = fileids[layer2[pos]].id;
		    hfloorat(pos) = Empty;
		}
	    } else {
		if (fileids[layer1[pos]].isfloor) {
		    floorat(pos) = fileids[layer1[pos]].id;
		    hfloorat(pos) = Buried_Creature;
		    layer1[pos] = Nobody;
		} else {
		    if (fileids[layer1[pos]].id != Block)
			die("Creature on creature: don't know what to do!"
			    " -- (%d %d): 0x%02X on 0x%02X", pos % CXGRID,
			    pos / CXGRID, layer1[pos], layer2[pos]);
		    floorat(pos) = Empty;
		    hfloorat(pos) = Buried_Creature;
		}
	    }
	}
    }

    chip = creaturelist();
    chip->pos = -1;
    cr = chip + 1;
    for (n = 0 ; n < game->creaturecount ; ++n) {
	pos = game->creatures[n];
	if (!layer1[pos] || fileids[layer1[pos]].isfloor) {
	    warn("Level %d: No creature at location (%d %d)\n",
		 game->number, pos % CXGRID, pos / CXGRID);
	    continue;
	}
	cr->pos = pos;
	cr->id = fileids[layer1[pos]].id;
	cr->dir = fileids[layer1[pos]].dir;
	cr->state = 0;
	cr->hidden = FALSE;
	layer1[pos] = 0;
	claimlocation(pos);
	if (floorat(pos) != CloneMachine)
	    setboss(cr);
	++cr;
    }
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (layer1[pos]) {
	    if (floorat(pos) == CloneMachine
				|| fileids[layer1[pos]].id == Block) {
		cr->pos = pos;
		cr->id = fileids[layer1[pos]].id;
		cr->dir = fileids[layer1[pos]].dir;
		cr->state = 0;
		layer1[pos] = 0;
		cr->hidden = FALSE;
		claimlocation(pos);
		++cr;
	    } else if (fileids[layer1[pos]].id == Chip) {
		chip->pos = pos;
		chip->id = Chip;
		chip->dir = fileids[layer1[pos]].dir;
		chip->state = 0;
		chip->hidden = FALSE;
		layer1[pos] = 0;
	    }
	}
	if (hfloorat(pos) == Buried_Creature) {
	    cr->pos = pos;
	    cr->id = fileids[layer2[pos]].id;
	    cr->dir = fileids[layer2[pos]].dir;
	    cr->state = 0;
	    cr->hidden = TRUE;
	    layer2[pos] = 0;
	    ++cr;
	}
    }
    if (chip->pos < 0)
	die("Level %d: Chip nowhere on map!", game->number);
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (!layer1[pos])
	    continue;
	cr->pos = pos;
	cr->id = fileids[layer1[pos]].id;
	cr->dir = fileids[layer1[pos]].dir;
	cr->state = CS_INERT;
	cr->hidden = FALSE;
	claimlocation(pos);
	++cr;
    }

    cr->id = Nobody;

    slipping[0] = NULL;
    chipsneeded() = game->chips;
    possession(Key_Red) = possession(Key_Blue)
			= possession(Key_Yellow)
			= possession(Key_Green) = 0;
    possession(Boots_Ice) = possession(Boots_Slide)
			  = possession(Boots_Fire)
			  = possession(Boots_Water) = 0;

    addsoundeffect(NULL);

    if (floorat(chippos()) == HintButton)
	displayflags() |= DF_SHOWHINT;
    else
	displayflags() &= ~DF_SHOWHINT;

    return TRUE;
}

/* Advance the game state by one tick.
 */
int ms_advancegame(gamestate *pstate)
{
    creature   *cr;
    int		n;

    setstate(pstate);

    initialhousekeeping();

    if (!(currenttime() & 1)) {
	for (cr = creaturelist() + 1 ; cr->id ; ++cr) {
	    if (cr->hidden || (cr->state & CS_INERT) || cr->id == Block)
		continue;
	    choosemove(cr);
	    if (cr->tdir != NIL)
		advancecreature(cr, cr->tdir);
	    cr->tdir = NIL;
	    cr->fdir = NIL;
	    if ((n = checkforending()))
		return n;
	}
    }

    if (!(currenttime() & 1)) {
	floormovements();
	if ((n = checkforending()))
	    return n;
    }

    cr = getchip();
    choosemove(cr);
    if (cr->tdir != NIL) {
	if (advancecreature(cr, cr->tdir)) {
	    if ((n = checkforending()))
		return n;
	} else
	    addsoundeffect("Mnphf!");
	cr->state |= CS_HASMOVED;
    }
    cr->tdir = NIL;
    cr->fdir = NIL;

    if (currenttime() & 1) {
	teleportations();
	if ((n = checkforending()))
	    return n;
    }

    finalhousekeeping();

    return 0;
}
