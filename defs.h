/* defs.h: General definitions used throughout the program.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_defs_h_
#define	_defs_h_

#include	<stdio.h>
#include	"gen.h"

/*
 * Miscellaneous definitions.
 */

/* The various rulesets the program can emulate.
 */
enum {
    Ruleset_None = 0,
    Ruleset_Lynx = 1,
    Ruleset_MS = 2,
    Ruleset_Count
};

/* File I/O structure.
 */
typedef	struct fileinfo {
    char       *name;		/* the name of the file */
    FILE       *fp;		/* the real file handle */
    char	alloc;		/* TRUE if name was allocated internally */
} fileinfo;

/* Pseudorandom number generators.
 */
typedef	struct prng {
    unsigned long	initial;	/* initial seed value */
    unsigned long	value;		/* latest random value */
    char		shared;		/* FALSE if independent sequence */
} prng;

/*
 * Definitions used in game play.
 */

/* Turning macros.
 */
#define	left(dir)	((((dir) << 1) | ((dir) >> 3)) & 15)
#define	back(dir)	((((dir) << 2) | ((dir) >> 2)) & 15)
#define	right(dir)	((((dir) << 3) | ((dir) >> 1)) & 15)

/* A move is specified by its direction and when it takes place.
 */
typedef	struct action { int when:27, dir:5; } action;

/* A structure for managing the memory holding the moves of a game.
 */
typedef struct actlist {
    int			allocated;	/* number of elements allocated */
    int			count;		/* size of the actual array */
    action	       *list;		/* the array */
} actlist;

/* Two x,y-coordinates give the locations of a button and the
 * beartrap/cloner it is connected to.
 */
typedef	struct xyconn {
    short		from;		/* location of the button */
    short		to;		/* location of the beartrap/cloner */
} xyconn;

/* Commands from the user, both during and between games.
 */
enum {
    CmdNone = NIL,
    CmdNorth = NORTH,
    CmdWest = WEST,
    CmdSouth = SOUTH,
    CmdEast = EAST,
    CmdPrevLevel,
    CmdNextLevel,
    CmdSameLevel,
    CmdQuitLevel,
    CmdPrev,
    CmdNext,
    CmdSame,
    CmdPrev10,
    CmdNext10,
    CmdPauseGame,
    CmdHelp,
    CmdPlayback,
    CmdSeeScores,
    CmdKillSolution,
    CmdProceed,
    CmdDebugCmd1,
    CmdDebugCmd2,
    CmdQuit,
    CmdPreserve,
    CmdCheatNorth,
    CmdCheatWest,
    CmdCheatSouth,
    CmdCheatEast,
    CmdCheatHome,
    CmdCheatKeyRed,
    CmdCheatKeyBlue,
    CmdCheatKeyYellow,
    CmdCheatKeyGreen,
    CmdCheatBootsIce,
    CmdCheatBootsSlide,
    CmdCheatBootsFire,
    CmdCheatBootsWater,
    CmdCheatICChip,
    CmdCount
};

/* The list of available sound effects.
 */
#define	SND_CHIP_LOSES		0
#define	SND_CHIP_WINS		1
#define	SND_TIME_OUT		2
#define	SND_TIME_LOW		3
#define	SND_CANT_MOVE		4
#define	SND_IC_COLLECTED	5
#define	SND_ITEM_COLLECTED	6
#define	SND_BOOTS_STOLEN	7
#define	SND_TELEPORTING		8
#define	SND_DOOR_OPENED		9
#define	SND_SOCKET_OPENED	10
#define	SND_BUTTON_PUSHED	11
#define	SND_TILE_EMPTIED	12
#define	SND_WALL_CREATED	13
#define	SND_TRAP_ENTERED	14
#define	SND_BOMB_EXPLODES	15
#define	SND_WATER_SPLASH	16

#define	SND_ONESHOT_COUNT	17

#define	SND_BLOCK_MOVING	17
#define	SND_SKATING_FORWARD	18
#define	SND_SKATING_TURN	19
#define	SND_SLIDING		20
#define	SND_SLIDEWALKING	21
#define	SND_ICEWALKING		22
#define	SND_WATERWALKING	23
#define	SND_FIREWALKING		24

#define	SND_COUNT		25

/*
 * Structures for managing the different games.
 */

/* The collection of data maintained for each game.
 */
typedef	struct gamesetup {
    int			number;		/* numerical ID of the level */
    int			time;		/* no. of seconds allotted */
    int			chips;		/* no. of chips for the socket */
    int			besttime;	/* time (in ticks) of best solution */
    unsigned long	savedrndseed;	/* PRNG seed of best solution */
    unsigned char	savedrndslidedir; /* rnd-slide dir of best solution */
    unsigned char	sgflags;	/* saved-game flags (see below) */
    int			map1size;	/* compressed size of layer 1 */
    int			map2size;	/* compressed size of layer 2 */
    unsigned char      *map1;		/* layer 1 (top) of the map */
    unsigned char      *map2;		/* layer 2 (bottom) of the map */
    int			creaturecount;	/* size of active creature list */
    int			trapcount;	/* size of beartrap connection list */
    int			clonercount;	/* size of cloner connection list */
    actlist		savedsolution;	/* the player's best solution so far */
    short		creatures[256];	/* the active creature list */
    xyconn		traps[256];	/* the beatrap connection list */
    xyconn		cloners[256];	/* the clone machine connection list */
    char		name[256];	/* name of the level */
    char		passwd[256];	/* the level's password */
    char		hinttext[256];	/* the level's hint */
} gamesetup;

/* Flags associated with a saved game.
 */
#define	SGF_HASPASSWD	0x01		/* player knows the level's password */
#define	SGF_REPLACEABLE	0x02		/* solution is marked as replaceable */

/* The collection of data maintained for each game file.
 */
typedef	struct gameseries {
    int			total;		/* the number of levels in the file */
    int			allocated;	/* number of elements allocated */
    int			count;		/* actual size of array */
    int			ruleset;	/* the ruleset for the game file */
    gamesetup	       *games;		/* the list of levels */
    fileinfo		mapfile;	/* the file containing the levels */
    fileinfo		solutionfile;	/* the file of the user's solutions */
    int			solutionflags;	/* settings for the saved solutions */
    int			allmapsread;	/* TRUE if levels are at EOF */
    char		name[256];	/* the name of the series */
} gameseries;

#endif
