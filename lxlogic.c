/* lxlogic.c: The game logic for the Lynx ruleset.
 *
 * Copyright (C) 2001,2002 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"defs.h"
#include	"err.h"
#include	"state.h"
#include	"random.h"
#include	"logic.h"

/* The walker's PRNG seed value. Don't change this; it needs to
 * remain forever constant.
 */
#define	WALKER_PRNG_SEED	105977040UL

#undef assert
#define	assert(test)	((test) || (die("internal error: failed sanity check" \
				        " (%s)\nPlease report this error to"  \
				        " breadbox@muppetlabs.com", #test), 0))

/* Internal status flags.
 */
#define	SF_ENDGAMETIMERBITS	0x000F	/* end-game countdown timer */
#define	SF_GREENTOGGLE		0x0010	/* green button has been toggled */
#define	SF_COULDNTMOVE		0x0020	/* can't-move sound has been played */
#define	SF_CHIPPUSHING		0x0040	/* Chip is pushing against something */

/* Declarations of (indirectly recursive) functions.
 */
static int canmakemove(creature const *cr, int dir, int flags);
static int advancecreature(creature *cr, int releasing);

/* A pointer to the game state, used so that it doesn't have to be
 * passed to every single function.
 */
static gamestate *state;

/* The direction used the last time something stepped onto a random
 * slide floor.
 */
static int lastrndslidedir = NORTH;

/* The PRNG used for the walkers.
 */
static prng		walkerprng;

/* The memory used to hold the list of creatures.
 */
static creature	       *creaturearray = NULL;

#define	MAX_CREATURES	(CXGRID * CYGRID)

static int		xviewoffset, yviewoffset;

/*
 * Accessor macros for various fields in the game state. Many of the
 * macros can be used as an lvalue.
 */

#define	setstate(p)		(state = (p)->state)

#define	creaturelist()		(state->creatures)

#define	getchip()		(creaturelist())
#define	chippos()		(getchip()->pos)
#define	ischipalive()		(!getchip()->hidden)

#define	mainprng()		(&state->mainprng)

#define	timelimit()		(state->timelimit)
#define	currenttime()		(state->currenttime)
#define	currentinput()		(state->currentinput)
#define	lastmove()		(state->lastmove)
#define	xviewpos()		(state->xviewpos)
#define	yviewpos()		(state->yviewpos)

#define	inendgame()		(state->statusflags & SF_ENDGAMETIMERBITS)
#define	decrendgametimer()	(--state->statusflags & SF_ENDGAMETIMERBITS)
#define	startendgametimer()	(state->statusflags |= SF_ENDGAMETIMERBITS)

#define	iscompleted()		(state->statusflags & SF_COMPLETED)
#define	setcompleted()		(state->statusflags |= SF_COMPLETED)
#define	setnosaving()		(state->statusflags |= SF_NOSAVING)
#define	showhint()		(state->statusflags |= SF_SHOWHINT)
#define	hidehint()		(state->statusflags &= ~SF_SHOWHINT)
#define	isgreentoggleset()	(state->statusflags & SF_GREENTOGGLE)
#define	togglegreen()		(state->statusflags ^= SF_GREENTOGGLE)
#define	resetgreentoggle()	(state->statusflags &= ~SF_GREENTOGGLE)
#define	iscouldntmoveset()	(state->statusflags & SF_COULDNTMOVE)
#define	setcouldntmove()	(state->statusflags |= SF_COULDNTMOVE)
#define	resetcouldntmove()	(state->statusflags &= ~SF_COULDNTMOVE)
#define	ispushing()		(state->statusflags & SF_CHIPPUSHING)
#define	setpushing()		(state->statusflags |= SF_CHIPPUSHING)
#define	resetpushing()		(state->statusflags &= ~SF_CHIPPUSHING)

#define	chipsneeded()		(state->chipsneeded)

#define	addsoundeffect(sfx)	(state->soundeffects |= 1 << (sfx))
#define	stopsoundeffect(sfx)	(state->soundeffects &= ~(1 << (sfx)))

#define	floorat(pos)		(state->map[pos].top.id)

#define	claimlocation(pos)	(state->map[pos].top.state |= 0x40)
#define	removeclaim(pos)	(state->map[pos].top.state &= ~0x40)
#define	issomeoneat(pos)	(state->map[pos].top.state & 0x40)
#define	markanimated(pos)	(state->map[pos].top.state |= 0x20)
#define	clearanimated(pos)	(state->map[pos].top.state &= ~0x20)
#define	ismarkedanimated(pos)	(state->map[pos].top.state & 0x20)

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
	    lastrndslidedir = right(lastrndslidedir);
	return lastrndslidedir;
    }
    warn("Invalid floor %d handed to getslidedir()\n", floor);
    assert(!"getslidedir() called with an invalid object");
    return NIL;
}

/* Alter a creature's direction if they are at an ice wall.
 */
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

