/* mslogic.c: The game logic for the MS ruleset.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"defs.h"
#include	"err.h"
#include	"state.h"
#include	"random.h"
#include	"mslogic.h"

#undef assert
#define	assert(test)	((test) || (die("internal error: failed sanity check" \
				        " (%s)\nPlease report this error to"  \
				        " breadbox@muppetlabs.com", #test), 0))

/* Internal game status flags.
 */
#define	SF_CHIPWAITMASK		0x0007
#define	SF_CHIPOKAY		0x0000
#define	SF_CHIPBURNED		0x0010
#define	SF_CHIPBOMBED		0x0020
#define	SF_CHIPDROWNED		0x0030
#define	SF_CHIPHIT		0x0040
#define	SF_CHIPNOTOKAY		0x0070
#define	SF_CHIPSTATUSMASK	0x0070
#define	SF_DEFERBUTTONS		0x0080

#define	creatureid(id)		((id) & ~3)
#define	creaturedirid(id)	(idxdir((id) & 3))

static int advancecreature(creature *cr, int dir);

/* A pointer to the game state, used so that it doesn't have to be
 * passed to every single function.
 */
static gamestate       *state;

static int xviewoffset = 0, yviewoffset = 0;

/*
 * Accessor macros for various fields in the game state. Many of the
 * macros can be used as an lvalue.
 */

#define	setstate(p)		(state = (p))

#define	getchip()		(creatures[0])
#define	chippos()		(getchip()->pos)
#define	chipdir()		(getchip()->dir)

#define	chipsneeded()		(state->chipsneeded)

#define	clonerlist()		(state->game->cloners)
#define	clonerlistsize()	(state->game->clonercount)
#define	traplist()		(state->game->traps)
#define	traplistsize()		(state->game->trapcount)

#define	timelimit()		(state->timelimit)
#define	currenttime()		(state->currenttime)
#define	currentinput()		(state->currentinput)
#define	displayflags()		(state->displayflags)
#define	xviewpos()		(state->xviewpos)
#define	yviewpos()		(state->yviewpos)

#define	mainprng()		(&state->mainprng)

#define	iscompleted()		(state->statusflags & SF_COMPLETED)
#define	setcompleted()		(state->statusflags |= SF_COMPLETED)
#define	setnosaving()		(state->statusflags |= SF_NOSAVING)
#define	showhint()		(state->statusflags |= SF_SHOWHINT)
#define	hidehint()		(state->statusflags &= ~SF_SHOWHINT)

#define	setdeferbuttons()	(state->statusflags |= SF_DEFERBUTTONS)
#define	resetdeferbuttons()	(state->statusflags &= ~SF_DEFERBUTTONS)
#define	buttonsdeferred()	(state->statusflags & SF_DEFERBUTTONS)

#define	getchipstatus()		(state->statusflags & SF_CHIPSTATUSMASK)
#define	setchipstatus(s)	\
    (state->statusflags = (state->statusflags & ~SF_CHIPSTATUSMASK) | (s))

#define	resetchipwait()		(state->statusflags &= ~SF_CHIPWAITMASK)
#define	getchipwait()		(state->statusflags & SF_CHIPWAITMASK)
#define	incrchipwait()		\
    (state->statusflags = (state->statusflags & ~SF_CHIPWAITMASK) \
			| ((state->statusflags & SF_CHIPWAITMASK) + 1))

#define	lastmove()		(state->lastmove)

#define	addsoundeffect(sfx)	(state->soundeffects |= 1 << (sfx))

#define	cellat(pos)		(&state->map[pos])

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
 * Memory allocation.
 */

typedef	struct slipper {
    creature   *cr;
    int		dir;
} slipper;

static creature	       *creaturepool = NULL;
static void	       *creaturepoolend = NULL;
static int const	creaturepoolchunk = 256;

static creature	      **creatures = NULL;
static int		creaturecount = 0;
static int		creaturesallocated = 0;

static creature	      **blocks = NULL;
static int		blockcount = 0;
static int		blocksallocated = 0;

static slipper	       *slips = NULL;
static int		slipcount = 0;
static int		slipsallocated = 0;

static void resetcreaturepool(void)
{
    if (!creaturepoolend)
	return;
    while (creaturepoolend) {
	creaturepool = creaturepoolend;
	creaturepoolend = ((creature**)creaturepoolend)[0];
    }
    creaturepoolend = creaturepool;
    creaturepool = (creature*)creaturepoolend - creaturepoolchunk + 1;
}

static creature *allocatecreature(void)
{
    creature   *cr;

    if (creaturepool == creaturepoolend) {
	if (creaturepoolend && ((creature**)creaturepoolend)[1]) {
	    creaturepool = ((creature**)creaturepoolend)[1];
	    creaturepoolend = creaturepool + creaturepoolchunk - 1;
	} else {
	    cr = creaturepoolend;
	    creaturepool = malloc(creaturepoolchunk * sizeof *creaturepool);
	    if (!creaturepool)
		memerrexit();
	    if (cr)
		((creature**)cr)[1] = creaturepool;
	    creaturepoolend = creaturepool + creaturepoolchunk - 1;
	    ((creature**)creaturepoolend)[0] = cr;
	    ((creature**)creaturepoolend)[1] = NULL;
	}
    }

    cr = creaturepool++;
    cr->id = Nothing;
    cr->pos = -1;
    cr->dir = NIL;
    cr->fdir = NIL;
    cr->tdir = NIL;
    cr->state = 0;
    cr->hidden = FALSE;
    cr->moving = 0;
    cr->waits = 0;
    return cr;
}

static void resetcreaturelist(void)
{
    creaturecount = 0;
}

static creature *addtocreaturelist(creature *cr)
{
    if (creaturecount >= creaturesallocated) {
	creaturesallocated = creaturesallocated ? creaturesallocated * 2 : 16;
	creatures = realloc(creatures, creaturesallocated * sizeof *creatures);
	if (!creatures)
	    memerrexit();
    }
    creatures[creaturecount++] = cr;
    return cr;
}

static void resetblocklist(void)
{
    blockcount = 0;
}

static creature *addtoblocklist(creature *cr)
{
    if (blockcount >= blocksallocated) {
	blocksallocated = blocksallocated ? blocksallocated * 2 : 16;
	blocks = realloc(blocks, blocksallocated * sizeof *blocks);
	if (!blocks)
	    memerrexit();
    }
    blocks[blockcount++] = cr;
    return cr;
}

static void resetsliplist(void)
{
    slipcount = 0;
}

