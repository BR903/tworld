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
#include	"commands.h"
#include	"statestr.h"
#include	"random.h"
#include	"logic.h"

static int advancecreature(creature *cr, int oob);

extern int recordmove(int dir);

static int const delta[] = { -CXGRID, -1, +CXGRID, +1 };

static int	rndslidedir = NORTH;
static int	greenbuttons = 0;

/*
 * Game state accessor macros.
 */

static gamestate *state;

#define	setstate(p)		(state = (p))

#define	clonerlist()		(state->game->cloners)
#define	clonerlistsize()	(state->game->clonercount)
#define	traplist()		(state->game->traps)
#define	traplistsize()		(state->game->trapcount)
#define	creaturelist()		(state->creatures)

#define	chippos()		(creaturelist()->pos)
#define	chipdir()		(creaturelist()->dir)
#define	chipmoving()		(creaturelist()->moving)
#define	chipstate()		(creaturelist()->state)

#define	currentinput()		(state->currentinput)
#define	displayflags()		(state->displayflags)

#define	chipsneeded()		(state->chipsneeded)
#define	completed()		(state->completed)

#define	addsoundeffect(str)	(state->soundeffect = (str))

#define	floorat(pos)		(state->map[pos].floor)

#define	claimlocation(pos)	(state->map[pos].state |= 0x40)
#define	removeclaim(pos)	(state->map[pos].state &= ~0x40)
#define	issomeoneat(pos)	(state->map[pos].state & 0x40)
#define	isoccupied(pos)		(chippos() == pos || issomeoneat(pos))
#define	countervalue(pos)	(state->map[pos].state & 0x1F)
#define	incrcounter(pos)	(++state->map[pos].state & 0x1F)
#define	decrcounter(pos)	(--state->map[pos].state & 0x1F)
#define	resetcounter(pos)	(state->map[pos].state &= ~0x1F)
#define	setcounter(pos, n)	\
    (state->map[pos].state = (state->map[pos].state & ~0x1F) | ((n) & 0x1F))

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
 * Basic floor functions.
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
	    rndslidedir = right(rndslidedir);
	return rndslidedir;
    }
    die("Invalid floor %d handed to getslidedir()\n", floor);
    return NIL;
}

static void applyicewallturn(creature *cr)
{
    int	floor, dir;

    floor = floorat(cr->pos);
    dir = cr->dir;
    switch (floor) {
      case IceWall_Northeast:
	dir = dir == SOUTH ? EAST : dir == WEST ? NORTH : dir;
	break;
      case IceWall_Southwest:
	dir = dir == NORTH ? WEST : dir == EAST ? SOUTH : dir;
	break;
      case IceWall_Northwest:
	dir = dir == SOUTH ? WEST : dir == EAST ? NORTH : dir;
	break;
      case IceWall_Southeast:
	dir = dir == NORTH ? EAST : dir == WEST ? SOUTH : dir;
	break;
    }
    cr->dir = dir;
}

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

static void togglewalls(void)
{
    int	pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos)
	if (floorat(pos) == SwitchWall_Open)
	    floorat(pos) = SwitchWall_Closed;
	else if (floorat(pos) == SwitchWall_Closed)
	    floorat(pos) = SwitchWall_Open;
}

/*
 * Maintaining the list of entities.
 */

#define	CS_DEAD			0x01
#define	CS_REVERSE		0x02
#define	CS_SLIDETOKEN		0x04

static creature *lookupcreature(int pos, int includechip)
{
    creature   *cr;

    cr = creaturelist();
    if (!includechip)
	++cr;
    for ( ; cr->id ; ++cr)
	if (cr->pos == pos)
	    if (!(cr->state & CS_DEAD))
		return cr;
    return NULL;
}

static creature *addclone(creature *old)
{
    creature   *new;

    assert(old);

    for (new = creaturelist() + 1 ; new->id ; ++new) {
	if (new->state & CS_DEAD) {
	    *new = *old;
	    return new;
	}
    }

    if (new - creaturelist() + 1 >= CXGRID * CYGRID)
	die("Ran out of room in the creatures array!");

    *new = *old;

    new[1].pos = -1;
    new[1].id = Nobody;
    new[1].dir = NIL;

    return new;
}