/* Find the location of a beartrap or cloner from one of its buttons.
 */
static int floorfrombutton(int floor, int buttonpos)
{
    int	pos;

    pos = buttonpos;
    do {
	++pos;
	if (pos >= CXGRID * CYGRID)
	    pos -= CXGRID * CYGRID;
	if (floorat(pos) == floor)
	    return pos;
    } while (pos != buttonpos);

    return -1;
}

#define	trapfrombutton(pos)	(floorfrombutton(Beartrap, (pos)))
#define	clonerfrombutton(pos)	(floorfrombutton(CloneMachine, (pos)))

/* Flip-flop the state of any and all toggle walls.
 */
static void togglewalls(void)
{
    int	pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos)
	if (floorat(pos) == SwitchWall_Open)
	    floorat(pos) = SwitchWall_Closed;
	else if (floorat(pos) == SwitchWall_Closed)
	    floorat(pos) = SwitchWall_Open;
}

static void resetfloorsounds(int includepushing)
{
    stopsoundeffect(SND_SKATING_FORWARD);
    stopsoundeffect(SND_SKATING_TURN);
    stopsoundeffect(SND_FIREWALKING);
    stopsoundeffect(SND_WATERWALKING);
    stopsoundeffect(SND_ICEWALKING);
    stopsoundeffect(SND_SLIDEWALKING);
    stopsoundeffect(SND_SLIDING);
    if (includepushing)
	stopsoundeffect(SND_BLOCK_MOVING);
}

/*
 * Functions that manage the list of entities.
 */

/* Creature state flags.
 */
#define	CS_SLIDETOKEN		0x01	/* can move off of a slide floor */
#define	CS_REVERSE		0x02	/* needs to turn around */
#define	CS_NOISYMOVEMENT	0x04	/* block was pushed by Chip */
#define	CS_NASCENT		0x08	/* creature was just added */

/* Return the creature located at pos. Ignores Chip unless includechip
 * is TRUE. (This is important in the case when Chip and a second
 * creature are currently occupying a single location.)
 */
static creature *lookupcreature(int pos, int includechip)
{
    creature   *cr;

    cr = creaturelist();
    if (!includechip)
	++cr;
    for ( ; cr->id ; ++cr)
	if (cr->pos == pos && !cr->hidden && !isanimation(cr->id))
	    return cr;
    return NULL;
}

/* Return a new creature.
 */
static creature *newcreature(void)
{
    creature   *cr;

    for (cr = creaturelist() + 1 ; cr->id ; ++cr) {
	if (cr->hidden)
	    return cr;
    }
    if (cr - creaturelist() + 1 >= MAX_CREATURES) {
	warn("Ran out of room in the creatures array!");
	return NULL;
    }
    cr->hidden = TRUE;

    cr[1].id = Nothing;
    return cr;
}

/* Mark a creature as dead.
 */
static void removecreature(creature *cr)
{
    cr->hidden = TRUE;
    if (cr->state & CS_NOISYMOVEMENT)
	stopsoundeffect(SND_BLOCK_MOVING);
    if (cr->id != Chip)
	removeclaim(cr->pos);
    if (isanimation(cr->id))
	clearanimated(cr->pos);
}

/* Turn around any and all tanks not currently inbetween moves.
 */
static void turntanks(void)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->hidden)
	    continue;
	if (cr->id == Tank && floorat(cr->pos) != CloneMachine)
	    cr->state ^= CS_REVERSE;
    }
}

/* Start an animation sequence at the given location.
 */
static creature *addanimation(int pos, int dir, int moving, int id, int seqlen)
{
    creature   *anim;

    anim = newcreature();
    if (!anim)
	return NULL;
    anim->id = id;
    anim->dir = dir;
    anim->pos = pos;
    anim->moving = moving;
    anim->frame = seqlen;
    anim->hidden = FALSE;
    anim->state = 0;
    anim->fdir = NIL;
    anim->tdir = NIL;
    markanimated(pos);
    return anim;
}

/* Abort the animation sequence occuring at the given location.
 */
static int stopanimation(int pos)
{
    creature   *anim;

    for (anim = creaturelist() ; anim->id ; ++anim) {
	if (anim->pos == pos && isanimation(anim->id)) {
	    removecreature(anim);
	    return TRUE;
	}
    }
    return FALSE;
}

/* What happens when Chip dies.
 */
static void removechip(int explode, creature *also)
{
    creature  *chip = getchip();

    if (explode) {
	addanimation(chip->pos, chip->dir, chip->moving, Entity_Explosion, 12);
	addsoundeffect(SND_DEREZZ);
    }
    if (also)
	addanimation(also->pos, also->dir, also->moving, Entity_Explosion, 12);
    resetfloorsounds(FALSE);
    startendgametimer();
    removecreature(chip);
    if (also)
	removecreature(also);
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
    /* Nothing */
    { 0, 0, 0 },
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
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* ICChip */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Key_Red */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Blue */
    { ALL_IN_OUT, ALL_IN_OUT, ALL_IN_OUT },
    /* Key_Yellow */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Key_Green */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Slide */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Ice */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Water */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Boots_Fire */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
    /* Block_Static */
    { 0, 0, 0 },
    /* Floor_Reserved3 */
    { 0, 0, 0 },
    /* Floor_Reserved2 */
    { 0, 0, 0 },
    /* Floor_Reserved1 */
    { 0, 0, 0 }
};

