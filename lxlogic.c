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

/* A number well above the maximum number of creatures that could possibly
 * exist simultaneously.
 */
#define	MAX_CREATURES	(2 * CXGRID * CYGRID)

/* The walker's PRNG seed value. Don't change this; it needs to
 * remain forever constant.
 */
#define	WALKER_PRNG_SEED	105977040UL

#ifdef NDEBUG
#define	_assert(test)
#else
#define	_assert(test)	((test) || (die("internal error: failed sanity check" \
				        " (%s)\nPlease report this error to"  \
				        " breadbox@muppetlabs.com", #test), 0))
#endif

/* A list of ways for Chip to lose.
 */
enum {
    CHIP_OKAY = 0,
    CHIP_DROWNED, CHIP_BURNED, CHIP_BOMBED, CHIP_OUTOFTIME, CHIP_COLLIDED,
    CHIP_NOTOKAY
};

/* Status information specific to the Lynx game logic.
 */
struct lxstate {
    unsigned char	greentoggle;	/* green button has been toggled */
    unsigned char	couldntmove;	/* can't-move sound has been played */
    unsigned char	pushing;	/* Chip is pushing against something */
    unsigned char	completed;	/* level completed successfully */
    prng		walkerprng;	/* the PRNG used for the walkers */
    creature	       *chiptocr;	/* is Chip colliding with a creature */
    short		chiptopos;	/*   just starting to move itself? */
    signed char		xviewoffset;	/* offset of map view center */
    signed char		yviewoffset;	/*   position from position of Chip */
    unsigned char	endgametimer;	/* end-game countdown timer */
};

/* Declarations of (indirectly recursive) functions.
 */
static int canmakemove(creature const *cr, int dir, int flags);
static int advancecreature(creature *cr, int releasing);

/* Used to calculate movement offsets.
 */
static int const	delta[] = { 0, -CXGRID, -1, 0, +CXGRID, 0, 0, 0, +1 };

/* The direction used the last time something stepped onto a random
 * slide floor.
 */
static int		lastrndslidedir = NORTH;

/* The memory used to hold the list of creatures.
 */
static creature	       *creaturearray = NULL;

/* A pointer to the game state, used so that it doesn't have to be
 * passed to every single function.
 */
static gamestate       *state;

/*
 * Accessor macros for various fields in the game state. Many of the
 * macros can be used as an lvalue.
 */

#define	setstate(p)		(state = (p)->state)

#define	creaturelist()		(state->creatures)

#define	getchip()		(creaturelist())
#define	chippos()		(getchip()->pos)

#define	mainprng()		(&state->mainprng)

#define	timelimit()		(state->timelimit)
#define	timeoffset()		(state->timeoffset)
#define	currenttime()		(state->currenttime)
#define	currentinput()		(state->currentinput)
#define	lastmove()		(state->lastmove)
#define	xviewpos()		(state->xviewpos)
#define	yviewpos()		(state->yviewpos)

#define	setnosaving()		(state->statusflags |= SF_NOSAVING)
#define	showhint()		(state->statusflags |= SF_SHOWHINT)
#define	hidehint()		(state->statusflags &= ~SF_SHOWHINT)
#define	markinvalid()		(state->statusflags |= SF_INVALID)
#define	ismarkedinvalid()	(state->statusflags & SF_INVALID)

#define	chipsneeded()		(state->chipsneeded)

#define	clonerlist()		(state->game->cloners)
#define	clonerlistsize()	(state->game->clonercount)
#define	traplist()		(state->game->traps)
#define	traplistsize()		(state->game->trapcount)

#define	getlxstate()		((struct lxstate*)state->localstateinfo)

#define	completed()		(getlxstate()->completed)
#define	greentoggle()		(getlxstate()->greentoggle)
#define	couldntmove()		(getlxstate()->couldntmove)
#define	chippushing()		(getlxstate()->pushing)
#define	chiptopos()		(getlxstate()->chiptopos)
#define	chiptocr()		(getlxstate()->chiptocr)
#define	xviewoffset()		(getlxstate()->xviewoffset)
#define	yviewoffset()		(getlxstate()->yviewoffset)
#define	walkerprng()		(&(getlxstate()->walkerprng))