static void removecreature(creature *cr)
{
    cr->state |= CS_DEAD;
    if (cr->id != Chip)
	removeclaim(cr->pos);
}

static void turntanks(void)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->state & CS_DEAD)
	    continue;
	if (cr->id == Tank && floorat(cr->pos) != CloneMachine)
	    cr->state ^= CS_REVERSE;
    }
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
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
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
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* ICChip */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Key_Blue */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Green */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Key_Red */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Yellow */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Slide */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Ice */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Water */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Fire */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Water_Splash */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Dirt_Splash */
    { ALL_OUT, ALL_OUT, ALL_OUT },
    /* Bomb_Explosion */
    { ALL_OUT, ALL_IN_OUT, ALL_IN_OUT }
};

static int canpushblock(creature *block, int dir, int noreally);

static int canmakemove(creature const *cr, int dir,
		       int noreally, int releasing)
{
    creature   *other;
    int		floorfrom, floorto;
    int		to, y, x;

    assert(cr);
    assert((uchar)dir != (uchar)NIL);

    to = cr->pos + delta[dir];
    y = to / CXGRID;
    x = to % CXGRID;
    if (y < 0 || y >= CYGRID || x < 0 || x >= CXGRID)
	return FALSE;

    floorfrom = floorat(cr->pos);
    floorto = floorat(to);

    if ((floorfrom == Beartrap || floorfrom == CloneMachine) && !releasing)
	return FALSE;

    if (isslide(floorfrom) && (cr->id != Chip || !possession(Boots_Slide)))
	if (getslidedir(floorfrom, FALSE) == back(dir))
	    return FALSE;

    if (cr->id == Chip) {
	if (!(movelaws[floorfrom].chipmove & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].chipmove & DIR_IN(dir)))
	    return FALSE;
	if (floorto == Socket && chipsneeded() > 0)
	    return FALSE;
	if (isdoor(floorto) && !possession(floorto))
	    return FALSE;
	other = lookupcreature(to, FALSE);
	if (other && other->id == Block) {
	    if (!canpushblock(other, dir, noreally))
		return FALSE;
	}
	if (floorto == HiddenWall_Temp || floorto == BlueWall_Real) {
	    if (noreally)
		floorat(to) = Wall;
	    return FALSE;
	}
    } else if (cr->id == Block) {
	if (cr->moving > 0)
	    return FALSE;
	if (!(movelaws[floorfrom].blockmove & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].blockmove & DIR_IN(dir)))
	    return FALSE;
	if (issomeoneat(to))
	    return FALSE;
    } else {
	if (!(movelaws[floorfrom].creaturemove & DIR_OUT(dir)))
	    return FALSE;
	if (!(movelaws[floorto].creaturemove & DIR_IN(dir)))
	    return FALSE;
	if (issomeoneat(to))
	    return FALSE;
	if (floorto == Fire && cr->id != Fireball)
	    return FALSE;
    }

    return TRUE;
}

static int canpushblock(creature *block, int dir, int noreally)
{
    assert(block && block->id == Block);
    assert(floorat(block->pos) != CloneMachine);
    assert((uchar)dir != (uchar)NIL);

    if (!canmakemove(block, dir, noreally, FALSE))
	return FALSE;
    if (noreally > 1) {
	block->tdir = dir;
	advancecreature(block, FALSE);
    }

    return TRUE;
}

/*
 * How everyone decides how to move.
 */

static void choosecreaturemove(creature *cr)
{
    int		choices[4] = { NIL, NIL, NIL, NIL };
    int		dir, pdir;
    int		floor;
    int		y, x, m, n;

    cr->tdir = NIL;
    if (cr->id == Block)
	return;
    if (cr->fdir != NIL)
	return;
    floor = floorat(cr->pos);
    if (floor == CloneMachine || floor == Beartrap) {
	cr->tdir = cr->dir;
	return;
    }

    dir = cr->dir;
    pdir = NIL;
    assert((uchar)dir != (uchar)NIL);

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
      case Walker:
	choices[0] = dir;
	choices[1] = randomof3(left(dir), back(dir), right(dir));
	break;
      case Blob:
	choices[0] = random4();
	break;
      case Teeth:
	if ((state->currenttime >> 2) & 1)
	    return;
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
    for (n = 0 ; n < 4 && choices[n] != NIL ; ++n) {
	cr->tdir = choices[n];
	if (canmakemove(cr, choices[n], 0, FALSE))
	    return;
    }

    if (pdir != NIL)
	cr->tdir = pdir;
}

