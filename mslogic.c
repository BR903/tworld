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

/* Internal game status flags.
 */
#define	SF_CHIPOKAY		0x0000
#define	SF_CHIPBURNED		0x0001
#define	SF_CHIPBOMBED		0x0002
#define	SF_CHIPDROWNED		0x0003
#define	SF_CHIPHIT		0x0004
#define	SF_CHIPNOTOKAY		0x0007
#define	SF_CHIPSTATUSMASK	0x0007
#define	SF_GREENTOGGLED		0x0010

#define	creatureid(id)		((id) & ~3)
#define	creaturedirid(id)	(idxdir((id) & 3))
#define	creaturetile(id, dir)	((id) | diridx(dir))

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

#define	currenttime()		(state->currenttime)
#define	currentinput()		(state->currentinput)
#define	displayflags()		(state->displayflags)
#define	xviewpos()		(state->xviewpos)
#define	yviewpos()		(state->yviewpos)

#define	mainprng()		(&state->mainprng)

#define	iscompleted()		(state->statusflags & SF_COMPLETED)
#define	setcompleted()		(state->statusflags |= SF_COMPLETED)
#define	setnosaving()		(state->statusflags |= SF_NOSAVING)
#define	isgreentoggleset()	(state->statusflags & SF_GREENTOGGLED)
#define	togglegreen()		(state->statusflags ^= SF_GREENTOGGLED)
#define	resetgreentoggle()	(state->statusflags &= ~SF_GREENTOGGLED)
#define	getchipstatus()		(state->statusflags & SF_CHIPSTATUSMASK)
#define	setchipstatus(s)	\
    (state->statusflags = (state->statusflags & ~SF_CHIPSTATUSMASK) | (s))

#define	lastmove()		(state->lastmove)

#define	addsoundeffect(str)	(state->soundeffect = (str))

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

static creature	       *creaturepool = NULL;
static void	       *creaturepoolend = NULL;
static int const	creaturepoolchunk = 256;

static creature	      **creatures = NULL;
static int		creaturecount = 0;
static int		creaturesallocated = 0;

static creature	      **blocks = NULL;
static int		blockcount = 0;
static int		blocksallocated = 0;

static creature	      **slips = NULL;
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

static creature *appendtosliplist(creature *cr)
{
    int	n;

    for (n = 0 ; n < slipcount ; ++n)
	if (slips[n] == cr)
	    return cr;

    if (slipcount >= slipsallocated) {
	slipsallocated = slipsallocated ? slipsallocated * 2 : 16;
	slips = realloc(slips, slipsallocated * sizeof *slips);
	if (!slips)
	    memerrexit();
    }

    slips[slipcount++] = cr;
    return cr;
}

static creature *prependtosliplist(creature *cr)
{
    int	n;

    if (slipcount && slips[0] == cr)
	return cr;

    if (slipcount >= slipsallocated) {
	slipsallocated = slipsallocated ? slipsallocated * 2 : 16;
	slips = realloc(slips, slipsallocated * sizeof *slips);
	if (!slips)
	    memerrexit();
    }

    for (n = slipcount ; n ; --n)
	slips[n] = slips[n - 1];
    ++slipcount;
    slips[0] = cr;
    return cr;
}

static void removefromsliplist(creature *cr)
{
    int	n;

    for (n = 0 ; n < slipcount ; ++n)
	if (slips[n] == cr)
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
#define	FS_CLONERFULL		0x02
#define	FS_BROKEN		0x04

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
#define	CS_RELEASED		0x01	/* can leave a trap/cloner/teleport */
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
	      case SF_CHIPBOMBED:	tile->id = Bombed_Chip;		return;
	      case SF_CHIPDROWNED:	tile->id = Water_Splash;	return;
	    }
	} else if (cellat(cr->pos)->bot.id == Water) {
	    id = Swimming_Chip;
	}
    }

    dir = cr->dir;
    if (cr->state & CS_TURNING)
	dir = right(dir);

    tile->id = creaturetile(id, dir);
}

/* Given a creature (presumably in an open beartrap), return the
 * direction of its controller, or NIL if the creature has no living
 * controller.
 */
