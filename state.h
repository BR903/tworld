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

/* All the objects that make up Chip's universe.
 */
enum
{
    Nothing		= 0,

    Empty		= 0x01,

    Slide_North		= 0x02,
    Slide_West		= 0x03,
    Slide_South		= 0x04,
    Slide_East		= 0x05,
    Slide_Random	= 0x06,
    Ice			= 0x07,
    IceWall_Northwest	= 0x08,
    IceWall_Northeast	= 0x09,
    IceWall_Southwest	= 0x0A,
    IceWall_Southeast	= 0x0B,
    Gravel		= 0x0C,
    Dirt		= 0x0D,
    Water		= 0x0E,
    Fire		= 0x0F,
    Bomb		= 0x10,
    Beartrap		= 0x11,
    Burglar		= 0x12,
    HintButton		= 0x13,

    Button_Blue		= 0x14,
    Button_Green	= 0x15,
    Button_Red		= 0x16,
    Button_Brown	= 0x17,
    Teleport		= 0x18,

    Wall		= 0x19,
    Wall_North		= 0x1A,
    Wall_West		= 0x1B,
    Wall_South		= 0x1C,
    Wall_East		= 0x1D,
    Wall_Southeast	= 0x1E,
    HiddenWall_Perm	= 0x1F,
    HiddenWall_Temp	= 0x20,
    BlueWall_Real	= 0x21,
    BlueWall_Fake	= 0x22,
    SwitchWall_Open	= 0x23,
    SwitchWall_Closed	= 0x24,
    PopupWall		= 0x25,

    CloneMachine	= 0x26,

    Door_Red		= 0x27,
    Door_Blue		= 0x28,
    Door_Yellow		= 0x29,
    Door_Green		= 0x2A,
    Socket		= 0x2B,
    Exit		= 0x2C,

    ICChip		= 0x2D,
    Key_Red		= 0x2E,
    Key_Blue		= 0x2F,
    Key_Yellow		= 0x30,
    Key_Green		= 0x31,
    Boots_Ice		= 0x32,
    Boots_Slide		= 0x33,
    Boots_Fire		= 0x34,
    Boots_Water		= 0x35,

    Water_Splash	= 0x36,
    Dirt_Splash		= 0x37,
    Bomb_Explosion	= 0x38,

    Block_Static	= 0x39,

    Burned_Chip		= 0x3A,
    Bombed_Chip		= 0x3B,
    Exited_Chip		= 0x3C,
    Exit_Extra_1	= 0x3D,
    Exit_Extra_2	= 0x3E,

    Overlay_Buffer	= 0x3F,

    Chip		= 0x40,

    Block		= 0x44,

    Tank		= 0x48,
    Ball		= 0x4C,
    Glider		= 0x50,
    Fireball		= 0x54,
    Walker		= 0x58,
    Blob		= 0x5C,
    Teeth		= 0x60,
    Bug			= 0x64,
    Paramecium		= 0x68,

    Swimming_Chip	= 0x6C,
    Pushing_Chip	= 0x70,

    Entity_Reserved3	= 0x74,
    Entity_Reserved2	= 0x78,
    Entity_Reserved1	= 0x7C,
};

/* Macros to assist in identifying types of tiles.
 */
#define	isslide(f)	((f) >= Slide_North && (f) <= Slide_Random)
#define	isice(f)	((f) >= Ice && (f) <= IceWall_Southeast)
#define	isdoor(f)	((f) >= Door_Red && (f) <= Door_Green)
#define	iskey(f)	((f) >= Key_Red && (f) <= Key_Green)
#define	isboots(f)	((f) >= Boots_Ice && (f) <= Boots_Water)
#define	isdecaying(f)	((f) >= Water_Splash && (f) <= Bomb_Explosion)
#define	iscreature(f)	((f) >= Chip)

/*
 * Substructures of the game state
 */

/* A tile on the map.
 */
typedef struct maptile {
    unsigned char	id;	/* identity of the tile */
    unsigned char	state;	/* internal state flags */
} maptile;

/* A location on the map.
 */
typedef	struct mapcell {
    maptile		top;	/* the upper tile */
    maptile		bot;	/* the lower tile */
} mapcell;