/* Including the flag CMM_RELEASING in a call to canmakemove indicates
 * that the creature in question is being moved out of a beartrap or
 * clone machine, moves that would normally be forbidden.
 * CMM_EXPOSEWALLS causes blue and hidden walls to be exposed in the
 * case of Chip. CMM_PUSHBLOCKS causes blocks to be pushed when in the
 * way of Chip.
 */
#define	CMM_EXPOSEWALLS		0x0001
#define	CMM_PUSHBLOCKS		0x0002
#define	CMM_RELEASING		0x0004

/* Return TRUE if the given block is allowed to be moved in the given
 * direction. If flags includes CMM_PUSHBLOCKS, then the indicated
 * movement of the block will be initiated.
 */
static int canpushblock(creature *block, int dir, int flags)
{
    assert(block && block->id == Block);
    assert(floorat(block->pos) != CloneMachine);
    assert(dir != NIL);

    if (!canmakemove(block, dir, flags)) {
	if (!block->moving && (flags & CMM_PUSHBLOCKS))
	    block->dir = dir;
	return FALSE;
    }
    if (flags & CMM_PUSHBLOCKS) {
	block->tdir = dir;
	if (advancecreature(block, FALSE)) {
	    setpushing();
	    addsoundeffect(SND_BLOCK_MOVING);
	    block->state |= CS_NOISYMOVEMENT;
	}
    }

    return TRUE;
}

/* Return TRUE if the given creature is allowed to attempt to move in
 * the given direction.
 */
static int canmakemove(creature const *cr, int dir, int flags)
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

    if ((floorfrom == Beartrap || floorfrom == CloneMachine)
					&& !(flags & CMM_RELEASING))
	return FALSE;

    if (isslide(floorfrom) && (cr->id != Chip || !possession(Boots_Slide)))
	if (getslidedir(floorfrom, FALSE) == back(dir))
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
	if (ismarkedanimated(to))
	    return FALSE;
	other = lookupcreature(to, FALSE);
	if (other && other->id == Block) {
	    if (!canpushblock(other, dir, flags & ~CMM_RELEASING))
		return FALSE;
	}
	if (floorto == HiddenWall_Temp || floorto == BlueWall_Real) {
	    if (flags & CMM_EXPOSEWALLS)
		floorat(to) = Wall;
	    return FALSE;
	}
    } else if (cr->id == Block) {
	if (cr->moving > 0)
	    return FALSE;
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
	if (floorto == Fire && cr->id != Fireball)
	    return FALSE;
    }

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

    if (isanimation(cr->id))
	return;
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

    assert(dir != NIL);

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
	choices[1] = randomof3(&walkerprng,
			       left(dir), back(dir), right(dir));
	break;
      case Blob:
	choices[0] = 1 << random4(mainprng());
	break;
      case Teeth:
	if ((currenttime() >> 2) & 1)
	    return;
	if (!ischipalive())
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
	if (canmakemove(cr, choices[n], 0))
	    return;
    }

    if (pdir != NIL)
	cr->tdir = pdir;
}

/* Resolve a diagonal movement command to one of the two directions.
 */
static int selectonemove(int vert, int horz)
{
    creature   *cr;
    int		fv, fh;

    cr = getchip();
    fv = canmakemove(cr, vert, CMM_EXPOSEWALLS | CMM_PUSHBLOCKS);
    fh = canmakemove(cr, horz, CMM_EXPOSEWALLS | CMM_PUSHBLOCKS);
    if (fv && !fh)
	return vert;
    return horz;
}

/* Determine the direction of Chip's next move. If discard is TRUE,
 * then Chip is not currently permitted to select a direction of
 * movement, and the player's input should not be retained.
 */
static void choosechipmove(creature *cr, int discard)
{
    int	dir;

    dir = currentinput();
    currentinput() = NIL;

    switch (dir) {
      case CmdNorth | CmdWest:
      case CmdNorth | CmdEast:
      case CmdSouth | CmdWest:
      case CmdSouth | CmdEast:
	dir = selectonemove(dir & (CmdNorth | CmdSouth),
			    dir & (CmdWest | CmdEast));
	break;
    }

    if (dir == NIL || discard) {
	cr->tdir = NIL;
    } else {
	lastmove() = dir;
	cr->tdir = dir;
    }
    if (cr->tdir == NIL)
	resetpushing();
}