static creature *appendtosliplist(creature *cr, int dir)
{
    int	n;

    for (n = 0 ; n < slipcount ; ++n) {
	if (slips[n].cr == cr) {
	    slips[n].dir = dir;
	    return cr;
	}
    }

    if (slipcount >= slipsallocated) {
	slipsallocated = slipsallocated ? slipsallocated * 2 : 16;
	slips = realloc(slips, slipsallocated * sizeof *slips);
	if (!slips)
	    memerrexit();
    }
    slips[slipcount].cr = cr;
    slips[slipcount].dir = dir;
    ++slipcount;
    return cr;
}

static creature *prependtosliplist(creature *cr, int dir)
{
    int	n;

    if (slipcount && slips[0].cr == cr) {
	slips[0].dir = dir;
	return cr;
    }

    if (slipcount >= slipsallocated) {
	slipsallocated = slipsallocated ? slipsallocated * 2 : 16;
	slips = realloc(slips, slipsallocated * sizeof *slips);
	if (!slips)
	    memerrexit();
    }
    for (n = slipcount ; n ; --n)
	slips[n] = slips[n - 1];
    ++slipcount;
    slips[0].cr = cr;
    slips[0].dir = dir;
    return cr;
}

static int getslipdir(creature *cr)
{
    int	n;

    for (n = 0 ; n < slipcount ; ++n)
	if (slips[n].cr == cr)
	    return slips[n].dir;
    return NIL;
}

static void removefromsliplist(creature *cr)
{
    int	n;

    for (n = 0 ; n < slipcount ; ++n)
	if (slips[n].cr == cr)
	    break;
    if (n == slipcount)
	return;
    --slipcount;
    for ( ; n < slipcount ; ++n)
	slips[n] = slips[n + 1];
}

/*
 * Simple floor functions.
 */

#define	FS_BUTTONDOWN		0x01
#define	FS_CLONING		0x02
#define	FS_BROKEN		0x08

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
	if (!advance)
	    return NIL;
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

static int floorat(int pos)
{
    mapcell    *cell;

    cell = cellat(pos);
    if (!iskey(cell->top.id) && !isboots(cell->top.id)
			     && !iscreature(cell->top.id))
	return cell->top.id;
    if (!iskey(cell->bot.id) && !isboots(cell->bot.id)
			     && !iscreature(cell->bot.id))
	return cell->bot.id;
    return Empty;
}

static maptile *getfloorat(int pos)
{
    mapcell    *cell;

    cell = cellat(pos);
    if (!iskey(cell->top.id) && !isboots(cell->top.id)
			     && !iscreature(cell->top.id))
	return &cell->top;
    if (!iskey(cell->bot.id) && !isboots(cell->bot.id)
			     && !iscreature(cell->bot.id))
	return &cell->bot;
    return &cell->bot; /*???*/
}

static int issomeoneat(int pos)
{
    mapcell    *cell;
    int		id;

    cell = cellat(pos);
    id = cell->top.id;
    if (creatureid(id) == Chip || creatureid(id) == Swimming_Chip) {
	id = cell->bot.id;
	if (creatureid(id) == Chip || creatureid(id) == Swimming_Chip)
	    return FALSE;
    }
    return iscreature(id);
}

static int isoccupied(int pos)
{
    int	id;

    id = cellat(pos)->top.id;
    return id == Block_Static || iscreature(id);
}

static void pushtile(int pos, maptile tile)
{
    mapcell    *cell;

    cell = cellat(pos);
    cell->bot = cell->top;
    cell->top = tile;
}

static maptile poptile(int pos)
{
    maptile	tile;
    mapcell    *cell;

    cell = cellat(pos);
    tile = cell->top;
    cell->top = cell->bot;
    cell->bot.id = Empty;
    cell->bot.state = 0;
    return tile;
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
    mapcell    *cell;
    int		pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	cell = cellat(pos);
	if ((cell->top.id == SwitchWall_Open
				|| cell->top.id == SwitchWall_Closed)
			&& !(cell->top.state & FS_BROKEN))
	    cell->top.id ^= SwitchWall_Open ^ SwitchWall_Closed;
	if ((cell->bot.id == SwitchWall_Open
				|| cell->bot.id == SwitchWall_Closed)
			&& !(cell->bot.state & FS_BROKEN))
	    cell->bot.id ^= SwitchWall_Open ^ SwitchWall_Closed;
    }
}

/*
 * Functions that manage the list of entities.
 */

/* Creature state flags.
 */
#define	CS_RELEASED		0x01	/* can leave a beartrap */
#define	CS_CLONING		0x02	/* cannot move this tick */
#define	CS_HASMOVED		0x04	/* already used current move */
#define	CS_TURNING		0x08	/* is turning around */
#define	CS_SLIP			0x10	/* is on the slip list */
#define	CS_SLIDE		0x20	/* is on the slip list but can move */

/* Return the creature located at pos. Ignores Chip unless includechip
 * is TRUE.
 */
static creature *lookupcreature(int pos, int includechip)
{
    int	n;

    if (!creatures)
	return NULL;
    for (n = 0 ; n < creaturecount ; ++n) {
	if (creatures[n]->hidden)
	    continue;
	if (creatures[n]->pos == pos)
	    if (creatures[n]->id != Chip || includechip)
		return creatures[n];
    }
    return NULL;
}

static creature *lookupblock(int pos)
{
    creature   *cr;
    int		id, n;

    if (blocks) {
	for (n = 0 ; n < blockcount ; ++n)
	    if (blocks[n]->pos == pos && !blocks[n]->hidden)
		return blocks[n];
    }

    cr = allocatecreature();
    cr->id = Block;
    cr->pos = pos;
    id = cellat(pos)->top.id;
    if (id == Block_Static)
	cr->dir = NIL;
    else if (creatureid(id) == Block)
	cr->dir = creaturedirid(id);
    else
	die("lookupblock() called for (%d %d) which contains non-block %02X",
	    pos % CXGRID, pos / CXGRID, id);

    if (cellat(pos)->bot.id == Beartrap) {
	for (n = 0 ; n < traplistsize() ; ++n) {
	    if (traplist()[n].to == cr->pos) {
		cr->state |= CS_RELEASED;
		break;
	    }
	}
    }

    return addtoblocklist(cr);
}

