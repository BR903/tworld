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

/* The frequency of the internal timer during game play.
 */
#define	TICKS_PER_SECOND	20

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

/* The size of each level.
 */
#define	CXGRID	32
#define	CYGRID	32

/* The four directions, plus NIL (the absence of direction).
 */
#define	NIL	0
#define	NORTH	1
#define	WEST	2
#define	SOUTH	4
#define	EAST	8

/* Turning macros.
 */
#define	left(dir)	((((dir) << 1) | ((dir) >> 3)) & 15)
#define	back(dir)	((((dir) << 2) | ((dir) >> 2)) & 15)
#define	right(dir)	((((dir) << 3) | ((dir) >> 1)) & 15)

/* Translating directions to and from a two-bit representation.
 */
#define	diridx(dir)	((0x30210 >> ((dir) * 2)) & 3)
#define	idxdir(idx)	(1 << ((idx) & 3))

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
    CmdProceed,
    CmdDebugCmd1,
    CmdDebugCmd2,
    CmdQuit,
    CmdPreserve,
    CmdCount
};

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
    int			allsolutionsread;  /* TRUE if solutions are at EOF */
    int			solutionsreadonly; /* TRUE if solutions are readonly */
    char		name[256];	/* the name of the series */
} gameseries;

#endif