/* This function determines if the given creature is currently being
 * forced to move. (Ice, slide floors, and teleports are the three
 * possible causes of this. Bear traps and clone machines also cause
 * forced movement, but these are handled outside of the normal
 * movement sequence.) If so, the direction is stored in the
 * creature's fdir field, and TRUE is returned unless the creature can
 * override the forced move.
 */
static int getforcedmove(creature *cr)
{
    int	floor, dir;

    cr->fdir = NIL;

    floor = floorat(cr->pos);

    if (isice(floor)) {
	if (cr->id == Chip && possession(Boots_Ice))
	    return FALSE;
	dir = cr->dir;
	if (dir == NIL)
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

/* Return the move a creature will make on the current tick.
 */
static int choosemove(creature *cr)
{
    if (cr->id == Chip) {
	choosechipmove(cr, getforcedmove(cr));
	if (cr->tdir == NIL && cr->fdir == NIL)
	    resetfloorsounds(FALSE);
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

/* Teleport the given creature instantaneously from one teleport tile
 * to another.
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
	    n = canmakemove(cr, cr->dir, 0);
	    cr->pos = origpos;
	    if (n)
		break;
	    if (pos == origpos) {
		if (cr->id == Chip) {
		    addsoundeffect(SND_CANT_MOVE);
		    setpushing();
		}
		return TRUE;
	    }
	}
    }

    if (cr->id != Chip)
	removeclaim(cr->pos);
    cr->pos = pos;
    if (cr->id != Chip)
	claimlocation(cr->pos);
    if (cr->id == Chip)
	addsoundeffect(SND_TELEPORTING);
    return TRUE;
}

/* Cause a clone machine to produce a clone.
 */
static int activatecloner(int pos)
{
    creature   *cr;
    creature   *clone;

    assert(pos >= 0);
    assert(floorat(pos) == CloneMachine);
    cr = lookupcreature(pos, TRUE);
    if (!cr)
	return FALSE;
    clone = newcreature();
    if (!clone)
	return FALSE;
    *clone = *cr;
    if (!advancecreature(cr, TRUE)) {
	if (!cr->hidden)
	    clone->hidden = TRUE;
	return FALSE;
    }
    return TRUE;
}

/* Release any creature on a beartrap at the given location.
 */
static void springtrap(int pos)
{
    creature   *cr;

    cr = lookupcreature(pos, TRUE);
    if (cr && cr->dir != NIL)
	advancecreature(cr, TRUE);
}

/*
 * When something actually moves.
 */

/* Initiate a move by the given creature. The direction of movement is
 * given by the tdir field, or the fdir field if tdir is NIL.
 * releasing must be TRUE if the creature is moving out of a bear trap
 * or clone machine.
 */
static int startmovement(creature *cr, int releasing)
{
    static int const	delta[] = { 0, -CXGRID, -1, 0, +CXGRID, 0, 0, 0, +1 };
    int			dir;
    int			floorfrom;

    assert(cr->moving <= 0);

    if (cr->tdir != NIL)
	dir = cr->tdir;
    else if (cr->fdir != NIL)
	dir = cr->fdir;
    else
	return FALSE;

    cr->dir = dir;

    floorfrom = floorat(cr->pos);

    if (cr->id == Chip) {
	assert(ischipalive());
	resetpushing();
	if (!possession(Boots_Slide)) {
	    if (isslide(floorfrom) && cr->tdir == NIL)
		cr->state |= CS_SLIDETOKEN;
	    else if (!isice(floorfrom) || possession(Boots_Ice))
		cr->state &= ~CS_SLIDETOKEN;
	}
    }

    if (!canmakemove(cr, dir, CMM_EXPOSEWALLS | CMM_PUSHBLOCKS
		     | (releasing ? CMM_RELEASING : 0))) {
	if (isice(floorfrom) && (cr->id != Chip || !possession(Boots_Ice))) {
	    cr->dir = back(dir);
	    applyicewallturn(cr);
	}
	if (cr->id == Chip) {
	    if (!iscouldntmoveset()) {
		setcouldntmove();
		addsoundeffect(SND_CANT_MOVE);
	    }
	    setpushing();
	}
	return FALSE;
    }

    if (floorfrom == CloneMachine || floorfrom == Beartrap)
	assert(releasing);

    if (cr->id != Chip)
	removeclaim(cr->pos);
    cr->pos += delta[dir];
    if (ismarkedanimated(cr->pos))
	stopanimation(cr->pos);
    if (cr->id != Chip)
	claimlocation(cr->pos);

    cr->moving += 8;

    if (cr->id != Chip && ischipalive() && cr->pos == chippos()) {
	removechip(TRUE, cr);
	return FALSE;
    }
    if (cr->id == Chip) {
	resetcouldntmove();
	cr = lookupcreature(cr->pos, FALSE);
	if (cr) {
	    removechip(TRUE, cr);
	    return FALSE;
	}
    }

    return TRUE;
}

/* Determine the speed of a moving creature. (Speed is measured in
 * eighths of a tile per tick.)
 */
static int continuemovement(creature *cr)
{
    int	floor, speed;

    if (isanimation(cr->id))
	return TRUE;

    assert(cr->moving > 0);

    speed = cr->id == Blob ? 1 : 2;
    floor = floorat(cr->pos);
    if (isslide(floor) && (cr->id != Chip || !possession(Boots_Slide)))
	speed *= 2;
    else if (isice(floor) && (cr->id != Chip || !possession(Boots_Ice)))
	speed *= 2;
    cr->moving -= speed;
    cr->frame = cr->moving / 2;
    return cr->moving > 0;
}

/* Complete the movement of the given creature. Most side effects
 * produced by moving onto a tile occur at this point.
 */
static int endmovement(creature *cr)
{
    int	floor;

    assert(cr->moving <= 0);

    if (isanimation(cr->id))
	return TRUE;

    floor = floorat(cr->pos);

    if (cr->id != Chip || !possession(Boots_Ice))
	applyicewallturn(cr);

    if (cr->id == Chip) {
	switch (floor) {
	  case Water:
	    if (!possession(Boots_Water)) {
		addanimation(cr->pos, NIL, 0, Water_Splash, 12);
		addsoundeffect(SND_WATER_SPLASH);
		removechip(FALSE, NULL);
	    }
	    break;
	  case Fire:
	    if (!possession(Boots_Fire))
		removechip(TRUE, NULL);
	    break;
	  case Dirt:
	  case BlueWall_Fake:
	    floorat(cr->pos) = Empty;
	    addsoundeffect(SND_TILE_EMPTIED);
	    break;
	  case PopupWall:
	    floorat(cr->pos) = Wall;
	    addsoundeffect(SND_WALL_CREATED);
	    break;
	  case Door_Red:
	  case Door_Blue:
	  case Door_Yellow:
	  case Door_Green:
	    assert(possession(floor));
	    if (floor != Door_Green)
		--possession(floor);
	    floorat(cr->pos) = Empty;
	    addsoundeffect(SND_DOOR_OPENED);
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
	    floorat(cr->pos) = Empty;
	    addsoundeffect(SND_ITEM_COLLECTED);
	    break;
	  case Burglar:
	    possession(Boots_Ice) = 0;
	    possession(Boots_Slide) = 0;
	    possession(Boots_Fire) = 0;
	    possession(Boots_Water) = 0;
	    addsoundeffect(SND_BOOTS_STOLEN);
	    break;
	  case ICChip:
	    if (chipsneeded())
		--chipsneeded();
	    floorat(cr->pos) = Empty;
	    addsoundeffect(SND_ITEM_COLLECTED);
	    break;
	  case Socket:
	    assert(chipsneeded() == 0);
	    floorat(cr->pos) = Empty;
	    addsoundeffect(SND_SOCKET_OPENED);
	    break;
	  case Exit:
	    setcompleted();
	    break;
	}
    } else if (cr->id == Block) {
	switch (floor) {
	  case Water:
	    floorat(cr->pos) = Dirt;
	    addanimation(cr->pos, NIL, 0, Dirt_Splash, 12);
	    addsoundeffect(SND_WATER_SPLASH);
	    removecreature(cr);
	    break;
	  case Key_Blue:
	    floorat(cr->pos) = Empty;
	    break;
	}
    } else {
	switch (floor) {
	  case Water:
	    if (cr->id != Glider) {
		addanimation(cr->pos, NIL, 0, Water_Splash, 12);
		addsoundeffect(SND_WATER_SPLASH);
		removecreature(cr);
	    }
	    break;
	  case Key_Blue:
	    floorat(cr->pos) = Empty;
	    break;
	}
    }

    switch (floor) {
      case Bomb:
	floorat(cr->pos) = Empty;
	addanimation(cr->pos, NIL, 0, Bomb_Explosion, 10);
	addsoundeffect(SND_BOMB_EXPLODES);
	if (cr->id == Chip)
	    removechip(FALSE, NULL);
	else
	    removecreature(cr);
	break;
      case Teleport:
	teleportcreature(cr);
	break;
      case Beartrap:
	addsoundeffect(SND_TRAP_ENTERED);
	break;
      case Button_Blue:
	turntanks();
	addsoundeffect(SND_BUTTON_PUSHED);
	break;
      case Button_Green:
	togglegreen();
	addsoundeffect(SND_BUTTON_PUSHED);
	break;
      case Button_Red:
	if (activatecloner(clonerfrombutton(cr->pos)))
	    addsoundeffect(SND_BUTTON_PUSHED);
	break;
      case Button_Brown:
	addsoundeffect(SND_BUTTON_PUSHED);
	break;
    }

    return TRUE;
}

/* Advance the movement of the given creature. If the creature is not
 * currently moving but should be, movement is initiated. If the
 * creature completes their movement, any and all appropriate side
 * effects are applied. If releasing is TRUE, the movement is occuring
 * out-of-turn, as with movement across an open beatrap or an
 * activated clone machine.
 */
static int advancecreature(creature *cr, int releasing)
{
    char	tdir = NIL;

    if (cr->moving <= 0) {
	if (releasing) {
	    assert(cr->dir != NIL);
	    tdir = cr->tdir;
	    cr->tdir = cr->dir;
	} else if (cr->tdir == NIL && cr->fdir == NIL)
	    return TRUE;
	if (!startmovement(cr, releasing)) {
	    if (releasing)
		cr->tdir = tdir;
	    return FALSE;
	}
	cr->tdir = NIL;
    }
    if (!continuemovement(cr))
	endmovement(cr);
    return TRUE;
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
	    fprintf(stderr, "%02X%c", state->map[y + x].top.id,
		    (state->map[y + x].top.state ?
		     state->map[y + x].top.state & 0x40 ? '*' : '.' : ' '));
	fputc('\n', stderr);
    }
    fputc('\n', stderr);
    for (cr = creaturelist() ; cr->id ; ++cr)
	fprintf(stderr, "%02X%c%1d (%d %d)%s%s%s\n",
			cr->id, "-^<?v???>"[(int)cr->dir],
			cr->moving, cr->pos % CXGRID, cr->pos / CXGRID,
			cr->hidden ? " dead" : "",
			cr->state & CS_SLIDETOKEN ? " slide-token" : "",
			cr->state & CS_REVERSE ? " reversing" : "");
    fflush(stderr);
}

/* Run various sanity checks on the current game state.
 */
static void verifymap(void)
{
    creature   *cr;
    int		pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (state->map[pos].top.id >= 0x40)
	    die("%d: Undefined floor %d at (%d %d)",
		currenttime(), state->map[pos].top.id,
		pos % CXGRID, pos / CXGRID);
	if (state->map[pos].top.state & 0x80)
	    die("%d: Undefined floor state %02X at (%d %d)",
		currenttime(), state->map[pos].top.id,
		pos % CXGRID, pos / CXGRID);
    }

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (isanimation(state->map[pos].top.id)) {
	    if (cr->moving > 12)
		die("%d: Too-large animation frame %02X at (%d %d)",
		    currenttime(), cr->moving, pos % CXGRID, pos / CXGRID);
	    continue;
	}
	if (cr->id < 0x40 || cr->id >= 0x80)
	    die("%d: Undefined creature %d:%d at (%d %d)",
		currenttime(), cr - creaturelist(), cr->id,
		cr->pos % CXGRID, cr->pos / CXGRID);
	if (cr->pos < 0 || cr->pos >= CXGRID * CYGRID)
	    die("%d: Creature %d:%d has left the map: %04X",
		currenttime(), cr - creaturelist(), cr->id, cr->pos);
	if (isanimation(cr->id))
	    continue;
	if (cr->dir > EAST && (cr->dir != NIL || cr->id != Block))
	    die("%d: Creature %d:%d moving in illegal direction (%d)",
		currenttime(), cr - creaturelist(), cr->id, cr->dir);
	if (cr->dir == NIL && cr->id != Block)
	    die("%d: Creature %d:%d lacks direction",
		currenttime(), cr - creaturelist(), cr->id);
	if (cr->state & 0xF0)
	    die("%d: Creature %d:%d in an illegal state %X",
		currenttime(), cr - creaturelist(), cr->id, cr->state);
	if (cr->moving > 8)
	    die("%d: Creature %d:%d has a moving time of %d",
		currenttime(), cr - creaturelist(), cr->id, cr->moving);
	if (cr->moving < 0)
	    warn("%d: Creature %d:%d has a negative moving time: %d",
		 currenttime(), cr - creaturelist(), cr->id, cr->moving);
    }
}

/*
 * Per-tick maintenance functions.
 */

/* Actions and checks that occur at the start of every tick.
 */
static void initialhousekeeping(void)
{
    creature   *chip;
    creature   *cr;

    if (currenttime() == 0) {
	if (state->initrndslidedir == NIL)
	    state->initrndslidedir = lastrndslidedir;
	else
	    lastrndslidedir = state->initrndslidedir;
    }

    chip = getchip();
    if (chip->id == Pushing_Chip)
	chip->id = Chip;

    if (isgreentoggleset())
	togglewalls();
    resetgreentoggle();

    if (currentinput() == CmdDebugCmd2) {
	dumpmap();
	exit(0);
	currentinput() = NIL;
    } else if (currentinput() == CmdDebugCmd1) {
	static int mark = 0;
	warn("Mark %d (%d).", ++mark, currenttime());
	currentinput() = NIL;
    }
    if (currentinput() >= CmdCheatNorth && currentinput() <= CmdCheatICChip) {
	switch (currentinput()) {
	  case CmdCheatNorth:		--yviewoffset;			break;
	  case CmdCheatWest:		--xviewoffset;			break;
	  case CmdCheatSouth:		++yviewoffset;			break;
	  case CmdCheatEast:		++xviewoffset;			break;
	  case CmdCheatHome:		xviewoffset = yviewoffset = 0;	break;
	  case CmdCheatKeyRed:		++possession(Key_Red);		break;
	  case CmdCheatKeyBlue:		++possession(Key_Blue);		break;
	  case CmdCheatKeyYellow:	++possession(Key_Yellow);	break;
	  case CmdCheatKeyGreen:	++possession(Key_Green);	break;
	  case CmdCheatBootsIce:	++possession(Boots_Ice);	break;
	  case CmdCheatBootsSlide:	++possession(Boots_Slide);	break;
	  case CmdCheatBootsFire:	++possession(Boots_Fire);	break;
	  case CmdCheatBootsWater:	++possession(Boots_Water);	break;
	  case CmdCheatICChip:	if (chipsneeded()) --chipsneeded();	break;
	}
	currentinput() = NIL;
	setnosaving();
    }

    verifymap();

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (!cr->hidden && isanimation(cr->id) && cr->frame <= 0)
	    removecreature(cr);
	if (cr->state & CS_NOISYMOVEMENT) {
	    if (cr->hidden || cr->moving <= 0) {
		stopsoundeffect(SND_BLOCK_MOVING);
		cr->state &= ~CS_NOISYMOVEMENT;
	    }
	}
    }
}