static void updatecreature(creature const *cr)
{
    maptile    *tile;
    int		id, dir;

    if (cr->hidden)
	return;
    tile = &cellat(cr->pos)->top;
    id = cr->id;
    if (id == Block) {
	tile->id = Block_Static;
	return;
    } else if (id == Chip) {
	if (getchipstatus()) {
	    switch (getchipstatus()) {
	      case SF_CHIPBURNED:	tile->id = Burned_Chip;		return;
	      case SF_CHIPDROWNED:	tile->id = Water_Splash;	return;
#if 0
	      case SF_CHIPBOMBED:	tile->id = Bombed_Chip;		return;
#endif
	    }
	} else if (cellat(cr->pos)->bot.id == Water) {
	    id = Swimming_Chip;
	}
    }

    dir = cr->dir;
    if (cr->state & CS_TURNING)
	dir = right(dir);

    tile->id = crtile(id, dir);
    tile->state = 0;
}

static void addcreaturetomap(creature const *cr)
{
    static maptile const dummy = { Empty, 0 };

    if (cr->hidden)
	return;
    pushtile(cr->pos, dummy);
    updatecreature(cr);
}

/* Given a creature (presumably in an open beartrap), return the
 * direction of its controller, or NIL if the creature has no living
 * controller.
 */
static int getcontrollerdir(creature const *cr)
{
    int	n;

    assert(cr != getchip());
    for (n = 0 ; n < creaturecount && creatures[n] != cr ; ++n) ;
    assert(n < creaturecount);
    for (--n ; n >= 0 ; --n) {
	if (creatures[n]->hidden)
	    continue;
	if (cellat(creatures[n]->pos)->bot.id == CloneMachine)
	    continue;
	if (creatures[n] == getchip())
	    continue;
	break;
    }
    if (n < 0)
	return NIL;
    return creatures[n]->dir;
}

/* Enervate an inert creature.
 */
static creature *awakencreature(int pos)
{
    creature   *new;
    int		tileid;

    tileid = cellat(pos)->top.id;
    if (!iscreature(tileid) || creatureid(tileid) == Chip)
	return NULL;
    new = allocatecreature();
    new->id = creatureid(tileid);
    new->dir = creaturedirid(tileid);
    new->pos = pos;
    return new->id == Block ? addtoblocklist(new) : addtocreaturelist(new);
}

/* Mark a creature as dead.
 */
static void removecreature(creature *cr)
{
    cr->state &= ~(CS_SLIP | CS_SLIDE);
    if (cr->id == Chip) {
	if (getchipstatus() == SF_CHIPOKAY)
	    setchipstatus(SF_CHIPNOTOKAY);
    } else
	cr->hidden = TRUE;

}

/* Turn around any and all tanks.
 */
static void turntanks(creature const *inmidmove)
{
    int	n;

    for (n = 0 ; n < creaturecount ; ++n) {
	if (creatures[n]->hidden || creatures[n]->id != Tank)
	    continue;
	if (cellat(creatures[n]->pos)->bot.id == CloneMachine)
	    continue;
	creatures[n]->dir = back(creatures[n]->dir);
	if (!(creatures[n]->state & CS_TURNING))
	    creatures[n]->state |= CS_TURNING | CS_HASMOVED;
	if (creatures[n] != inmidmove)
	    updatecreature(creatures[n]);
    }
}

/*
 * Maintaining the slip list.
 */

/* Add the given creature to the slip list if it is not already on it.
 * If nosliding is FALSE, the creature is still permitted to slide.
 */
static void startfloormovement(creature *cr, int floor)
{
    int	dir;

    cr->state &= ~(CS_SLIP | CS_SLIDE);

    if (isice(floor))
	dir = icewallturn(floor, cr->dir);
    else if (isslide(floor))
	dir = getslidedir(floor, TRUE);
    else if (floor == Teleport)
	dir = cr->dir;
    else if (floor == Beartrap && cr->id == Block)
	dir = cr->dir;
    else
	return;

    if (cr->id == Chip) {
	cr->state |= isslide(floor) ? CS_SLIDE : CS_SLIP;
	prependtosliplist(cr, dir);
	cr->dir = dir;
	updatecreature(cr);
    } else {
	cr->state |= CS_SLIP;
	appendtosliplist(cr, dir);
    }
}

/* Remove the given creature from the slip list.
 */
static void endfloormovement(creature *cr)
{
    cr->state &= ~(CS_SLIP | CS_SLIDE);
    removefromsliplist(cr);
}

/* Move a block at the given position forward in the given direction.
 * FALSE is returned if the block cannot be pushed. If collapse is
 * TRUE and the block is atop another block, the bottom block will
 * be destroyed.
 */
static int pushblock(int pos, int dir, int collapse)
{
    creature   *cr;
    int		slipdir;

    assert(cellat(pos)->top.id == Block_Static);
    assert(dir != NIL);

    cr = lookupblock(pos);
    if (!cr) {
	warn("%d: attempt to push disembodied block!", currenttime());
	return FALSE;
    }
    if (cr->state & (CS_SLIP | CS_SLIDE)) {
	slipdir = getslipdir(cr);
	if (dir == slipdir || dir == back(slipdir))
	    return FALSE;
	endfloormovement(cr);
    }
    if (collapse && cellat(pos)->bot.id == Block_Static)
	cellat(pos)->bot.id = Empty;
    return advancecreature(cr, dir);
}

/*
 * The laws of movement across the various floors.
 *
 * Chip, blocks, and other creatures all have slightly different rules
 * about what sort of tiles they are permitted to move into. The
 * following lookup table encapsulates these rules. Note that these
 * rules are only the first check; a creature may be occasionally
 * permitted a particular type of move but still prevented in a
 * specific situation.
 */

#define	NWSE	(NORTH | WEST | SOUTH | EAST)