static void choosechipmove(creature *cr, int discard)
{
    int	dir;

    dir = currentinput();
    currentinput() = NIL;
    if (dir == NIL || discard) {
	cr->tdir = NIL;
	return;
    }
    recordmove(dir);
    cr->tdir = dir;
}

static int getforcedmove(creature *cr)
{
    int	floor, dir;

    cr->fdir = NIL;

    floor = floorat(cr->pos);

    if (isice(floor)) {
	if (cr->id == Chip && possession(Boots_Ice))
	    return FALSE;
	dir = cr->dir;
	if ((uchar)dir == (uchar)NIL)
	    return FALSE;
	cr->fdir = dir;
	return TRUE;
    } else if (isslide(floor)) {
	if (cr->id == Chip && possession(Boots_Slide))
	    return FALSE;
	cr->fdir = getslidedir(floor, TRUE);
	return !(cr->state & CS_SLIDETOKEN);
    } else if (floor == Teleport) {
	cr->fdir = cr->dir;
	return TRUE;
    }

    return FALSE;
}

static int choosemove(creature *cr)
{
    if (cr->id == Chip) {
	choosechipmove(cr, getforcedmove(cr));
    } else {
	if (getforcedmove(cr))
	    cr->tdir = NIL;
	else
	    choosecreaturemove(cr);
    }

    return cr->tdir != NIL || cr->fdir != NIL;
}

/*
 * Special movements.
 */

