/* state.h: The structure embodying the state of a game in progress.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_state_h_
#define	_state_h_

#include	"defs.h"
#include	"random.h"

/*
 * The objects and entities of the game.
 */

/* Floor tiles (i.e., static objects)
 */
enum
{
    Empty = 0,

    Slide_North,
    Slide_West,
    Slide_South,
    Slide_East,
    Slide_Random,
    Ice,
    IceWall_Northwest,
    IceWall_Northeast,
    IceWall_Southwest,
    IceWall_Southeast,
    Gravel,
    Dirt,
    Water,
    Fire,
    Bomb,
    Beartrap,
    Burglar,
    HintButton,

    Button_Blue,
    Button_Green,
    Button_Red,
    Button_Brown,
    Teleport,

    Wall,
    Wall_North,
    Wall_West,
    Wall_South,
    Wall_East,
    Wall_Southeast,
    HiddenWall_Perm,
    HiddenWall_Temp,
    BlueWall_Real,
    BlueWall_Fake,
    SwitchWall_Open,
    SwitchWall_Closed,
    PopupWall,

    CloneMachine,

    Door_Red,
    Door_Blue,
    Door_Yellow,
    Door_Green,
    Socket,
    Exit,

    ICChip,
    Key_Red,
    Key_Blue,
    Key_Yellow,
    Key_Green,
    Boots_Ice,
    Boots_Slide,
    Boots_Fire,
    Boots_Water,

    Water_Splash,
    Dirt_Splash,
    Bomb_Explosion,

    Block_Emergent,

    Count_Floors
};

#if Count_Floors >= 64
#error "Too many unique floor objects."
#endif

/* Creatures (i.e., moving objects)
 */
enum
{
    Nobody = 0,

    Chip,

    Block,

    Tank,
    Ball,
    Glider,
    Fireball,
    Walker,
    Blob,
    Teeth,
    Bug,
    Paramecium,

    Count_Entities
};

#if Count_Entities > 16
#error "Over 16 unique entities."
#endif

/* Macros to assist in identify types of tiles.
 */
#define	isslide(f)	((f) >= Slide_North && (f) <= Slide_Random)
#define	isice(f)	((f) >= Ice && (f) <= IceWall_Southeast)
#define	isdoor(f)	((f) >= Door_Red && (f) <= Door_Green)
#define	iskey(f)	((f) >= Key_Red && (f) <= Key_Green)
#define	isboots(f)	((f) >= Boots_Ice && (f) <= Boots_Water)
#define	isdecaying(f)	((f) >= Water_Splash && (f) <= Bomb_Explosion)

/*
 * Substructures of the game state
 */

/* A location on the map.
 */
typedef	struct mapcell {
    unsigned char	floor;		/* the main tile */
    unsigned char	hfloor;		/* the other, hidden tile */
    unsigned short	state;		/* internal state value */
} mapcell;

/* A creature.
 */
#if 0
typedef	struct creature {
    short		pos;		/* creature's location */
    unsigned short	state;		/* internal state value */
    signed int		id    : 6;	/* type of creature */
    signed int		dir   : 5;	/* current direction of creature */
    signed int		waits : 5;	/* internal state value */
    signed int		moving: 4;	/* positional offset of creature */
    signed int		fdir  : 5;	/* internal state value */
    signed int		tdir  : 5;	/* internal state value */
    signed int		hidden: 2;	/* TRUE if creature is invisible */
} creature;
#else
typedef struct creature {
    short		pos;		/* creature's location */
    unsigned char	id;		/* type of creature */
    unsigned char	dir;		/* current direction of creature */
    unsigned char	hidden;		/* TRUE if creature is invisible */
    signed char		moving;		/* positional offset of creature */
    unsigned char	state;		/* internal state value */
    unsigned char	fdir;		/* internal state value */
    unsigned char	tdir;		/* internal state value */
    unsigned char	waits;		/* internal state value */
} creature;
#endif

/*
 * The game state structure proper.
 */

/* Ideally, everything the logic and gameplay modules need to know
 * about a game in progress is here.
 */
typedef struct gamestate {
    gamesetup	       *game;			/* the level specification */
    int			currenttime;		/* the current tick count */
    short		currentinput;		/* the current keystroke */
    short		ruleset;		/* the ruleset for the game */
    short		replay;			/* TRUE if in playback mode */
    short		chipsneeded;		/* no. of chips still needed */
    short		keys[4];		/* keys collected */
    short		boots[4];		/* boots collected */
    char const	       *soundeffect;		/* the latest sound effect */
    actlist		moves;			/* the list of moves */
    short		lastmove;		/* most recent move */
    unsigned char	initrndslidedir;	/* initial random-slide dir */
    unsigned char	rndslidedir;		/* latest random-slide dir */
    prng		mainprng;		/* the main PRNG */
    prng		restartprng;		/* the restarting PRNG */
    int			displayflags;		/* game display indicators */
    int			statusflags;		/* internal status flags */
    mapcell		map[CXGRID * CYGRID];	/* the game's map */
    creature		creatures[CXGRID * CYGRID];  /* the creature list */
} gamestate;

/* Status flags.
 */
#define	SF_COMPLETED		0x4000		/* level has been completed */

/* Display flags.
 */
#define	DF_SHOWHINT		0x4000		/* display the hint text */
#define	DF_CHIPPUSHING		0x2000		/* Chip is pushing something */

/* gamestate accessor macros.
 */
#define	redkeys(st)		((st)->keys[0])
#define	bluekeys(st)		((st)->keys[1])
#define	yellowkeys(st)		((st)->keys[2])
#define	greenkeys(st)		((st)->keys[3])
#define	iceboots(st)		((st)->boots[0])
#define	slideboots(st)		((st)->boots[1])
#define	fireboots(st)		((st)->boots[2])
#define	waterboots(st)		((st)->boots[3])

#endif
