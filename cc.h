#ifndef	_cc_h_
#define	_cc_h_

#define	NIL	(-1)

#define	NORTH	0
#define	WEST	1
#define	SOUTH	2
#define	EAST	3

#define	left(dir)	(((dir) + 1) & 3)
#define	back(dir)	(((dir) + 2) & 3)
#define	right(dir)	(((dir) + 3) & 3)

#define	CXGRID	32
#define	CYGRID	32

#define	DF_SHOWHINT	0x0001

enum {
    CmdNone = NIL,
    CmdNorth = NORTH,
    CmdWest = WEST,
    CmdSouth = SOUTH,
    CmdEast = EAST,
    CmdPrevLevel,
    CmdSameLevel,
    CmdNextLevel,
    CmdProceed,
    CmdQuitLevel,
    CmdQuit,
    CmdHelp,
    CmdReplay
    ,
    CmdDebugDumpMap,
    CmdDebugCmd
};

typedef	struct xyconn {
    ushrt	from;
    ushrt	to;
} xyconn;

typedef	struct mapcell {
    uchar	floor;
    uchar	state;
    uchar	entity;
} mapcell;

typedef struct creature {
    short	pos;
    uchar	id;
    uchar	dir;
    uchar	waits;
    uchar	state;
} creature;

#endif