static int teleportcreature(creature *cr)
{
    int pos, origpos, n;

    assert(floorat(cr->pos) == Teleport);

    origpos = pos = cr->pos;

    for (;;) {
	--pos;
	if (pos < 0)
	    pos += CXGRID * CYGRID;
	if (floorat(pos) == Teleport) {
	    cr->pos = pos;
	    n = canmakemove(cr, cr->dir, 0, FALSE);
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

    if (cr->id != Chip)
	removeclaim(cr->pos);
    cr->pos = pos;
    if (cr->id != Chip)
	claimlocation(cr->pos);
    addsoundeffect("Whing!");
    return TRUE;
}

static void activatecloner(int pos)
{
    creature   *cr;
    creature   *clone;

    assert(pos >= 0);
    assert(floorat(pos) == CloneMachine);
    cr = lookupcreature(pos, TRUE);
    assert(cr);
    clone = addclone(cr);
    if (!advancecreature(cr, TRUE))
	clone->state |= CS_DEAD;
}

static void springtrap(int pos)
{
    creature   *cr;

    cr = lookupcreature(pos, TRUE);
    if (cr && cr->dir != (uchar)NIL)
	advancecreature(cr, TRUE);
}

/*
 * When something actually moves.
 */

static int startmovement(creature *cr, int releasing)
{
    int	dir;
    int	floorfrom, floorto;

    assert(cr->moving <= 0);

    if (cr->tdir != NIL)
	dir = cr->tdir;
    else if (cr->fdir != NIL)
	dir = cr->fdir;
    else
	return FALSE;

    cr->dir = dir;

    floorfrom = floorat(cr->pos);

    if (!canmakemove(cr, dir, 2, releasing)) {
	if (cr->id == Chip && cr->tdir != NIL)
	    cr->state &= ~CS_SLIDETOKEN;
	if (isice(floorfrom) && (cr->id != Chip || !possession(Boots_Ice))) {
	    cr->dir = back(dir);
	    applyicewallturn(cr);
	}
	return FALSE;
    }

    if (floorfrom == CloneMachine || floorfrom == Beartrap)
	assert(releasing);

    if (cr->id != Chip)
	removeclaim(cr->pos);
    cr->pos += delta[dir];
    if (cr->id != Chip)
	claimlocation(cr->pos);

    floorto = floorat(cr->pos);

    if (cr->id == Chip && !possession(Boots_Slide)) {
	if (isslide(floorfrom) && cr->tdir == NIL)
	    cr->state |= CS_SLIDETOKEN;
	else if (!isice(floorfrom) || possession(Boots_Ice))
	    cr->state &= ~CS_SLIDETOKEN;
    }

    cr->moving += 8;

    if (cr->id != Chip && cr->pos == chippos()) {
	if (!(chipstate() & CS_DEAD))
	    warn("Chip discovered dead in startmovement()");
	chipstate() |= CS_DEAD;
/*
	if (cr->id != Block)
	    removecreature(cr);
*/
    }

    if (cr->id == Chip) {
	cr = lookupcreature(cr->pos, FALSE);
	if (cr) {
	    assert(!(chipstate() & CS_DEAD));
	    warn("Chip indirectly discovered dead in startmovement()");
	    chipstate() |= CS_DEAD;
/*
	    if (cr->id != Block)
		removecreature(cr);
*/
	}
    }

    return TRUE;
}

static int movementspeed(creature *cr)
{
    int	floor, speed;

    speed = cr->id == Blob ? 1 : 2;

    floor = floorat(cr->pos);
    if (isslide(floor) && (cr->id != Chip || !possession(Boots_Slide))) {
	speed *= 2;
    } else if (isice(floor) && (cr->id != Chip || !possession(Boots_Ice))) {
	speed *= 2;
    }

    return speed;
}

static int endmovement(creature *cr)
{
    int	floor;

    assert(cr->moving <= 0);

    floor = floorat(cr->pos);

    applyicewallturn(cr);

    if (cr->id == Chip) {
	switch (floor) {
	  case Water:
	  case Water_Splash:
	    if (!possession(Boots_Water)) {
		removecreature(cr);
		floorat(cr->pos) = Water_Splash;
		setcounter(cr->pos, 4);
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
	  case BlueWall_Fake:
	    floorat(cr->pos) = Empty;
	    break;
	  case PopupWall:
	    floorat(cr->pos) = Wall;
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
	  case ICChip:
	    if (chipsneeded())
		--chipsneeded();
	    floorat(cr->pos) = Empty;
	    addsoundeffect("Chack!");
	    break;
	  case Socket:
	    assert(chipsneeded() == 0);
	    floorat(cr->pos) = Empty;
	    addsoundeffect("Chack!");
	    break;
	  case Exit:
	    completed() = TRUE;
	    addsoundeffect("Tadaa!");
	    break;
	}
    } else if (cr->id == Block) {
	switch (floor) {
	  case Water:
	  case Water_Splash:
	    floorat(cr->pos) = Dirt_Splash;
	    setcounter(cr->pos, 12);
	    removecreature(cr);
	    addsoundeffect("Plash!");
	    break;
	  case Key_Blue:
	    floorat(cr->pos) = Empty;
	    break;
	}
	if (cr->pos == chippos())
	    removecreature(creaturelist());
    } else {
	switch (floor) {
	  case Water:
	  case Water_Splash:
	    if (cr->id != Glider) {
		removecreature(cr);
		floorat(cr->pos) = Water_Splash;
		setcounter(cr->pos, 4);
		addsoundeffect("Plash!");
	    }
	    break;
	  case Key_Blue:
	    floorat(cr->pos) = Empty;
	    break;
	}
    }

    switch (floor) {
      case Bomb:
	removecreature(cr);
	floorat(cr->pos) = Bomb_Explosion;
	setcounter(cr->pos, 10);
	addsoundeffect("Booom!");
	break;
      case Teleport:
	teleportcreature(cr);
	break;
      case Button_Blue:
	turntanks();
	addsoundeffect("Click!");
	break;
      case Button_Green:
	++greenbuttons;
	addsoundeffect("Klock!");
	break;
      case Button_Red:
	activatecloner(clonerfrombutton(cr->pos));
	break;
      case Button_Brown:
	addsoundeffect("Plink!");
	break;
    }

    return TRUE;
}

static int advancecreature(creature *cr, int oob)
{
    char	tdir;

    if (cr->moving <= 0) {
	if (oob) {
	    assert((uchar)cr->dir != (uchar)NIL);
	    tdir = cr->tdir;
	    cr->tdir = cr->dir;
	} else if (cr->tdir == NIL && cr->fdir == NIL)
	    return TRUE;
	if (!startmovement(cr, oob)) {
	    if (oob)
		cr->tdir = tdir;
	    return FALSE;
	}
	cr->tdir = NIL;
    }
    cr->moving -= movementspeed(cr);
    if (cr->moving <= 0)
	endmovement(cr);
    return TRUE;
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
	    fprintf(stderr, "%02X%c", state->map[y + x].floor,
		    (state->map[y + x].state ? state->map[y + x].state & 0x40 ? '*' : '.' : ' '));
	fputc('\n', stderr);
    }
    fputc('\n', stderr);
    for (cr = creaturelist() ; cr->id ; ++cr)
	fprintf(stderr, "%02X%c%1d (%d %d)%s%s%s\n",
			cr->id, "-^<v>"[(cr->dir + 1) & 255],
			cr->moving, cr->pos % CXGRID, cr->pos / CXGRID,
			cr->state & CS_DEAD ? " dead" : "",
			cr->state & CS_SLIDETOKEN ? " slide-token" : "",
			cr->state & CS_REVERSE ? " reversing" : "");
    fflush(stderr);
}

static void verifymap(void)
{
    creature   *cr;
    int		pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (state->map[pos].floor >= Count_Floors)
	    die("%d: Undefined floor %d at (%d %d)",
		state->currenttime, state->map[pos].floor,
		pos % CXGRID, pos / CXGRID);
	if (state->map[pos].state & 0x80)
	    die("%d: Undefined floor state %02X at (%d %d)",
		state->currenttime, state->map[pos].state,
		pos % CXGRID, pos / CXGRID);
	if (isdecaying(state->map[pos].floor)
				&& (state->map[pos].state & 0x1F) > 12)
	    die("%d: Excessive floor decay %02X at (%d %d)",
		state->currenttime, state->map[pos].state,
		pos % CXGRID, pos / CXGRID);
    }

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->id >= Count_Entities)
	    die("%d: Undefined creature %d:%d at (%d %d)",
		state->currenttime, cr - creaturelist(), cr->id,
		cr->pos % CXGRID, cr->pos / CXGRID);
	if (cr->pos < 0 || cr->pos >= CXGRID * CYGRID)
	    die("%d: Creature %d:%d has left the map: %04X",
		state->currenttime, cr - creaturelist(), cr->id, cr->pos);
	if (cr->dir > EAST && (cr->dir != (uchar)NIL || cr->id != Block))
	    die("%d: Creature %d:%d moving in illegal direction (%d)",
		state->currenttime, cr - creaturelist(), cr->id, cr->dir);
	if (cr->dir == (uchar)NIL && cr->id != Block)
	    die("%d: Creature %d:%d lacks direction",
		state->currenttime, cr - creaturelist(), cr->id);
	if (cr->state & 0xF0)
	    die("%d: Creature %d:%d in an illegal state %X",
		state->currenttime, cr - creaturelist(), cr->id, cr->state);
	if (cr->moving > 8)
	    die("%d: Creature %d:%d has a moving time of %d",
		state->currenttime, cr - creaturelist(), cr->id, cr->moving);
	if (cr->moving < 0)
	    warn("%d: Creature %d:%d has a negative moving time: %d",
		 state->currenttime, cr - creaturelist(), cr->id, cr->moving);
    }
}