/* Actions and checks that occur at the end of every tick.
 */
static void finalhousekeeping(void)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->hidden)
	    continue;
	if (isanimation(cr->id))
	    --cr->frame;
	if (cr->state & CS_REVERSE) {
	    cr->state &= ~CS_REVERSE;
	    if (cr->moving <= 0)
		cr->dir = back(cr->dir);
	}
    }
}

/* Set the state fields specifically used to produce the output.
 */
static void preparedisplay(void)
{
    creature   *chip;
    int		floor;

    chip = getchip();
    floor = floorat(chip->pos);

    xviewpos() = (chip->pos % CXGRID) * 8 + xviewoffset * 8;
    yviewpos() = (chip->pos / CXGRID) * 8 + yviewoffset * 8;
    if (chip->moving) {
	switch (chip->dir) {
	  case NORTH:	yviewpos() += chip->moving;	break;
	  case WEST:	xviewpos() += chip->moving;	break;
	  case SOUTH:	yviewpos() -= chip->moving;	break;
	  case EAST:	xviewpos() -= chip->moving;	break;
	}
    }
    if (ischipalive() && floor == HintButton && chip->moving <= 0)
	showhint();
    else
	hidehint();

    if (ispushing())
	chip->id = Pushing_Chip;

    if (chip->moving) {
	resetfloorsounds(FALSE);
	floor = floorat(chip->pos);
	if (floor == Fire && possession(Boots_Fire))
	    addsoundeffect(SND_FIREWALKING);
	else if (floor == Water && possession(Boots_Water))
	    addsoundeffect(SND_WATERWALKING);
	else if (isice(floor)) {
	    if (possession(Boots_Ice))
		addsoundeffect(SND_ICEWALKING);
	    else if (floor == Ice)
		addsoundeffect(SND_SKATING_FORWARD);
	    else
		addsoundeffect(SND_SKATING_TURN);
	} else if (isslide(floor)) {
	    if (possession(Boots_Slide))
		addsoundeffect(SND_SLIDEWALKING);
	    else
		addsoundeffect(SND_SLIDING);
	}
    }
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
/* 0E cloning block N		*/	{ FALSE, Block,		    NORTH },
/* 0F cloning block W		*/	{ FALSE, Block,		    WEST },
/* 10 cloning block S		*/	{ FALSE, Block,		    SOUTH },
/* 11 cloning block E		*/	{ FALSE, Block,		    EAST },
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
/* 20 not used (overlay buffer)	*/	{ TRUE,  NIL,		    NIL },
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
/* 34 burned Chip in fire	*/	{ TRUE,  NIL,		    NIL },
/* 35 burned Chip		*/	{ TRUE,  NIL,		    NIL },
/* 36 not used			*/	{ TRUE,  NIL,		    NIL },
/* 37 not used			*/	{ TRUE,  NIL,		    NIL },
/* 38 not used			*/	{ TRUE,  NIL,		    NIL },
/* 39 Chip in exit		*/	{ TRUE,  NIL,		    NIL },
/* 3A exit (frame 2)		*/	{ TRUE,  Exit,		    NIL },
/* 3B exit (frame 3)		*/	{ TRUE,  Exit,		    NIL },
/* 3C Chip swimming N		*/	{ TRUE,  NIL,		    NIL },
/* 3D Chip swimming W		*/	{ TRUE,  NIL,		    NIL },
/* 3E Chip swimming S		*/	{ TRUE,  NIL,		    NIL },
/* 3F Chip swimming E		*/	{ TRUE,  NIL,		    NIL },
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
 * The level map is decoded and assembled, the list of creatures is
 * drawn up, and other miscellaneous initializations are performed.
 */
static int initgame(gamelogic *logic)
{
    unsigned char	layer1[CXGRID * CYGRID];
    unsigned char	layer2[CXGRID * CYGRID];
    creature		crtemp;
    creature	       *cr;
    gamesetup	       *game;
    int			pos, n;

    setstate(logic);

    game = state->game;

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

    creaturelist() = creaturearray;

    cr = creaturelist();

    n = -1;
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	state->map[pos].bot.id = Empty;
	if (layer2[pos])
	    floorat(pos) = fileids[layer2[pos]].id;
	else if (fileids[layer1[pos]].isfloor)
	    floorat(pos) = fileids[layer1[pos]].id;
	else
	    floorat(pos) = Empty;
	if (!fileids[layer1[pos]].isfloor) {
	    cr->pos = pos;
	    cr->id = fileids[layer1[pos]].id;
	    cr->dir = fileids[layer1[pos]].dir;
	    cr->moving = 0;
	    cr->hidden = FALSE;
	    if (cr->id == Chip) {
		if (n >= 0)
		    die("Multiple Chips on the map!");
		n = cr - creaturelist();
		cr->state = CS_SLIDETOKEN;
	    } else {
		cr->state = 0;
		claimlocation(pos);
	    }
	    cr->fdir = NIL;
	    cr->tdir = NIL;
	    cr->frame = 0;
	    ++cr;
	}
    }
    if (n < 0)
	die("Chip isn't on the map!");
    cr->pos = -1;
    cr->id = Nothing;
    cr->dir = NIL;

    if (n) {
	cr = creaturelist();
	crtemp = cr[0];
	cr[0] = cr[n];
	cr[n] = crtemp;
    }

    chipsneeded() = game->chips;
    possession(Key_Red) = possession(Key_Blue)
			= possession(Key_Yellow)
			= possession(Key_Green) = 0;
    possession(Boots_Ice) = possession(Boots_Slide)
			  = possession(Boots_Fire)
			  = possession(Boots_Water) = 0;

    resetgreentoggle();
    restartprng(&walkerprng, WALKER_PRNG_SEED);

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (isice(floorat(cr->pos)) && cr->dir != NIL
				    && !canmakemove(cr, cr->dir, 0)) {
	     cr->dir = back(cr->dir);
	     applyicewallturn(cr);
	}
    }

    xviewoffset = yviewoffset = 0;

    preparedisplay();
    return TRUE;
}