static struct { unsigned char chip, block, creature; } const movelaws[] = {
    /* Nothing */		{ 0, 0, 0 },
    /* Empty */			{ NWSE, NWSE, NWSE },
    /* Slide_North */		{ NWSE, NWSE, NWSE },
    /* Slide_West */		{ NWSE, NWSE, NWSE },
    /* Slide_South */		{ NWSE, NWSE, NWSE },
    /* Slide_East */		{ NWSE, NWSE, NWSE },
    /* Slide_Random */		{ NWSE, NWSE, 0 },
    /* Ice */			{ NWSE, NWSE, NWSE },
    /* IceWall_Northwest */	{ SOUTH | EAST, SOUTH | EAST, SOUTH | EAST },
    /* IceWall_Northeast */	{ SOUTH | WEST, SOUTH | WEST, SOUTH | WEST },
    /* IceWall_Southwest */	{ NORTH | EAST, NORTH | EAST, NORTH | EAST },
    /* IceWall_Southeast */	{ NORTH | WEST, NORTH | WEST, NORTH | WEST },
    /* Gravel */		{ NWSE, NWSE, 0 },
    /* Dirt */			{ NWSE, 0, 0 },
    /* Water */			{ NWSE, NWSE, NWSE },
    /* Fire */			{ NWSE, NWSE, NWSE },
    /* Bomb */			{ NWSE, NWSE, NWSE },
    /* Beartrap */		{ NWSE, NWSE, NWSE },
    /* Burglar */		{ NWSE, 0, 0 },
    /* HintButton */		{ NWSE, NWSE, NWSE },
    /* Button_Blue */		{ NWSE, NWSE, NWSE },
    /* Button_Green */		{ NWSE, NWSE, NWSE },
    /* Button_Red */		{ NWSE, NWSE, NWSE },
    /* Button_Brown */		{ NWSE, NWSE, NWSE },
    /* Teleport */		{ NWSE, NWSE, NWSE },
    /* Wall */			{ 0, 0, 0 },
    /* Wall_North */		{ NORTH | WEST | EAST,
				  NORTH | WEST | EAST,
				  NORTH | WEST | EAST },
    /* Wall_West */		{ NORTH | WEST | SOUTH,
				  NORTH | WEST | SOUTH,
				  NORTH | WEST | SOUTH },
    /* Wall_South */		{ WEST | SOUTH | EAST,
				  WEST | SOUTH | EAST,
				  WEST | SOUTH | EAST },
    /* Wall_East */		{ NORTH | SOUTH | EAST,
				  NORTH | SOUTH | EAST,
				  NORTH | SOUTH | EAST },
    /* Wall_Southeast */	{ SOUTH | EAST, SOUTH | EAST, SOUTH | EAST },
    /* HiddenWall_Perm */	{ 0, 0, 0 },
    /* HiddenWall_Temp */	{ NWSE, 0, 0 },
    /* BlueWall_Real */		{ NWSE, 0, 0 },
    /* BlueWall_Fake */		{ NWSE, 0, 0 },
    /* SwitchWall_Open */	{ NWSE, NWSE, NWSE },
    /* SwitchWall_Closed */	{ 0, 0, 0 },
    /* PopupWall */		{ NWSE, 0, 0 },
    /* CloneMachine */		{ 0, 0, 0 },
    /* Door_Red */		{ NWSE, 0, 0 },
    /* Door_Blue */		{ NWSE, 0, 0 },
    /* Door_Yellow */		{ NWSE, 0, 0 },
    /* Door_Green */		{ NWSE, 0, 0 },
    /* Socket */		{ NWSE, 0, 0 },
    /* Exit */			{ NWSE, NWSE, 0 },
    /* ICChip */		{ NWSE, 0, 0 },
    /* Key_Red */		{ NWSE, NWSE, NWSE },
    /* Key_Blue */		{ NWSE, NWSE, NWSE },
    /* Key_Yellow */		{ NWSE, NWSE, NWSE },
    /* Key_Green */		{ NWSE, NWSE, NWSE },
    /* Boots_Ice */		{ NWSE, NWSE, 0 },
    /* Boots_Slide */		{ NWSE, NWSE, 0 },
    /* Boots_Fire */		{ NWSE, NWSE, 0 },
    /* Boots_Water */		{ NWSE, NWSE, 0 },
    /* Water_Splash */		{ 0, 0, 0 },
    /* Dirt_Splash */		{ 0, 0, 0 },
    /* Bomb_Explosion */	{ 0, 0, 0 },
    /* Block_Static */		{ NWSE, 0, 0 },
    /* Burned_Chip */		{ 0, 0, 0 },
    /* Bombed_Chip */		{ 0, 0, 0 },
    /* Exited_Chip */		{ 0, 0, 0 },
    /* Exit_Extra_1 */		{ 0, 0, 0 },
    /* Exit_Extra_2 */		{ 0, 0, 0 },
    /* Overlay_Buffer */	{ 0, 0, 0 },
};

#define	CMM_NOLEAVECHECK	0x0001
#define	CMM_NOEXPOSEWALLS	0x0002
#define	CMM_CLONECANTBLOCK	0x0004
#define	CMM_NOCOLLAPSEBLOCKS	0x0008

/* Return TRUE if the given creature is allowed to attempt to move in
 * the given direction. If skipleavecheck is TRUE, the tile the
 * creature is moving out of will be assumed to permit movement in the
 * given direction. If exposewalls is FALSE, hidden walls will not be
 * exposed by calling this function.
 */
