#ifndef	_commands_h_
#define	_commands_h_

enum {
    CmdNone = -1,

    CmdNorth,
    CmdWest,
    CmdSouth,
    CmdEast,
    CmdNorthWest,
    CmdNorthEast,
    CmdSouthWest,
    CmdSouthEast,
    CmdWestNorth,
    CmdWestSouth,
    CmdEastNorth,
    CmdEastSouth,

    CmdQuitLevel,
    CmdPrevLevel,
    CmdSameLevel,
    CmdNextLevel,
    CmdQuit,
    CmdPrev,
    CmdSame,
    CmdNext,

    CmdPauseGame,
    CmdHelp,
    CmdPlayback,

    CmdProceed,

    CmdPreserve,

    CmdDebugCmd1,
    CmdDebugCmd2,

    CmdCount
};

#endif