static int getcontrollerdir(creature const *cr)
{
#if 0
    creature   *prev = NULL;
    int		n;

    assert(cr != getchip());
    for (n = 0 ; n < creaturecount ; ++n) {
	if (creatures[n] == cr)
	    break;
	prev = creatures[n];
    }
    if (!prev || prev == getchip() || prev->hidden)
	return NIL;
    if (cellat(prev->pos)->bot.id == CloneMachine)
	return NIL;
    return prev->dir;
#else
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
#endif
}

/* Create a new creature as a clone of the given creature.
 */
static creature *addclone(creature const *old)
{
    creature   *new;

    new = allocatecreature();
    *new = *old;
    new->state &= ~CS_RELEASED;
    return new->id == Block ? addtoblocklist(new) : addtocreaturelist(new);
}

/* Mark a creature as dead.
 */
static void removecreature(creature *cr)
{
    cr->state &= ~(CS_SLIP | CS_SLIDE);
    if (cr->id == Chip)
	setchipstatus(SF_CHIPNOTOKAY);
    else
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
static void startfloormovement(creature *cr, int nosliding)
{
    cr->state &= ~(CS_SLIP | CS_SLIDE);
    cr->state |= nosliding ? CS_SLIP : CS_SLIDE;
    if (cr->id == Chip)
	prependtosliplist(cr);
    else
	appendtosliplist(cr);
}

/* Remove the given creature from the slip list.
 */
static void endfloormovement(creature *cr)
{
    cr->state &= ~(CS_SLIP | CS_SLIDE);
    removefromsliplist(cr);
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
    { ALL_OUT | SOUTH_IN | EAST_IN,
      NORTH_OUT | WEST_OUT | SOUTH_IN | EAST_IN,
      NORTH_OUT | WEST_OUT | SOUTH_IN | EAST_IN },
    /* IceWall_Northeast */
    { ALL_OUT | SOUTH_IN | WEST_IN,
      NORTH_OUT | EAST_OUT | SOUTH_IN | WEST_IN,
      NORTH_OUT | EAST_OUT | SOUTH_IN | WEST_IN },
    /* IceWall_Southwest */
    { ALL_OUT | NORTH_IN | EAST_IN,
      SOUTH_OUT | WEST_OUT | NORTH_IN | EAST_IN,
      SOUTH_OUT | WEST_OUT | NORTH_IN | EAST_IN },
    /* IceWall_Southeast */
    { ALL_OUT | NORTH_IN | WEST_IN,
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
    /* Block_Static */
    { ALL_IN_OUT, ALL_OUT, ALL_OUT },
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
    /* Invalid_Tile */
    { 0, 0, 0 },
};

static int pushblock(int pos, int dir);

/* Return TRUE if the given creature is allowed to attempt to move in
 * the given direction. If exposewalls is FALSE, hidden walls will not
 * be exposed by calling this function.
 */
static int canmakemove(creature const *cr, int dir, int exposewalls)
{
    int		to;
    int		floorfrom, floorto;
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

    floorfrom = floorat(cr->pos);
    if (cr->id == Block && floorfrom == Block_Static)
	floorfrom = cellat(cr->pos)->bot.id;
    floorto = floorat(to);

    if (floorfrom == Beartrap || floorfrom == CloneMachine
			      || floorfrom == Teleport)
	if (!(cr->state & CS_RELEASED))
	    return FALSE;

    if (cellat(to)->bot.id == CloneMachine)
	return FALSE;

    if (cr->id == Chip) {
	if (!(movelaws[floorfrom].chip & DIR_OUT(dir)))
	    return FALSE;
	if (isice(floorfrom) && (cr->state & CS_SLIP)
			     && icewallturn(floorfrom, dir) != dir)
	    return FALSE;
	if (!(movelaws[floorto].chip & DIR_IN(dir)))
	    return FALSE;
	if (floorto == Socket && chipsneeded() > 0)
	    return FALSE;
	if (isdoor(floorto) && !possession(floorto))
	    return FALSE;
	if (iscreature(cellat(to)->top.id)) {
	    id = creatureid(cellat(to)->top.id);
	    if (id == Chip || id == Swimming_Chip || id == Block)
		return FALSE;
	}
	if (floorto == HiddenWall_Temp || floorto == BlueWall_Real) {
	    if (exposewalls)
		getfloorat(to)->id = Wall;
	    return FALSE;
	}
	if (floorto == Block_Static) {
	    if (pushblock(to, dir))
		return canmakemove(cr, dir, exposewalls);
	    else
		return FALSE;
	}
    } else if (cr->id == Block) {
	if (floorfrom == Block_Static)
	    floorfrom = cellat(cr->pos)->bot.id;
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
	if (isboots(cellat(to)->top.id))
	    return FALSE;
	if (floorto == Fire && (cr->id == Bug || cr->id == Walker))
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
	if (canmakemove(cr, choices[n], TRUE))
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
static void teleportcreature(creature *cr)
{
    int pos, origpos, n;

    assert(cellat(cr->pos)->bot.id == Teleport);
    assert(!cr->hidden);

    if (cr->dir == NIL) {
	warn("%d: directionless creature %02X on teleport at (%d %d)",
	     currenttime(), cr->id, cr->pos % CXGRID, cr->pos / CXGRID);
	return;
    }
    if (cr->state & CS_RELEASED) {
	warn("%d: tried to teleport teleported creature %02X at (%d %d)",
	     currenttime(), cr->id, cr->pos % CXGRID, cr->pos / CXGRID);
	return;
    }

    cr->state |= CS_RELEASED;

    origpos = pos = cr->pos;

    if (!(cellat(pos)->bot.state & FS_BROKEN)) {
	for (;;) {
	    --pos;
	    if (pos < 0)
		pos += CXGRID * CYGRID;
	    if (pos == origpos)
		break;
	    if (cellat(pos)->top.id != Teleport)
		continue;
	    if (cellat(pos)->top.state & FS_BROKEN)
		continue;
	    cr->pos = pos;
	    n = canmakemove(cr, cr->dir, FALSE);
	    cr->pos = origpos;
	    if (n)
		break;
	}
    }

    if (pos == origpos) {
	if (cr->id != Chip)
	    return;
	if (!canmakemove(cr, cr->dir, FALSE))
	    cr->dir = back(cr->dir);
	return;
    }

    pushtile(pos, poptile(cr->pos));
    cr->pos = pos;
    if (cr->id == Chip)
	addsoundeffect("Whing!");
}

/* This function determines if the given creature is currently being
 * forced to move. (Ice, slide floors, and teleports all cause forced
 * movements; bear traps can also force blocks to move, if they
 * entered the trap via one of the other three causes.) If so, the
 * direction is stored in the creature's fdir field, and TRUE is
 * returned unless the creature can still make a voluntary move.
 */
static int getslipmove(creature *cr)
{
    int floor, dir;

    cr->fdir = NIL;

    floor = cellat(cr->pos)->bot.id;

    if (isice(floor)) {
	if (cr->id == Chip && possession(Boots_Ice))
	    return FALSE;
	dir = cr->dir;
	if (dir == NIL)
	    return FALSE;
	dir = icewallturn(floor, dir);
	if (!canmakemove(cr, dir, TRUE)) {
	    dir = back(cr->dir);
	    if (!canmakemove(cr, dir, TRUE))
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
	if (!(cr->state & CS_RELEASED))
	    return FALSE;
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
    int		pos;

    pos = clonerfrombutton(buttonpos);
    if (pos < 0)
	return;
    assert(cellat(pos)->bot.id == CloneMachine);
    if (cellat(pos)->bot.state & FS_CLONERFULL)
	return;
    if (!iscreature(cellat(pos)->top.id)) {
	warn("Empty clone machine at (%d %d) activated.",
	     pos % CXGRID, pos / CXGRID);
	return;
    }
    cellat(pos)->bot.state |= FS_BUTTONDOWN;
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
    if (!canmakemove(cr, dir, TRUE)) {
	if (cr->id == Chip || (floor != Beartrap && floor != CloneMachine)) {
	    cr->dir = dir;
	    updatecreature(cr);
	}
	return FALSE;
    }

    if (floor == Beartrap || floor == CloneMachine || floor == Teleport) {
	assert(cr->state & CS_RELEASED);
	cr->state &= ~CS_RELEASED;
    }

    if (floor == CloneMachine)
	cellat(cr->pos)->bot.state &= ~FS_CLONERFULL;
    else
	poptile(cr->pos);

    cr->dir = dir;

    return TRUE;
}

/* Complete the movement of the given creature. Most side effects
 * produced by moving onto a tile occur at this point. This function
 * is also the only place where a creature can be added to the slip
 * list.
 */
static void endmovement(creature *cr)
{
    mapcell    *cell;
    maptile    *tile;
    int		floor;

    cell = cellat(cr->pos);
    tile = &cell->top;
    floor = tile->id;

    if (cr->id == Chip) {
	switch (floor) {
	  case Empty:
	    poptile(cr->pos);
	    break;
	  case Water:
	    if (!possession(Boots_Water)) {
		setchipstatus(SF_CHIPDROWNED);
		addsoundeffect("Glugg!");
	    }
	    break;
	  case Fire:
	    if (!possession(Boots_Fire)) {
		setchipstatus(SF_CHIPBURNED);
		addsoundeffect("Yowch!");
	    }
	    break;
	  case Dirt:
	    poptile(cr->pos);
	    break;
	  case BlueWall_Fake:
	    poptile(cr->pos);
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
	    poptile(cr->pos);
	    addsoundeffect("Spang!");
	    break;
	  case Boots_Ice:
	  case Boots_Slide:
	  case Boots_Fire:
	  case Boots_Water:
	  case Key_Red:
	  case Key_Blue:
	  case Key_Yellow:
	  case Key_Green:
	    assert("Too much shit in one place?!");
	    ++possession(floor);
	    poptile(cr->pos);
	    addsoundeffect("Slurp!");
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
	    poptile(cr->pos);
	    addsoundeffect("Chack!");
	    break;
	  case Socket:
	    assert(chipsneeded() == 0);
	    poptile(cr->pos);
	    addsoundeffect("Chack!");
	    break;
	  case Bomb:
	    setchipstatus(SF_CHIPBOMBED);
	    addsoundeffect("Booom!");
	    break;
	  case Beartrap:
	    if (istrapopen(cr->pos))
		cr->state |= CS_RELEASED;
	    break;
	}
    } else if (cr->id == Block) {
	switch (floor) {
	  case Empty:
	    poptile(cr->pos);
	    break;
	  case Water:
	    tile->id = Dirt;
	    removecreature(cr);
	    addsoundeffect("Plash!");
	    break;
	  case Bomb:
	    tile->id = Empty;
	    removecreature(cr);
	    addsoundeffect("Booom!");
	    break;
	  case Beartrap:
	    if (istrapopen(cr->pos))
		cr->state |= CS_RELEASED;
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
		removecreature(cr);
	    break;
	  case Fire:
	    if (cr->id != Fireball)
		removecreature(cr);
	    break;
	  case Bomb:
	    tile->id = Empty;
	    removecreature(cr);
	    addsoundeffect("Booom!");
	    break;
	  case Beartrap:
	    if (istrapopen(cr->pos))
		cr->state |= CS_RELEASED;
	    break;
	}
    }

    if (cr->hidden)
	return;

    pushtile(cr->pos, *tile);
    updatecreature(cr);

    if (cr->id == Chip) {
	if (iscreature(cell->bot.id) && creatureid(cell->bot.id) != Chip) {
	    setchipstatus(SF_CHIPHIT);
	    return;
	} else if (cell->bot.id == Exit) {
	    setcompleted();
	    addsoundeffect("Tadaa!");
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

    switch (cell->bot.id) {
      case Beartrap:
	if (istrapopen(cr->pos))
	    cr->state |= CS_RELEASED;
	break;
      case Button_Blue:
	turntanks(cr);
	addsoundeffect("Klick!");
	break;
      case Button_Green:
	togglewalls();
	addsoundeffect("Clack!");
	break;
      case Button_Red:
	activatecloner(cr->pos);
	addsoundeffect("Thock!");
	break;
      case Button_Brown:
	springtrap(cr->pos);
	addsoundeffect("Plock!");
	break;
    }

    if (floor == Teleport)
	startfloormovement(cr, TRUE);
    else if (isice(floor) && (cr->id != Chip || !possession(Boots_Ice)))
	startfloormovement(cr, TRUE);
    else if (isslide(floor) && (cr->id != Chip || !possession(Boots_Slide)))
	startfloormovement(cr, cr->id != Chip);
    else if (floor != Beartrap || cr->id != Block)
	cr->state &= ~(CS_SLIP | CS_SLIDE);
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

/* Move a block at the given position forward in the given direction.
 * FALSE is returned if the block cannot be pushed.
 */
static int pushblock(int pos, int dir)
{
    creature   *cr;

    assert(cellat(pos)->top.id == Block_Static);
    assert(dir != NIL);

    cr = lookupblock(pos);
    if (!cr) {
	warn("%d: attempt to push disembodied block!", currenttime());
	return FALSE;
    }
    endfloormovement(cr);
    return advancecreature(cr, dir);
}

/* Return TRUE if gameplay is over.
 */
static int checkforending(void)
{
    if (iscompleted())
	return +1;
    if (getchipstatus() != SF_CHIPOKAY)
	return -1;
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

    for (n = 0 ; n < slipcount ; ++n) {
	cr = slips[n];
	getslipmove(cr);
	if (cr->fdir != NIL) {
	    if (advancecreature(cr, cr->fdir))
		if (cr->id == Chip)
		    cr->state &= ~CS_HASMOVED;
#if 0
	} else {
	    if (cr->id != Chip) {
		int f = cr->state & CS_SLIP;
		endfloormovement(cr);
		startfloormovement(cr, f);
	    }
#endif
	}
	if (checkforending())
	    return;
    }

    for (n = slipcount - 1 ; n >= 0 ; --n)
	if (!(slips[n]->state & (CS_SLIP | CS_SLIDE)))
	    endfloormovement(slips[n]);
}

/* Teleport any creatures newly arrived on teleport tiles.
 */
static void teleportations(void)
{
    int	n;

    for (n = 0 ; n < creaturecount ; ++n) {
	if (creatures[n]->hidden)
	    continue;
	if (cellat(creatures[n]->pos)->bot.id != Teleport)
	    continue;
	if (creatures[n]->state & CS_RELEASED)
	    continue;
	teleportcreature(creatures[n]);
    }
    for (n = 0 ; n < blockcount ; ++n) {
	if (blocks[n]->hidden)
	    continue;
	if (cellat(blocks[n]->pos)->bot.id != Teleport)
	    continue;
	if (creatures[n]->state & CS_RELEASED)
	    continue;
	teleportcreature(blocks[n]);
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
	    if (cr == slips[x]) {
		fprintf(stderr, " [%d]", x + 1);
		break;
	    }
	}
	fprintf(stderr, "%s%s%s%s%s\n",
			cr->hidden ? " hidden" : "",
			cr->state & CS_RELEASED ? " released" : "",
			cr->state & CS_HASMOVED ? " has-moved" : "",
			cr->state & CS_SLIP ? " slipping" : "",
			cr->state & CS_SLIDE ? " sliding" : "");
    }
    for (y = 0 ; y < blockcount ; ++y) {
	cr = blocks[y];
	fprintf(stderr, "block %d: (%d %d) %c", y,
		cr->pos % CXGRID, cr->pos / CXGRID, "-^<?v???>"[(int)cr->dir]);
	for (x = 0 ; x < slipcount ; ++x) {
	    if (cr == slips[x]) {
		fprintf(stderr, " [%d]", x + 1);
		break;
	    }
	}
	fprintf(stderr, "%s%s%s%s%s\n",
			cr->hidden ? " hidden" : "",
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
    }
}

/* Actions and checks that occur at the end of a tick.
 */
static void finalhousekeeping(void)
{
    maptile    *tile;
    creature   *cr;
    creature   *clone;
    int		pos;

    if (!(currenttime() & 1)) {
	for (pos = 0 ; pos < CXGRID * CYGRID ; ++pos) {
	    tile = &cellat(pos)->bot;
	    if (tile->id == CloneMachine && (tile->state & FS_BUTTONDOWN)) {
		tile->state &= ~FS_BUTTONDOWN;
		if (creatureid(cellat(pos)->top.id) == Block) {
		    cr = lookupblock(pos);
		    if (!cr) {
			warn("Disembodied block on clone machine at (%d %d)",
			     pos % CXGRID, pos / CXGRID);
			continue;
		    }
		    cr->state |= CS_RELEASED;
		    if (canmakemove(cr, cr->dir, TRUE)) {
			clone = addclone(cr);
			clone->state |= CS_RELEASED;
			advancecreature(clone, clone->dir);
		    }
		    cr->state &= ~CS_RELEASED;
		} else {
		    cr = lookupcreature(pos, TRUE);
		    if (!cr)
			continue;
		    cr->state |= CS_RELEASED;
		    if (canmakemove(cr, cr->dir, TRUE)) {
			clone = addclone(cr);
			clone->state |= CS_RELEASED;
			tile->state |= FS_CLONERFULL;
		    }
		    cr->state &= ~CS_RELEASED;
		}
	    }
	}
    }
}

static void preparedisplay(void)
{
    int	pos;

    pos = getchip()->pos;
    if (cellat(pos)->bot.id == HintButton)
	displayflags() |= DF_SHOWHINT;
    else
	displayflags() &= ~DF_SHOWHINT;

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
/* 20 not used			*/	{ TRUE,  Invalid_Tile,	    NIL },
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
	n = FALSE;
	if (fileids[layer1[pos]].isfloor) {
	    cell->top.id = fileids[layer1[pos]].id;
	    n = iskey(cell->top.id) || isboots(cell->top.id);
	    layer1[pos] = 0;
	} else {
	    cell->top.id = creaturetile(fileids[layer1[pos]].id,
					fileids[layer1[pos]].dir);
	    n = TRUE;
	}
	cell->bot.state = 0;
	if (fileids[layer2[pos]].isfloor) {
	    cell->bot.id = fileids[layer2[pos]].id;
	    if (!n && (cell->bot.id == Teleport
			       || cell->bot.id == SwitchWall_Closed
			       || cell->bot.id == SwitchWall_Open))
		cell->bot.state |= FS_BROKEN;
	} else {
	    cell->bot.id = creaturetile(fileids[layer2[pos]].id,
					fileids[layer2[pos]].dir);
	}
    }

    chip = allocatecreature();
    chip->pos = -1;
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
	if (fileids[layer1[pos]].id != Block) {
	    cr = allocatecreature();
	    cr->pos = pos;
	    cr->id = fileids[layer1[pos]].id;
	    cr->dir = fileids[layer1[pos]].dir;
	    addtocreaturelist(cr);
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
	} else if (cellat(pos)->bot.id == CloneMachine) {
	    cr = allocatecreature();
	    cr->pos = pos;
	    cr->id = fileids[layer1[pos]].id;
	    cr->dir = fileids[layer1[pos]].dir;
	    if (cr->id == Block)
		addtoblocklist(cr);
	    else
		addtocreaturelist(cr);
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

    addsoundeffect(NULL);

    if (chip->pos < 0)
	chip->pos = 0;

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

    initialhousekeeping();

    if (currenttime() && !(currenttime() & 1)) {
	for (n = 1 ; n < creaturecount ; ++n) {
	    cr = creatures[n];
	    if (cr->hidden)
		continue;
	    choosemove(cr);
	    if (cr->tdir != NIL)
		advancecreature(cr, cr->tdir);
	    if ((r = checkforending()))
		goto done;
	}
    }

    if (currenttime() && !(currenttime() & 1)) {
	floormovements();
	if ((r = checkforending()))
	    goto done;
    }

    cr = getchip();
    choosemove(cr);
    if (cr->tdir != NIL) {
	if (advancecreature(cr, cr->tdir)) {
	    if ((r = checkforending()))
		goto done;
	} else
	    addsoundeffect("Mnphf!");
	cr->state |= CS_HASMOVED;
    }

    if (currenttime() & 1) {
	teleportations();
	if ((r = checkforending()))
	    goto done;
    }

  done:
    finalhousekeeping();
    preparedisplay();
    return r;
}