/*
 * General maintenance functions.
 */

static void initialhousekeeping(void)
{
    if (greenbuttons & 1)
	togglewalls();
    greenbuttons = 0;

    if (state->currentinput == CmdDebugCmd2) {
	dumpmap();
	exit(0);
    } else if (state->currentinput == CmdDebugCmd1) {
	static int mark = 0;
	warn("Mark %d.", ++mark);
	state->currentinput = NIL;
    }
    verifymap();
}

static void finalhousekeeping(void)
{
    creature   *cr;
    int		floor, pos;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->state & CS_DEAD)
	    continue;
	if (cr->state & CS_REVERSE) {
	    cr->state &= ~CS_REVERSE;
	    if (cr->moving <= 0)
		cr->dir = back(cr->dir);
	}
    }

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	floor = floorat(pos);
	if (isdecaying(floor) && !decrcounter(pos)) {
	    switch (floor) {
	      case Water_Splash:	floorat(pos) = Water;	break;
	      case Dirt_Splash:		floorat(pos) = Dirt;	break;
	      case Bomb_Explosion:	floorat(pos) = Empty;	break;
	    }
	}
    }

    if (floorat(chippos()) == HintButton && chipmoving() <= 0)
	displayflags() |= DF_SHOWHINT;
    else
	displayflags() &= ~DF_SHOWHINT;
}