/* A creature.
 */
#if 0
typedef	struct creature {
    signed int		pos   : 11;	/* creature's location */
    signed int		dir   : 5;	/* current direction of creature */
    signed int		id    : 8;	/* type of creature */
    signed int		state : 8;	/* internal state value */
    signed int		moving: 4;	/* positional offset of creature */
    signed int		hidden: 2;	/* TRUE if creature is invisible */
    signed int		fdir  : 5;	/* internal state value */
    signed int		tdir  : 5;	/* internal state value */
    signed int		waits : 5;	/* internal state value */
} creature;
#else
typedef struct creature {
    short		pos;		/* creature's location */
    unsigned char	id;		/* type of creature */
    unsigned char	dir;		/* current direction of creature */
    signed char		moving;		/* positional offset of creature */
    unsigned char	hidden;		/* TRUE if creature is invisible */
    unsigned char	state;		/* internal state value */
    unsigned char	fdir;		/* internal state value */
    unsigned char	tdir;		/* internal state value */
    signed char		waits;		/* internal state value */
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
    int			timelimit;		/* maximum time permitted */
    int			currenttime;		/* the current tick count */
    short		currentinput;		/* the current keystroke */
    short		ruleset;		/* the ruleset for the game */
    short		replay;			/* TRUE if in playback mode */
    short		chipsneeded;		/* no. of chips still needed */
    short		keys[4];		/* keys collected */
    short		boots[4];		/* boots collected */
    unsigned long	soundeffects;		/* the latest sound effects */
    actlist		moves;			/* the list of moves */
    short		lastmove;		/* most recent move */
    unsigned char	initrndslidedir;	/* initial random-slide dir */
    unsigned char	rndslidedir;		/* latest random-slide dir */
    prng		mainprng;		/* the main PRNG */
    prng		restartprng;		/* the restarting PRNG */
    short		xviewpos;
    short		yviewpos;
    int			displayflags;		/* game display indicators */
    int			statusflags;		/* internal status flags */
    mapcell		map[CXGRID * CYGRID];	/* the game's map */
    creature		creatures[CXGRID * CYGRID];  /* the creature list */
} gamestate;

/* General status flags.
 */
#define	SF_COMPLETED		0x40000000	/* level has been completed */
#define	SF_NOSAVING		0x20000000	/* solution won't be saved */
#define	SF_INVALID		0x10000000	/* level is not playable */
#define	SF_SHOWHINT		0x08000000	/* display the hint text */
#define	SF_ONOMATOPOEIA		0x04000000	/* use textual sound effects */
#define	SF_CHIPPUSHING		0x02000000	/* Chip is pushing something */

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

/* The list of available sound effects.
 */
#define	SND_CHIP_LOSES		0x00000001
#define	SND_CHIP_WINS		0x00000002
#define	SND_TIME_OUT		0x00000004
#define	SND_TIME_LOW		0x00000008
#define	SND_CANT_MOVE		0x00000010
#define	SND_IC_COLLECTED	0x00000020
#define	SND_ITEM_COLLECTED	0x00000040
#define	SND_BOOTS_STOLEN	0x00000080
#define	SND_TELEPORTING		0x00000100
#define	SND_DOOR_OPENED		0x00000200
#define	SND_SOCKET_OPENED	0x00000400
#define	SND_BUTTON_PUSHED	0x00000800
#define	SND_TILE_EMPTIED	0x00001000
#define	SND_WALL_CREATED	0x00002000
#define	SND_TRAP_SPRUNG		0x00004000
#define	SND_BLOCK_MOVING	0x00008000
#define	SND_BOMB_EXPLODES	0x00010000
#define	SND_WATER_SPLASH	0x00020000
#define	SND_SLIDEWALKING	0x00040000
#define	SND_ICEWALKING		0x00080000
#define	SND_WATERWALKING	0x00100000
#define	SND_FIREWALKING		0x00200000
#define	SND_SKATING_FORWARD	0x00400000
#define	SND_SKATING_TURN	0x00800000
#define	SND_SLIDING		0x01000000

#endif