#define	inendgame()		(getlxstate()->endgametimer)
#define	startendgametimer()	(getlxstate()->endgametimer = 12 + 1)
#define	decrendgametimer()	(--getlxstate()->endgametimer)
#define	resetendgametimer()	(getlxstate()->endgametimer = 0)

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
    _assert(!"possession() called with an invalid object");
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
    _assert(!"getslidedir() called with an invalid object");
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

/* Find the location of a beartrap from one of its buttons.
 */
static int trapfrombutton(int pos)
{
    xyconn     *xy;
    int		i;

    for (xy = traplist(), i = traplistsize() ; i ; ++xy, --i)
	if (xy->from == pos)
	    return xy->to;
    warn("unconnected trap button at (%d %d)", pos % CXGRID, pos / CXGRID);
    return -1;
}

/* Find the location of a clone machine from one of its buttons.
 */
static int clonerfrombutton(int pos)
{
    xyconn     *xy;
    int		i;

    for (xy = clonerlist(), i = clonerlistsize() ; i ; ++xy, --i)
	if (xy->from == pos)
	    return xy->to;
    warn("unconnected cloner button at (%d %d)", pos % CXGRID, pos / CXGRID);
    return -1;
}

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
#define	CS_FDIRMASK		0x0F	/* temp storage for forced moves */
#define	CS_SLIDETOKEN		0x10	/* can move off of a slide floor */
#define	CS_REVERSE		0x20	/* needs to turn around */
#define	CS_NOISYMOVEMENT	0x40	/* block was pushed by Chip */

#define	getfdir(cr)	((cr)->state & CS_FDIRMASK)
#define	setfdir(cr, d)	((cr)->state = ((cr)->state & ~CS_FDIRMASK) \
				     | ((d) & CS_FDIRMASK))

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
static void killcreature(creature *cr)
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
	if (cr->id != Tank)
	    continue;
	if (floorat(cr->pos) == CloneMachine || isice(floorat(cr->pos)))
	    continue;
	cr->state ^= CS_REVERSE;
    }
}

/* Start an animation sequence at the spot formerly occupied by the
 * given creature.
 */
static creature *addanimation(creature const *cr, int id)
{
    creature   *anim;

    anim = newcreature();
    if (!anim)
	return NULL;
    anim->id = id;
    anim->dir = cr->dir;
    anim->pos = cr->pos;
    anim->moving = cr->moving;
    anim->frame = 12;
    anim->hidden = FALSE;
    anim->state = 0;
    anim->tdir = NIL;
    if (anim->moving == 8) {
	anim->pos -= delta[anim->dir];
	anim->moving = 0;
    }
    markanimated(cr->pos);
    return anim;
}

/* Abort the animation sequence occuring at the given location.
 */
static int stopanimationat(int pos)
{
    creature   *anim;

    for (anim = creaturelist() ; anim->id ; ++anim) {
	if (anim->pos == pos && isanimation(anim->id)) {
	    killcreature(anim);
	    return TRUE;
	}
    }
    return FALSE;
}

/* Start an animation sequence at the spot formerly occupied by the
 * given creature.
 */
static void removecreature(creature *cr, int animationid)
{
#if 0
    addanimation(cr, animationid);
    killcreature(cr);
#else
    if (cr->id != Chip)
	removeclaim(cr->pos);
    cr->id = animationid;
    cr->frame = 12;
    cr->hidden = FALSE;
    if (cr->state & CS_NOISYMOVEMENT)
	stopsoundeffect(SND_BLOCK_MOVING);
    cr->state = 0;
    cr->tdir = NIL;
    if (cr->moving == 8) {
	cr->pos -= delta[cr->dir];
	cr->moving = 0;
    }
    markanimated(cr->pos);
#endif
}

/* What happens when Chip dies.
 */