static int canmakemove(creature const *cr, int dir, int flags)
{
    int		to;
    int		floor;
    int		id, y, x;

    assert(cr);
    assert(dir != NIL);

    y = cr->pos / CXGRID;
    x = cr->pos % CXGRID;
    y += dir == NORTH ? -1 : dir == SOUTH ? +1 : 0;
    x += dir == WEST ? -1 : dir == EAST ? +1 : 0;
    if (y < 0 || y >= CYGRID || x < 0 || x >= CXGRID)
	return FALSE;
    to = y * CXGRID + x;

    if (!(flags & CMM_NOLEAVECHECK)) {
	switch (cellat(cr->pos)->bot.id) {
	  case Wall_North: 	if (dir == NORTH) return FALSE;		break;
	  case Wall_West: 	if (dir == WEST)  return FALSE;		break;
	  case Wall_South: 	if (dir == SOUTH) return FALSE;		break;
	  case Wall_East: 	if (dir == EAST)  return FALSE;		break;
	  case Wall_Southeast:
	    if (dir == SOUTH || dir == EAST)
		return FALSE;
	    break;
	  case Beartrap:
	    if (!(cr->state & CS_RELEASED))
		return FALSE;
	    break;
	}
    }

    floor = floorat(to);

    if (cr->id == Chip) {
	if (!(movelaws[floor].chip & dir))
	    return FALSE;
	if (floor == Socket && chipsneeded() > 0)
	    return FALSE;
	if (isdoor(floor) && !possession(floor))
	    return FALSE;
	if (iscreature(cellat(to)->top.id)) {
	    id = creatureid(cellat(to)->top.id);
	    if (id == Chip || id == Swimming_Chip || id == Block)
		return FALSE;
	}
	if (floor == HiddenWall_Temp || floor == BlueWall_Real) {
	    if (!(flags & CMM_NOEXPOSEWALLS))
		getfloorat(to)->id = Wall;
	    return FALSE;
	}
	if (floor == Block_Static) {
	    if (pushblock(to, dir, !(flags & CMM_NOCOLLAPSEBLOCKS)))
		return canmakemove(cr, dir, flags);
	    else
		return FALSE;
	}
    } else if (cr->id == Block) {
	if (!(movelaws[floor].block & dir))
	    return FALSE;
	if (issomeoneat(to))
	    return FALSE;
    } else {
	if (!(movelaws[floor].creature & dir))
	    return FALSE;
	if (issomeoneat(to)) {
	    if (!(flags & CMM_CLONECANTBLOCK))
		return FALSE;
	    if (cellat(to)->top.id != crtile(cr->id, cr->dir))
		return FALSE;
	}
	if (isboots(cellat(to)->top.id))
	    return FALSE;
	if (floor == Fire && (cr->id == Bug || cr->id == Walker))
	    return FALSE;
    }

    if (cellat(to)->bot.id == CloneMachine)
	return FALSE;

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
    if (currenttime() & (cr->id == Teeth || cr->id == Blob ? 6 : 2))
	return;

    if (cr->state & CS_TURNING) {
	cr->state &= ~(CS_TURNING | CS_HASMOVED);
	updatecreature(cr);
    }
    if (cr->state & (CS_HASMOVED | CS_SLIP | CS_SLIDE))
	return;

    floor = floorat(cr->pos);

    pdir = dir = cr->dir;

    if (floor == CloneMachine || floor == Beartrap) {
	if (floor == Beartrap && !(cr->state & CS_RELEASED))
	    return;
	if (floor == CloneMachine && (cr->state & CS_CLONING))
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

    if (!(currenttime() & 3))
	cr->state &= ~CS_HASMOVED;
    if (cr->state & CS_HASMOVED)
	return;

    dir = currentinput();
    currentinput() = NIL;
    if (!(dir >= NORTH && dir <= EAST))
	dir = NIL;
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
static int teleportcreature(creature *cr, int start)
{
    maptile    *tile;
    int		defer;
    int		dest, origpos, f;

    assert(!cr->hidden);
    if (cr->dir == NIL) {
	warn("%d: directionless creature %02X on teleport at (%d %d)",
	     currenttime(), cr->id, cr->pos % CXGRID, cr->pos / CXGRID);
	return NIL;
    }

    origpos = cr->pos;
    dest = start;

    defer = buttonsdeferred();
    resetdeferbuttons();
    for (;;) {
	--dest;
	if (dest < 0)
	    dest += CXGRID * CYGRID;
	if (dest == start)
	    break;
	tile = &cellat(dest)->top;
	if (tile->id != Teleport || (tile->state & FS_BROKEN))
	    continue;
	cr->pos = dest;
	f = canmakemove(cr, cr->dir, CMM_NOLEAVECHECK | CMM_NOEXPOSEWALLS
						      | CMM_NOCOLLAPSEBLOCKS);
	cr->pos = origpos;
	if (f)
	    break;
    }
    if (defer)
	setdeferbuttons();

    return dest;
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
    creature	dummy;
    creature   *cr;
    int		pos, tileid;

    pos = clonerfrombutton(buttonpos);
    if (pos < 0)
	return;
    tileid = cellat(pos)->top.id;
    if (creatureid(tileid) == Block) {
	cr = lookupblock(pos);
	if (cr->dir != NIL)
	    advancecreature(cr, cr->dir);
    } else {
	if (!iscreature(tileid)) {
	    warn("Non-creature %02X cloning attempted at (%d %d)",
		 tileid, pos % CXGRID, pos / CXGRID);
	    return;
	}
	if (cellat(pos)->bot.state & FS_CLONING)
	    return;
	memset(&dummy, 0, sizeof dummy);
	dummy.id = creatureid(tileid);
	dummy.dir = creaturedirid(tileid);
	dummy.pos = pos;
	if (!canmakemove(&dummy, dummy.dir, CMM_CLONECANTBLOCK))
	    return;
	cr = awakencreature(pos);
	cr->state |= CS_CLONING;
	if (cellat(pos)->bot.id == CloneMachine)
	    cellat(pos)->bot.state |= FS_CLONING;
    }
}

/* Open a bear trap. Any creature already in the trap is released.
 */
static void springtrap(int buttonpos)
{
    creature   *cr;
    int		pos, id;

    pos = trapfrombutton(buttonpos);
    if (pos < 0)
	return;
    id = cellat(pos)->top.id;
    if (id == Block_Static) {
	cr = lookupblock(pos);
	if (cr)
	    cr->state |= CS_RELEASED;
    } else if (iscreature(id)) {
	cr = lookupcreature(pos, TRUE);
	if (cr)
	    cr->state |= CS_RELEASED;
    }
}

static void resetbuttons(void)
{
    int	pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	cellat(pos)->top.state &= ~FS_BUTTONDOWN;
	cellat(pos)->bot.state &= ~FS_BUTTONDOWN;
    }
}

static void handlebuttons(void)
{
    int	pos, id;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (cellat(pos)->top.state & FS_BUTTONDOWN) {
	    cellat(pos)->top.state &= ~FS_BUTTONDOWN;
	    id = cellat(pos)->top.id;
	} else if (cellat(pos)->bot.state & FS_BUTTONDOWN) {
	    cellat(pos)->bot.state &= ~FS_BUTTONDOWN;
	    id = cellat(pos)->bot.id;
	} else {
	    continue;
	}
	switch (id) {
	  case Button_Blue:
	    addsoundeffect(SND_BUTTON_PUSHED);
	    turntanks(NULL);
	    break;
	  case Button_Green:
	    togglewalls();
	    break;
	  case Button_Red:
	    activatecloner(pos);
	    addsoundeffect(SND_BUTTON_PUSHED);
	    break;
	  case Button_Brown:
	    springtrap(pos);
	    addsoundeffect(SND_BUTTON_PUSHED);
	    break;
	  default:
	    warn("Fooey! Tile %02X is not a button!", id);
	    break;
	}
    }
}

/*
 * When something actually moves.
 */

/* Initiate a move by the given creature in the given direction.
 */
static int startmovement(creature *cr, int dir)
{
    int	floor;

    assert(dir != NIL);

    floor = cellat(cr->pos)->bot.id;
    if (!canmakemove(cr, dir, 0)) {
	if (cr->id == Chip || (floor != Beartrap && floor != CloneMachine
						 && !(cr->state & CS_SLIP))) {
	    cr->dir = dir;
	    updatecreature(cr);
	}
	return FALSE;
    }

    if (floor == Beartrap)
	assert(cr->state & CS_RELEASED);
    cr->state &= ~CS_RELEASED;

    cr->dir = dir;

    return TRUE;
}