/* Advance the game state by one tick.
 */
static int advancegame(gamelogic *logic)
{
    creature   *cr;

    setstate(logic);

    if (!inendgame() && timelimit() && currenttime() >= timelimit())
	removechip(TRUE, NULL);

    initialhousekeeping();

    for (cr = creaturelist() ; cr->id ; ++cr) ;
    for (--cr ; cr >= creaturelist() ; --cr) {
	cr->fdir = NIL;
	cr->tdir = NIL;
	if (cr->hidden || isanimation(cr->id))
	    continue;
	if (cr->moving <= 0)
	    choosemove(cr);
    }

    if (getchip()->fdir == NIL && getchip()->tdir == NIL)
	resetcouldntmove();

    for (cr = creaturelist() ; cr->id ; ++cr) ;
    for (--cr ; cr >= creaturelist() ; --cr) {
	if (cr->hidden)
	    continue;
	advancecreature(cr, FALSE);
	cr->fdir = NIL;
	cr->tdir = NIL;
	if (floorat(cr->pos) == Button_Brown && cr->moving <= 0)
	    springtrap(trapfrombutton(cr->pos));
    }

    finalhousekeeping();

    preparedisplay();

    if (inendgame()) {
	if (!decrendgametimer()) {
	    resetfloorsounds(TRUE);
	    return -1;
	}
    } else {
	if (iscompleted()) {
	    resetfloorsounds(TRUE);
	    addsoundeffect(SND_CHIP_WINS);
	    return +1;
	}
    }

    return 0;
}

static int endgame(gamelogic *logic)
{
    (void)logic;
    return TRUE;
}

static void shutdown(gamelogic *logic)
{
    (void)logic;
    free(creaturearray);
    creaturearray = NULL;
}

gamelogic *lynxlogicstartup(void)
{
    static gamelogic	logic;

    creaturearray = calloc(MAX_CREATURES, sizeof *creaturearray);
    if (!creaturearray)
	memerrexit();
    lastrndslidedir = NORTH;
    xviewoffset = 0;
    yviewoffset = 0;

    logic.ruleset = Ruleset_Lynx;
    logic.initgame = initgame;
    logic.advancegame = advancegame;
    logic.endgame = endgame;
    logic.shutdown = shutdown;

    return &logic;
}