static int checkforending(void)
{
    if (chipstate() & CS_DEAD)
	return -1;
    if (completed())
	return +1;
    return 0;
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

int initgamelogic(gamestate *pstate)
{
    uchar	layer1[CXGRID * CYGRID];
    uchar	layer2[CXGRID * CYGRID];
    creature   *cr;
    gamesetup  *game;
    int		pos, n;

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

    cr = creaturelist();

    cr->id = Nobody;
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (layer2[pos])
	    floorat(pos) = fileids[layer2[pos]].id;
	else if (fileids[layer1[pos]].isfloor)
	    floorat(pos) = fileids[layer1[pos]].id;
	else
	    floorat(pos) = Empty;
	if (!fileids[layer1[pos]].isfloor && fileids[layer1[pos]].id == Chip) {
	    if (cr->id)
		die("Multiple Chips on the map!!");
	    cr->pos = pos;
	    cr->id = Chip;
	    cr->dir = fileids[layer1[pos]].dir;
	    cr->moving = 0;
	    cr->state = 0x04;
	}
    }
    if (cr->id == Nobody)
	die("Chip isn't on the map!!");

    ++cr;
    for (n = 0 ; n < game->creaturecount ; ++n, ++cr) {
	cr->pos = game->creatures[n];
	cr->id = fileids[layer1[cr->pos]].id;
	cr->dir = fileids[layer1[cr->pos]].dir;
	cr->moving = 0;
	cr->state = 0;
	claimlocation(cr->pos);
    }
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (!fileids[layer1[pos]].isfloor
			&& fileids[layer1[pos]].id == Block) {
	    cr->pos = pos;
	    cr->id = fileids[layer1[pos]].id;
	    cr->dir = fileids[layer1[pos]].dir;
	    cr->moving = 0;
	    cr->state = 0;
	    claimlocation(pos);
	    ++cr;
	}
    }
    cr->pos = -1;
    cr->id = Nobody;
    cr->dir = NIL;

    chipsneeded() = game->chips;
    possession(Key_Blue) = possession(Key_Green)
			 = possession(Key_Red)
			 = possession(Key_Yellow) = 0;
    possession(Boots_Slide) = possession(Boots_Ice)
			    = possession(Boots_Water)
			    = possession(Boots_Fire) = 0;

    addsoundeffect(NULL);
    displayflags() = 0;
    completed() = FALSE;

    greenbuttons = 0;
    if (state->rndslidedir == NIL)
	state->rndslidedir = rndslidedir;
    else
	rndslidedir = state->rndslidedir;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (isice(floorat(cr->pos)) && !canmakemove(cr, cr->dir, 0, FALSE)) {
	     cr->dir = back(cr->dir);
	     applyicewallturn(cr);
	}
    }

    return TRUE;
}

/*
 * The central function.
 */

int advancegame(gamestate *pstate)
{
    creature   *cr;

    setstate(pstate);

    initialhousekeeping();

    for (cr = creaturelist() ; cr->id ; ++cr) ;
    for (--cr ; cr >= creaturelist() ; --cr) {
	cr->fdir = NIL;
	cr->tdir = NIL;
	if (cr->state & CS_DEAD)
	    continue;
	if (cr->moving <= 0)
	    choosemove(cr);
    }

    for (cr = creaturelist() ; cr->id ; ++cr) ;
    for (--cr ; cr >= creaturelist() ; --cr) {
	if (cr->state & CS_DEAD)
	    continue;
	advancecreature(cr, FALSE);
	cr->fdir = NIL;
	cr->tdir = NIL;
	if (floorat(cr->pos) == Button_Brown && cr->moving <= 0)
	    springtrap(trapfrombutton(cr->pos));
    }

    finalhousekeeping();

    return checkforending();
}