static void removechip(int reason, creature *also)
{
    creature  *chip = getchip();

    switch (reason) {
      case CHIP_DROWNED:
	addsoundeffect(SND_WATER_SPLASH);
	removecreature(chip, Water_Splash);
	break;
      case CHIP_BOMBED:
	addsoundeffect(SND_BOMB_EXPLODES);
	removecreature(chip, Bomb_Explosion);
	break;
      case CHIP_OUTOFTIME:
	removecreature(chip, Entity_Explosion);
	break;
      case CHIP_BURNED:
	addsoundeffect(SND_CHIP_LOSES);
	removecreature(chip, Entity_Explosion);
	break;
      case CHIP_COLLIDED:
	addsoundeffect(SND_CHIP_LOSES);
	removecreature(chip, Entity_Explosion);
	if (also)
	    removecreature(also, Entity_Explosion);
	break;
    }

    resetfloorsounds(FALSE);
    startendgametimer();
    timeoffset() = 1;
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
    /* Drowned_Chip */
    { 0, 0, 0 },
    /* Burned_Chip */
    { 0, 0, 0 },
    /* Bombed_Chip */
    { 0, 0, 0 },
    /* Exited_Chip */
    { 0, 0, 0 },
    /* Exit_Extra_1 */
    { 0, 0, 0 },
    /* Exit_Extra_2 */
    { 0, 0, 0 },
    /* Overlay_Buffer */
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
    _assert(block && block->id == Block);
    _assert(floorat(block->pos) != CloneMachine);
    _assert(dir != NIL);

    if (!canmakemove(block, dir, flags)) {
	if (!block->moving && (flags & CMM_PUSHBLOCKS))
	    block->dir = dir;
	return FALSE;
    }
    if (flags & CMM_PUSHBLOCKS) {
	block->tdir = dir;
	if (advancecreature(block, FALSE) > 0) {
	    chippushing() = TRUE;
	    addsoundeffect(SND_BLOCK_MOVING);
	    block->state |= CS_NOISYMOVEMENT;
	} else {
	    warn("Thought block could move, but it couldn't!");
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

    _assert(cr);
    _assert(dir != NIL);

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
    if (getfdir(cr) != NIL)
	return;
    floor = floorat(cr->pos);
    if (floor == CloneMachine || floor == Beartrap) {
	cr->tdir = cr->dir;
	return;
    }

    dir = cr->dir;
    pdir = NIL;

    _assert(dir != NIL);

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
	choices[1] = randomof3(walkerprng(),
			       left(dir), back(dir), right(dir));
	break;
      case Blob:
	choices[0] = 1 << random4(mainprng());
	break;
      case Teeth:
	if ((currenttime() >> 2) & 1)
	    return;
	if (getchip()->hidden)
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

/* Determine the direction of Chip's next move. If discard is TRUE,
 * then Chip is not currently permitted to select a direction of
 * movement, and the player's input should not be retained.
 */
static void choosechipmove(creature *cr, int discard)
{
    int	dir;

    dir = currentinput();
    currentinput() = NIL;

    if (dir == NIL || discard) {
	cr->tdir = NIL;
    } else {
	lastmove() = dir;
	cr->tdir = dir;
    }
    if (cr->tdir == NIL)
	chippushing() = FALSE;
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
    int	floor;

    setfdir(cr, NIL);

    floor = floorat(cr->pos);

    if (currenttime() == 0)
	return FALSE;

    if (isice(floor)) {
	if (cr->id == Chip && possession(Boots_Ice))
	    return FALSE;
	if (cr->dir == NIL)
	    return FALSE;
	setfdir(cr, cr->dir);
	return TRUE;
    } else if (isslide(floor)) {
	if (cr->id == Chip && possession(Boots_Slide))
	    return FALSE;
	setfdir(cr, getslidedir(floor, TRUE));
	return !(cr->state & CS_SLIDETOKEN);
    } else if (floor == Teleport) {
	setfdir(cr, cr->dir);
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
	if (cr->tdir == NIL && getfdir(cr) == NIL)
	    resetfloorsounds(FALSE);
    } else {
	if (getforcedmove(cr))
	    cr->tdir = NIL;
	else
	    choosecreaturemove(cr);
    }

    return cr->tdir != NIL || getfdir(cr) != NIL;
}

static void checkmovingto(void)
{
    creature   *cr;
    int		dir;

    cr = getchip();
    dir = cr->tdir;
    if (dir == NIL) {
	chiptopos() = -1;
	chiptocr() = NULL;
	return;
    }
    if ((dir & (NORTH | SOUTH)) && (dir & (EAST | WEST))) {
	chiptopos() = -1;
	chiptocr() = NULL;
	return;
    }
    chiptopos() = cr->pos + delta[dir];
    chiptocr() = NULL;
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

    _assert(floorat(cr->pos) == Teleport);

    origpos = pos = cr->pos;

    for (;;) {
	--pos;
	if (pos < 0)
	    pos += CXGRID * CYGRID;
	if (floorat(pos) == Teleport) {
	    cr->pos = pos;
	    n = !issomeoneat(pos) &&
			(cr->dir == NIL || canmakemove(cr, cr->dir, 0));
	    cr->pos = origpos;
	    if (n)
		break;
	    if (pos == origpos) {
		if (cr->id == Chip) {
		    addsoundeffect(SND_CANT_MOVE);
		    chippushing() = TRUE;
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

/* Release a creature currently inside a clone machine. If the
 * creature successfully exits, a new clone is created to replace it.
 */
static int activatecloner(int pos)
{
    creature   *cr;
    creature   *clone;

    if (pos < 0)
	return FALSE;
    _assert(floorat(pos) == CloneMachine);
    cr = lookupcreature(pos, TRUE);
    if (!cr)
	return FALSE;
    clone = newcreature();
    if (!clone)
	return advancecreature(cr, TRUE) != 0;

    *clone = *cr;
    if (advancecreature(cr, TRUE) <= 0) {
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

    if (pos < 0)
	return;
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
 * or clone machine. +1 is returned if the creature succeeded in
 * moving, 0 is returned if the move could not be initiated, and -1 is
 * returned if the creature was killed in the attempt.
 */
static int startmovement(creature *cr, int releasing)
{
    int	dir;
    int	floorfrom;
    int	f1, f2;

    _assert(cr->moving <= 0);

    if (cr->tdir != NIL)
	dir = cr->tdir;
    else if (getfdir(cr) != NIL)
	dir = getfdir(cr);
    else
	return 0;

    if ((dir & (NORTH | SOUTH)) && (dir & (EAST | WEST))) {
	_assert(cr->id == Chip);
	if (cr->dir & dir) {
	    f1 = canmakemove(cr, cr->dir, CMM_EXPOSEWALLS | CMM_PUSHBLOCKS);
	    f2 = canmakemove(cr, cr->dir ^ dir,
			     CMM_EXPOSEWALLS | CMM_PUSHBLOCKS);
	    dir = !f1 && f2 ? dir ^ cr->dir : cr->dir;
	} else {
	    if (canmakemove(cr, dir & (EAST | WEST),
			    CMM_EXPOSEWALLS | CMM_PUSHBLOCKS))
		dir &= EAST | WEST;
	    else
		dir &= NORTH | SOUTH;
	}
    }

    cr->dir = dir;

    floorfrom = floorat(cr->pos);

    if (cr->id == Chip) {
	_assert(!cr->hidden);
	chippushing() = FALSE;
	if (!possession(Boots_Slide)) {
	    if (isslide(floorfrom) && cr->tdir == NIL)
		cr->state |= CS_SLIDETOKEN;
	    else if (!isice(floorfrom) || possession(Boots_Ice))
		cr->state &= ~CS_SLIDETOKEN;
	}
    }

    if (!canmakemove(cr, dir, CMM_EXPOSEWALLS | CMM_PUSHBLOCKS
					| (releasing ? CMM_RELEASING : 0))) {
	if (cr->id == Chip) {
	    if (!couldntmove()) {
		couldntmove() = TRUE;
		addsoundeffect(SND_CANT_MOVE);
	    }
	    chippushing() = TRUE;
	}
	if (isice(floorfrom) && (cr->id != Chip || !possession(Boots_Ice))) {
	    cr->dir = back(dir);
	    applyicewallturn(cr);
	    if (cr->id == Chip)
		chippushing() = FALSE;
	}
	return 0;
    }

    if (floorfrom == CloneMachine || floorfrom == Beartrap)
	_assert(releasing);

    if (cr->id != Chip) {
	removeclaim(cr->pos);
	if (cr->id != Block && cr->pos == chiptopos())
	    chiptocr() = cr;
    } else if (chiptocr() && !chiptocr()->hidden) {
	chiptocr()->moving = 8;
	removechip(CHIP_COLLIDED, chiptocr());
	return -1;
    }
    cr->pos += delta[dir];
    if (ismarkedanimated(cr->pos))
	stopanimationat(cr->pos);
    if (cr->id != Chip)
	claimlocation(cr->pos);

    cr->moving += 8;

    if (cr->id != Chip && cr->pos == chippos() && !getchip()->hidden) {
	removechip(CHIP_COLLIDED, cr);
	return -1;
    }
    if (cr->id == Chip) {
	couldntmove() = FALSE;
	cr = lookupcreature(cr->pos, FALSE);
	if (cr) {
	    removechip(CHIP_COLLIDED, cr);
	    return -1;
	}
    }

    return +1;
}

/* Determine the speed of a moving creature. (Speed is measured in
 * eighths of a tile per tick.)
 */
static int continuemovement(creature *cr)
{
    int	floor, speed;

    if (isanimation(cr->id))
	return TRUE;

    _assert(cr->moving > 0);

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
 * produced by moving onto a tile occur at this point. FALSE is
 * returned if the creature is removed by the time the function
 * returns.
 */
static int endmovement(creature *cr)
{
    int	floor;
    int	survived = TRUE;

    _assert(cr->moving <= 0);

    if (isanimation(cr->id))
	return TRUE;

    floor = floorat(cr->pos);

    if (cr->id != Chip || !possession(Boots_Ice))
	applyicewallturn(cr);

    if (cr->id == Chip) {
	switch (floor) {
	  case Water:
	    if (!possession(Boots_Water)) {
		removechip(CHIP_DROWNED, NULL);
		survived = FALSE;
	    }
	    break;
	  case Fire:
	    if (!possession(Boots_Fire)) {
		removechip(CHIP_BURNED, NULL);
		survived = FALSE;
	    }
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
	    _assert(possession(floor));
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
	    addsoundeffect(SND_IC_COLLECTED);
	    break;
	  case Socket:
	    _assert(chipsneeded() == 0);
	    floorat(cr->pos) = Empty;
	    addsoundeffect(SND_SOCKET_OPENED);
	    break;
	  case Exit:
	    cr->hidden = TRUE;
	    completed() = TRUE;
	    addsoundeffect(SND_CHIP_WINS);
	    break;
	}
    } else if (cr->id == Block) {
	switch (floor) {
	  case Water:
	    floorat(cr->pos) = Dirt;
	    addsoundeffect(SND_WATER_SPLASH);
	    removecreature(cr, Water_Splash);
	    survived = FALSE;
	    break;
	  case Key_Blue:
	    floorat(cr->pos) = Empty;
	    break;
	}
    } else {
	switch (floor) {
	  case Water:
	    if (cr->id != Glider) {
		addsoundeffect(SND_WATER_SPLASH);
		removecreature(cr, Water_Splash);
		survived = FALSE;
	    }
	    break;
	  case Key_Blue:
	    floorat(cr->pos) = Empty;
	    break;
	}
    }

    if (!survived)
	return FALSE;

    switch (floor) {
      case Bomb:
	floorat(cr->pos) = Empty;
	if (cr->id == Chip) {
	    removechip(CHIP_BOMBED, NULL);
	} else {
	    addsoundeffect(SND_BOMB_EXPLODES);
	    removecreature(cr, Bomb_Explosion);
	}
	survived = FALSE;
	break;
      case Beartrap:
	addsoundeffect(SND_TRAP_ENTERED);
	break;
      case Button_Blue:
	turntanks();
	addsoundeffect(SND_BUTTON_PUSHED);
	break;
      case Button_Green:
	greentoggle() = !greentoggle();
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

    return survived;
}

/* Advance the movement of the given creature. If the creature is not
 * currently moving but should be, movement is initiated. If the
 * creature completes their movement, any and all appropriate side
 * effects are applied. If releasing is TRUE, the movement is occuring
 * out-of-turn, as with movement across an open beatrap or an
 * activated clone machine. The return value is +1 if the creature
 * successfully moved (or successfully remained stationary), 0 if the
 * creature tried to move and failed, or -1 if the creature was killed
 * and exists no longer.
 */
static int advancecreature(creature *cr, int releasing)
{
    char	tdir = NIL;
    int		f;

    if (cr->moving <= 0) {
	if (releasing) {
	    _assert(cr->dir != NIL);
	    tdir = cr->tdir;
	    cr->tdir = cr->dir;
	} else if (cr->tdir == NIL && getfdir(cr) == NIL)
	    return +1;
	f = startmovement(cr, releasing);
	if (f < 0)
	    return f;
	if (f == 0) {
	    if (releasing)
		cr->tdir = tdir;
	    return 0;
	}
	cr->tdir = NIL;
    }

    if (!continuemovement(cr)) {
	if (!endmovement(cr))
	    return -1;
    }

    return +1;
}

#ifndef NDEBUG

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
			cr->id, "-^<?v?\?\?>"[(int)cr->dir],
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
	    warn("%d: Undefined floor %d at (%d %d)",
		 currenttime(), state->map[pos].top.id,
		 pos % CXGRID, pos / CXGRID);
	if (state->map[pos].top.state & 0x80)
	    warn("%d: Undefined floor state %02X at (%d %d)",
		 currenttime(), state->map[pos].top.id,
		 pos % CXGRID, pos / CXGRID);
    }

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (isanimation(state->map[cr->pos].top.id)) {
	    if (cr->moving > 12)
		warn("%d: Too-large animation frame %02X at (%d %d)",
		     currenttime(), cr->moving,
		     cr->pos % CXGRID, cr->pos / CXGRID);
	    continue;
	}
	if (cr->id < 0x40 || cr->id >= 0x80)
	    warn("%d: Undefined creature %d:%d at (%d %d)",
		 currenttime(), cr - creaturelist(), cr->id,
		 cr->pos % CXGRID, cr->pos / CXGRID);
	if (cr->pos < 0 || cr->pos >= CXGRID * CYGRID)
	    warn("%d: Creature %d:%d has left the map: %04X",
		 currenttime(), cr - creaturelist(), cr->id, cr->pos);
	if (isanimation(cr->id))
	    continue;
	if (cr->dir > EAST && (cr->dir != NIL || cr->id != Block))
	    warn("%d: Creature %d:%d moving in illegal direction (%d)",
		 currenttime(), cr - creaturelist(), cr->id, cr->dir);
	if (cr->dir == NIL && cr->id != Block)
	    warn("%d: Creature %d:%d lacks direction",
		 currenttime(), cr - creaturelist(), cr->id);
	if (cr->state & 0x80)
	    warn("%d: Creature %d:%d in an illegal state %X",
		 currenttime(), cr - creaturelist(), cr->id, cr->state);
	if (cr->moving > 8)
	    warn("%d: Creature %d:%d has a moving time of %d",
		 currenttime(), cr - creaturelist(), cr->id, cr->moving);
	if (cr->moving < 0)
	    warn("%d: Creature %d:%d has a negative moving time: %d",
		 currenttime(), cr - creaturelist(), cr->id, cr->moving);
    }
}

#endif

/*
 * Per-tick maintenance functions.
 */

/* Actions and checks that occur at the start of every tick.
 */
static void initialhousekeeping(void)
{
    creature   *chip;
    creature   *cr;

#ifndef NDEBUG
    verifymap();
#endif

    if (currenttime() == 0) {
	if (state->initrndslidedir == NIL)
	    state->initrndslidedir = lastrndslidedir;
	else
	    lastrndslidedir = state->initrndslidedir;
    }

    chip = getchip();
    if (chip->id == Pushing_Chip)
	chip->id = Chip;

    if (!inendgame()) {
	if (completed()) {
	    startendgametimer();
	    timeoffset() = 1;
	} else if (timelimit() && currenttime() >= timelimit()) {
	    removechip(CHIP_OUTOFTIME, NULL);
	}
    }

    if (greentoggle())
	togglewalls();
    greentoggle() = FALSE;

    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->hidden)
	    continue;
	if (isanimation(cr->id) && cr->frame <= 0) {
	    killcreature(cr);
	} else if (cr->state & CS_REVERSE) {
	    cr->state &= ~CS_REVERSE;
	    if (cr->moving <= 0)
		cr->dir = back(cr->dir);
	}
    }
    for (cr = creaturelist() ; cr->id ; ++cr) {
	if (cr->state & CS_NOISYMOVEMENT) {
	    if (cr->hidden || cr->moving <= 0) {
		stopsoundeffect(SND_BLOCK_MOVING);
		cr->state &= ~CS_NOISYMOVEMENT;
	    }
	}
    }

#ifndef NDEBUG
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
	  case CmdCheatNorth:		--yviewoffset();		break;
	  case CmdCheatWest:		--xviewoffset();		break;
	  case CmdCheatSouth:		++yviewoffset();		break;
	  case CmdCheatEast:		++xviewoffset();		break;
	  case CmdCheatHome:		xviewoffset()=yviewoffset()= 0;	break;
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
#endif

    chiptopos() = -1;
    chiptocr() = NULL;
}

/* Actions and checks that occur at the end of every tick.
 */
static void finalhousekeeping(void)
{
    creature   *cr;

    for (cr = creaturelist() ; cr->id ; ++cr)
	if (!cr->hidden && isanimation(cr->id))
	    --cr->frame;
    for (--cr ; cr >= creaturelist() ; --cr) {
	if (cr->hidden || cr->moving || isanimation(cr->id))
	    continue;
	if (floorat(cr->pos) == Teleport)
	    teleportcreature(cr);
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

    xviewpos() = (chip->pos % CXGRID) * 8 + xviewoffset() * 8;
    yviewpos() = (chip->pos / CXGRID) * 8 + yviewoffset() * 8;
    if (chip->moving) {
	switch (chip->dir) {
	  case NORTH:	yviewpos() += chip->moving;	break;
	  case WEST:	xviewpos() += chip->moving;	break;
	  case SOUTH:	yviewpos() -= chip->moving;	break;
	  case EAST:	xviewpos() -= chip->moving;	break;
	}
    }

    if (!chip->hidden) {
	if (floor == HintButton && chip->moving <= 0)
	    showhint();
	else
	    hidehint();
	if (chippushing())
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
/* 0A block			*/	{ FALSE, Block,		    NORTH },
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
/* 20 not used (overlay buffer)	*/	{ TRUE,  0,		    NIL },
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
/* 33 drowning Chip		*/	{ TRUE,  0,		    NIL },
/* 34 burned Chip in fire	*/	{ TRUE,  0,		    NIL },
/* 35 burned Chip		*/	{ TRUE,  0,		    NIL },
/* 36 not used			*/	{ TRUE,  0,		    NIL },
/* 37 not used			*/	{ TRUE,  0,		    NIL },
/* 38 not used			*/	{ TRUE,  0,		    NIL },
/* 39 Chip in exit		*/	{ TRUE,  Exited_Chip,	    NIL },
/* 3A exit (frame 2)		*/	{ TRUE,  Exit,		    NIL },
/* 3B exit (frame 3)		*/	{ TRUE,  Exit,		    NIL },
/* 3C Chip swimming N		*/	{ TRUE,  0,		    NIL },
/* 3D Chip swimming W		*/	{ TRUE,  0,		    NIL },
/* 3E Chip swimming S		*/	{ TRUE,  0,		    NIL },
/* 3F Chip swimming E		*/	{ TRUE,  0,		    NIL },
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
    xyconn	       *xy;
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
	if (layer1[pos] >= (int)(sizeof fileids / sizeof *fileids)
					|| !fileids[layer1[pos]].id) {
	    warn("Level %d: Invalid tile %02X at (%d %d) replaced with wall",
		 game->number, layer1[pos], pos % CXGRID, pos / CXGRID);
	    layer1[pos] = 0x01;
	}
	if (layer2[pos] >= (int)(sizeof fileids / sizeof *fileids)
					|| !fileids[layer2[pos]].id) {
	    warn("Level %d: Invalid tile %02X at (%d %d) replaced with wall",
		 game->number, layer2[pos], pos % CXGRID, pos / CXGRID);
	    layer2[pos] = 0x01;
	}
	state->map[pos].bot.id = Empty;
	if (layer2[pos]) {
	    if (fileids[layer1[pos]].isfloor) {
		warn("Level %d: Invalid \"buried\" tile at (%d %d)",
		     game->number, pos % CXGRID, pos / CXGRID);
		markinvalid();
	    } else if (!fileids[layer2[pos]].isfloor) {
		warn("Level %d: Invalid \"buried\" creature at (%d %d)",
		     game->number, pos % CXGRID, pos / CXGRID);
		markinvalid();
	    }
	    floorat(pos) = fileids[layer2[pos]].id;
	} else if (fileids[layer1[pos]].isfloor) {
	    floorat(pos) = fileids[layer1[pos]].id;
	} else {
	    floorat(pos) = Empty;
	}
	if (!fileids[layer1[pos]].isfloor) {
	    cr->pos = pos;
	    cr->id = fileids[layer1[pos]].id;
	    cr->dir = fileids[layer1[pos]].dir;
	    cr->moving = 0;
	    cr->hidden = FALSE;
	    if (cr->id == Chip) {
		if (n >= 0) {
		    warn("Level %d: multiple Chips on the map!", game->number);
		    markinvalid();
		}
		n = cr - creaturelist();
		cr->dir = SOUTH;
		cr->state = 0;
	    } else {
		cr->state = 0;
		claimlocation(pos);
		if (cr->id == Block)
		    if (floorat(pos) != CloneMachine &&
					floorat(pos) != Beartrap)
			cr->dir = NIL;
	    }
	    setfdir(cr, NIL);
	    cr->tdir = NIL;
	    cr->frame = 0;
	    ++cr;
	}
    }
    if (n < 0) {
	warn("Level %d: Chip isn't on the map!", game->number);
	markinvalid();
	n = cr - creaturelist();
	cr->pos = 0;
	cr->hidden = TRUE;
	++cr;
    }
    cr->pos = -1;
    cr->id = Nothing;
    cr->dir = NIL;
    if (n) {
	cr = creaturelist();
	crtemp = cr[0];
	cr[0] = cr[n];
	cr[n] = crtemp;
    }

    for (xy = traplist(), n = traplistsize() ; n ; --n, ++xy) {
	if (floorat(xy->from) != Button_Brown) {
	    warn("Level %d: invalid beartrap wiring: no button at (%d %d)",
		 game->number, xy->from % CXGRID, xy->to / CXGRID);
	} else if (floorat(xy->to) != Beartrap) {
	    warn("Level %d: disabling miswired beartrap button at (%d %d)",
		 game->number, xy->to % CXGRID, xy->to / CXGRID);
	    xy->from = -1;
	}
    }
    for (xy = clonerlist(), n = clonerlistsize() ; n ; --n, ++xy) {
	if (floorat(xy->from) != Button_Red) {
	    warn("Level %d: invalid cloner wiring: no button at (%d %d)",
		 game->number, xy->from % CXGRID, xy->to / CXGRID);
	} else if (floorat(xy->to) != CloneMachine) {
	    warn("Level %d: disabling miswired cloner button at (%d %d)",
		 game->number, xy->to % CXGRID, xy->to / CXGRID);
	    xy->from = -1;
	}
    }

    chipsneeded() = game->chips;
    possession(Key_Red) = possession(Key_Blue)
			= possession(Key_Yellow)
			= possession(Key_Green) = 0;
    possession(Boots_Ice) = possession(Boots_Slide)
			  = possession(Boots_Fire)
			  = possession(Boots_Water) = 0;

    resetendgametimer();
    greentoggle() = FALSE;
    couldntmove() = FALSE;
    chippushing() = FALSE;
    completed() = FALSE;
    chiptopos() = -1;
    chiptocr() = NULL;
    xviewoffset() = 0;
    yviewoffset() = 0;
    restartprng(walkerprng(), WALKER_PRNG_SEED);

    preparedisplay();
    return !ismarkedinvalid();
}

/* Advance the game state by one tick.
 */
static int advancegame(gamelogic *logic)
{
    creature   *cr;

    setstate(logic);

    initialhousekeeping();

    for (cr = creaturelist() ; cr->id ; ++cr) ;
    for (--cr ; cr >= creaturelist() ; --cr) {
	setfdir(cr, NIL);
	cr->tdir = NIL;
	if (cr->hidden || isanimation(cr->id))
	    continue;
	if (cr->moving <= 0)
	    choosemove(cr);
    }

    cr = getchip();
    if (getfdir(cr) == NIL && cr->tdir == NIL)
	couldntmove() = FALSE;
    else
	checkmovingto();

    for (cr = creaturelist() ; cr->id ; ++cr) ;
    for (--cr ; cr >= creaturelist() ; --cr) {
	if (cr->hidden)
	    continue;
	if (advancecreature(cr, FALSE) < 0)
	    continue;
	cr->tdir = NIL;
	setfdir(cr, NIL);
	if (floorat(cr->pos) == Button_Brown && cr->moving <= 0)
	    springtrap(trapfrombutton(cr->pos));
    }

    finalhousekeeping();

    preparedisplay();

    if (inendgame()) {
	--timeoffset();
	if (!decrendgametimer()) {
	    resetfloorsounds(TRUE);
	    return completed() ? +1 : -1;
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

    logic.ruleset = Ruleset_Lynx;
    logic.initgame = initgame;
    logic.advancegame = advancegame;
    logic.endgame = endgame;
    logic.shutdown = shutdown;

    return &logic;
}