/* Complete the movement of the given creature. Most side effects
 * produced by moving onto a tile occur at this point. This function
 * is also the only place where a creature can be added to the slip
 * list.
 */
static void endmovement(creature *cr, int dir)
{
    static int const delta[] = { 0, -CXGRID, -1, 0, +CXGRID, 0, 0, 0, +1 };
    mapcell    *cell;
    maptile    *tile;
    int		dead = FALSE;
    int		oldpos, newpos;
    int		floor, i;

    oldpos = cr->pos;
    newpos = cr->pos + delta[dir];
    /*cr->pos = newpos;*/

    cell = cellat(newpos);
    tile = &cell->top;
    floor = tile->id;

    if (cr->id == Chip) {
	switch (floor) {
	  case Empty:
	    poptile(newpos);
	    break;
	  case Water:
	    if (!possession(Boots_Water))
		setchipstatus(SF_CHIPDROWNED);
	    break;
	  case Fire:
	    if (!possession(Boots_Fire))
		setchipstatus(SF_CHIPBURNED);
	    break;
	  case Dirt:
	    poptile(newpos);
	    break;
	  case BlueWall_Fake:
	    poptile(newpos);
	    break;
	  case PopupWall:
	    tile->id = Wall;
	    break;
	  case Door_Red:
	  case Door_Blue:
	  case Door_Yellow:
	  case Door_Green:
	    assert(possession(floor));
	    if (floor != Door_Green)
		--possession(floor);
	    poptile(newpos);
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
	    poptile(newpos);
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
	    poptile(newpos);
	    addsoundeffect(SND_IC_COLLECTED);
	    break;
	  case Socket:
	    assert(chipsneeded() == 0);
	    poptile(newpos);
	    addsoundeffect(SND_SOCKET_OPENED);
	    break;
	  case Bomb:
	    setchipstatus(SF_CHIPBOMBED);
	    addsoundeffect(SND_BOMB_EXPLODES);
	    break;
	}
    } else if (cr->id == Block) {
	switch (floor) {
	  case Empty:
	    poptile(newpos);
	    break;
	  case Water:
	    tile->id = Dirt;
	    dead = TRUE;
	    addsoundeffect(SND_WATER_SPLASH);
	    break;
	  case Bomb:
	    tile->id = Empty;
	    dead = TRUE;
	    addsoundeffect(SND_BOMB_EXPLODES);
	    break;
	  case Teleport:
	    if (!(tile->state & FS_BROKEN))
		newpos = teleportcreature(cr, newpos);
	    break;
	}
    } else {
	if (iscreature(cell->top.id)) {
	    tile = &cell->bot;
	    floor = cell->bot.id;
	}
	switch (floor) {
	  case Water:
	    if (cr->id != Glider)
		dead = TRUE;
	    break;
	  case Fire:
	    if (cr->id != Fireball)
		dead = TRUE;
	    break;
	  case Bomb:
	    tile->id = Empty;
	    dead = TRUE;
	    addsoundeffect(SND_BOMB_EXPLODES);
	    break;
	  case Teleport:
	    if (!(tile->state & FS_BROKEN))
		newpos = teleportcreature(cr, newpos);
	    break;
	}
    }

    if (cellat(oldpos)->bot.id != CloneMachine)
	poptile(oldpos);
    if (dead) {
	removecreature(cr);
	if (cellat(oldpos)->bot.id == CloneMachine)
	    cellat(oldpos)->bot.state &= ~FS_CLONING;
	return;
    }

    if (cr->id == Chip && floor == Teleport && !(tile->state & FS_BROKEN)) {
	i = newpos;
	newpos = teleportcreature(cr, newpos);
	if (newpos != i)
	    addsoundeffect(SND_TELEPORTING);
    }

    cr->pos = newpos;
    addcreaturetomap(cr);
    cr->pos = oldpos;
    tile = &cell->bot;

    switch (floor) {
      case Button_Blue:
	if (buttonsdeferred())
	    tile->state |= FS_BUTTONDOWN;
	else
	    turntanks(cr);
	addsoundeffect(SND_BUTTON_PUSHED);
	break;
      case Button_Green:
	if (buttonsdeferred())
	    tile->state |= FS_BUTTONDOWN;
	else
	    togglewalls();
	break;
      case Button_Red:
	if (buttonsdeferred())
	    tile->state |= FS_BUTTONDOWN;
	else
	    activatecloner(newpos);
	addsoundeffect(SND_BUTTON_PUSHED);
	break;
      case Button_Brown:
	if (buttonsdeferred())
	    tile->state |= FS_BUTTONDOWN;
	else
	    springtrap(newpos);
	addsoundeffect(SND_BUTTON_PUSHED);
	break;
    }

    cr->pos = newpos;

    if (cellat(oldpos)->bot.id == CloneMachine)
	cellat(oldpos)->bot.state &= ~FS_CLONING;

    if (floor == Beartrap) {
	if (istrapopen(newpos))
	    cr->state |= CS_RELEASED;
    } else if (cellat(newpos)->bot.id == Beartrap) {
	for (i = 0 ; i < traplistsize() ; ++i) {
	    if (traplist()[i].to == newpos) {
		cr->state |= CS_RELEASED;
		break;
	    }
	}
    }

    if (cr->id == Chip) {
	if (iscreature(cell->bot.id) && creatureid(cell->bot.id) != Chip) {
	    setchipstatus(SF_CHIPHIT);
	    return;
	} else if (cell->bot.id == Exit) {
	    setcompleted();
	    return;
	}
    } else {
	if (iscreature(cell->bot.id)) {
	    assert(creatureid(cell->bot.id) == Chip
			|| creatureid(cell->bot.id) == Swimming_Chip);
	    setchipstatus(SF_CHIPHIT);
	    return;
	}
    }

    if (floor == Teleport)
	startfloormovement(cr, floor);
    else if (isice(floor) && (cr->id != Chip || !possession(Boots_Ice)))
	startfloormovement(cr, floor);
    else if (isslide(floor) && (cr->id != Chip || !possession(Boots_Slide)))
	startfloormovement(cr, floor);
    else if (floor != Beartrap || cr->id != Block)
	cr->state &= ~(CS_SLIP | CS_SLIDE);
}

/* Move the given creature in the given direction.
 */
static int advancecreature(creature *cr, int dir)
{
    if (dir == NIL)
	return TRUE;

    if (cr->id == Chip) {
	resetchipwait();
	resetbuttons();
	setdeferbuttons();
    }

    if (!startmovement(cr, dir)) {
	if (cr->id == Chip) {
	    addsoundeffect(SND_CANT_MOVE);
	    resetdeferbuttons();
	    resetbuttons();
	}
	return FALSE;
    }

    endmovement(cr, dir);
    if (cr->id == Chip) {
	resetdeferbuttons();
	handlebuttons();
    }

    return TRUE;
}

/* Return TRUE if gameplay is over.
 */
static int checkforending(void)
{
    if (iscompleted()) {
	addsoundeffect(SND_CHIP_WINS);
	return +1;
    }
    if (getchipstatus() != SF_CHIPOKAY) {
	addsoundeffect(SND_CHIP_LOSES);
	return -1;
    }
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
    int		floor, slipdir;
    int		n;

    for (n = 0 ; n < slipcount ; ++n) {
	cr = slips[n].cr;
	if (!(slips[n].cr->state & (CS_SLIP | CS_SLIDE)))
	    continue;
	slipdir = getslipdir(cr);
	if (slipdir == NIL)
	    continue;
	if (advancecreature(cr, slipdir)) {
	    if (cr->id == Chip)
		cr->state &= ~CS_HASMOVED;
	} else {
	    floor = cellat(cr->pos)->bot.id;
	    if (isice(floor) || (floor == Teleport && cr->id == Chip)) {
		slipdir = icewallturn(floor, back(slipdir));
		if (advancecreature(cr, slipdir)) {
		    if (cr->id == Chip)
			cr->state &= ~CS_HASMOVED;
		}
	    }
	    if (cr->id != Chip || (cr->state & (CS_SLIP | CS_SLIDE))) {
		endfloormovement(cr);
		startfloormovement(cr, cellat(cr->pos)->bot.id);
	    }
	}
	if (checkforending())
	    return;
    }

    for (n = slipcount - 1 ; n >= 0 ; --n)
	if (!(slips[n].cr->state & (CS_SLIP | CS_SLIDE)))
	    endfloormovement(slips[n].cr);
}

static void createclones(void)
{
#if 0
    maptile    *tile;
    int		pos;

    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	tile = &cellat(pos)->top;
	if (tile->state & FS_CLONING) {
	    tile->state &= ~FS_CLONING;
	    awakencreature(pos);
	}
    }
#else
    int	n;

    for (n = 0 ; n < creaturecount ; ++n)
	if (creatures[n]->state & CS_CLONING)
	    creatures[n]->state &= ~CS_CLONING;
#endif
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
	    fprintf(stderr, "%c%02x%02X%c",
		    (cellat(y + x)->top.state ? ':' : '.'),
		    cellat(y + x)->top.id,
		    cellat(y + x)->bot.id,
		    (cellat(y + x)->bot.state ? ':' : '.'));
	fputc('\n', stderr);
    }
    fputc('\n', stderr);
    for (y = 0 ; y < creaturecount ; ++y) {
	cr = creatures[y];
	fprintf(stderr, "%02X%c (%d %d)",
			cr->id, "-^<?v???>"[(int)cr->dir],
			cr->pos % CXGRID, cr->pos / CXGRID);
	for (x = 0 ; x < slipcount ; ++x) {
	    if (cr == slips[x].cr) {
		fprintf(stderr, " [%d]", x + 1);
		break;
	    }
	}
	fprintf(stderr, "%s%s%s%s%s",
			cr->hidden ? " hidden" : "",
			cr->state & CS_RELEASED ? " released" : "",
			cr->state & CS_HASMOVED ? " has-moved" : "",
			cr->state & CS_SLIP ? " slipping" : "",
			cr->state & CS_SLIDE ? " sliding" : "");
	if (x < slipcount)
	    printf(" %c", "-^<?v???>"[(int)slips[x].dir]);
	putchar('\n');
    }
    for (y = 0 ; y < blockcount ; ++y) {
	cr = blocks[y];
	fprintf(stderr, "block %d: (%d %d) %c", y,
		cr->pos % CXGRID, cr->pos / CXGRID, "-^<?v???>"[(int)cr->dir]);
	for (x = 0 ; x < slipcount ; ++x) {
	    if (cr == slips[x].cr) {
		fprintf(stderr, " [%d]", x + 1);
		break;
	    }
	}
	fprintf(stderr, "%s%s%s%s%s",
			cr->hidden ? " hidden" : "",
			cr->state & CS_RELEASED ? " released" : "",
			cr->state & CS_HASMOVED ? " has-moved" : "",
			cr->state & CS_SLIP ? " slipping" : "",
			cr->state & CS_SLIDE ? " sliding" : "");
	if (x < slipcount)
	    printf(" %c", "-^<?v???>"[(int)slips[x].dir]);
	putchar('\n');
    }
}

/* Run various sanity checks on the current game state.
 */
static void verifymap(void)
{
    creature   *cr;
    int		n;

    for (n = 0 ; n < creaturecount ; ++n) {
	cr = creatures[n];
	if (cr->id < 0x40 || cr->id >= 0x80)
	    die("%d: Undefined creature %02X at (%d %d)",
		state->currenttime, cr->id,
		cr->pos % CXGRID, cr->pos / CXGRID);
	if (!cr->hidden && (cr->pos < 0 || cr->pos >= CXGRID * CYGRID))
	    die("%d: Creature %02X has left the map: %04X",
		state->currenttime, cr->id, cr->pos);
	if (cr->dir > EAST && (cr->dir != NIL || cr->id != Block))
	    die("%d: Creature %d lacks direction (%d)",
		state->currenttime, cr->id, cr->dir);
    }
}

/*
 * Per-tick maintenance functions.
 */

/* Actions and checks that occur at the start of a tick.
 */
static void initialhousekeeping(void)
{
    int	n;

    if (currentinput() == CmdDebugCmd2) {
	dumpmap();
	exit(0);
    } else if (currentinput() == CmdDebugCmd1) {
	static int mark = 0;
	warn("Mark %d.", ++mark);
	currentinput() = NIL;
    }
    verifymap();

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

    if (!(currenttime() & 3)) {
	for (n = 1 ; n < creaturecount ; ++n) {
	    if (creatures[n]->state & CS_TURNING) {
		creatures[n]->state &= ~(CS_TURNING | CS_HASMOVED);
		updatecreature(creatures[n]);
	    }
	}
	incrchipwait();
	if (getchipwait() > 3) {
	    resetchipwait();
	    getchip()->dir = SOUTH;
	    updatecreature(getchip());
	}
    }
}

/* Actions and checks that occur at the end of a tick.
 */
static void finalhousekeeping(void)
{
    return;
}

static void preparedisplay(void)
{
    int	pos;

    pos = getchip()->pos;
    if (cellat(pos)->bot.id == HintButton)
	showhint();
    else
	hidehint();

    xviewpos() = (pos % CXGRID) * 8 + xviewoffset * 8;
    yviewpos() = (pos / CYGRID) * 8 + yviewoffset * 8;
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
/* 0A block			*/	{ TRUE,  Block_Static,	    NIL },
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
/* 20 not used			*/	{ TRUE,  Overlay_Buffer,    NIL },
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
/* 34 burned Chip		*/	{ TRUE,  Burned_Chip,	    NIL },
/* 35 burned Chip		*/	{ TRUE,  Bombed_Chip,	    NIL },
/* 36 not used			*/	{ TRUE,  HiddenWall_Perm,   NIL },
/* 37 not used			*/	{ TRUE,  HiddenWall_Perm,   NIL },
/* 38 not used			*/	{ TRUE,  HiddenWall_Perm,   NIL },
/* 39 Chip in exit		*/	{ TRUE,  Exited_Chip,	    NIL },
/* 3A exit - end game		*/	{ TRUE,  Exit_Extra_1,	    NIL },
/* 3B exit - end game		*/	{ TRUE,  Exit_Extra_2,	    NIL },
/* 3C Chip swimming north	*/	{ FALSE, Swimming_Chip,	    NORTH },
/* 3D Chip swimming west	*/	{ FALSE, Swimming_Chip,	    WEST },
/* 3E Chip swimming south	*/	{ FALSE, Swimming_Chip,	    SOUTH },
/* 3F Chip swimming east	*/	{ FALSE, Swimming_Chip,	    EAST },
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
    mapcell	       *cell;
    creature	       *cr;
    creature	       *chip;
    gamesetup	       *game;
    int			transparent;
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
	cell = cellat(pos);
	cell->top.state = 0;
	/*n = FALSE;*/
	if (fileids[layer1[pos]].isfloor) {
	    cell->top.id = fileids[layer1[pos]].id;
	    transparent = iskey(cell->top.id) || isboots(cell->top.id);
	    layer1[pos] = 0;
	} else {
	    cell->top.id = crtile(fileids[layer1[pos]].id,
				  fileids[layer1[pos]].dir);
	    transparent = fileids[layer1[pos]].id != Chip
		       && fileids[layer1[pos]].id != Block;
	}
	cell->bot.state = 0;
	if (fileids[layer2[pos]].isfloor) {
	    cell->bot.id = fileids[layer2[pos]].id;
	    if (!transparent && (cell->bot.id == Teleport
					|| cell->bot.id == SwitchWall_Closed
					|| cell->bot.id == SwitchWall_Open))
		cell->bot.state |= FS_BROKEN;
	} else {
	    cell->bot.id = crtile(fileids[layer2[pos]].id,
				  fileids[layer2[pos]].dir);
	}
    }

    chip = allocatecreature();
    chip->pos = 0;
    chip->id = Chip;
    chip->dir = SOUTH;
    addtocreaturelist(chip);
    for (n = 0 ; n < game->creaturecount ; ++n) {
	pos = game->creatures[n];
	if (!layer1[pos] || fileids[layer1[pos]].isfloor) {
	    warn("Level %d: No creature at location (%d %d)\n",
		 game->number, pos % CXGRID, pos / CXGRID);
	    continue;
	}
	if (fileids[layer1[pos]].id != Block
			&& cellat(pos)->bot.id != CloneMachine) {
	    cr = allocatecreature();
	    cr->pos = pos;
	    cr->id = fileids[layer1[pos]].id;
	    cr->dir = fileids[layer1[pos]].dir;
	    addtocreaturelist(cr);
	    if (!fileids[layer2[pos]].isfloor
			&& fileids[layer2[pos]].id == Chip) {
		chip->pos = pos;
		chip->dir = fileids[layer2[pos]].dir;
		setchipstatus(SF_CHIPHIT);
	    }
	}
	layer1[pos] = 0;
    }
    for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	if (!layer1[pos])
	    continue;
	if (fileids[layer1[pos]].id == Chip) {
	    chip->pos = pos;
	    chip->dir = fileids[layer1[pos]].dir;
	    layer1[pos] = 0;
	}
    }
    state->creatures[0].id = 0;

    chipsneeded() = game->chips;
    possession(Key_Red) = possession(Key_Blue)
			= possession(Key_Yellow)
			= possession(Key_Green) = 0;
    possession(Boots_Ice) = possession(Boots_Slide)
			  = possession(Boots_Fire)
			  = possession(Boots_Water) = 0;

    preparedisplay();
    return TRUE;
}

int ms_endgame(gamestate *pstate)
{
    resetcreaturepool();
    resetcreaturelist();
    resetblocklist();
    resetsliplist();
    xviewoffset = yviewoffset = 0;
    return pstate != NULL;
}

/* Advance the game state by one tick.
 */
int ms_advancegame(gamestate *pstate)
{
    creature   *cr;
    int		r = 0;
    int		n;

    setstate(pstate);

    if (timelimit()) {
	if (currenttime() >= timelimit()) {
	    addsoundeffect(SND_TIME_OUT);
	    return -1;
	} else if (timelimit() - currenttime() < 15 * TICKS_PER_SECOND
				&& currenttime() % TICKS_PER_SECOND == 0)
	    addsoundeffect(SND_TIME_LOW);
    }

    initialhousekeeping();

    if (currenttime() && !(currenttime() & 1)) {
	for (n = 1 ; n < creaturecount ; ++n) {
	    cr = creatures[n];
	    if (cr->hidden || (cr->state & CS_CLONING))
		continue;
	    choosemove(cr);
	    if (cr->tdir != NIL)
		advancecreature(cr, cr->tdir);
	}
	if ((r = checkforending()))
	    goto done;
    }

    if (currenttime() && !(currenttime() & 1)) {
	floormovements();
	if ((r = checkforending()))
	    goto done;
    }

    cr = getchip();
    choosemove(cr);
    if (cr->tdir != NIL) {
	if (advancecreature(cr, cr->tdir))
	    if ((r = checkforending()))
		goto done;
	cr->state |= CS_HASMOVED;
    }

    createclones();

  done:
    finalhousekeeping();
    preparedisplay();
    return r;
}
