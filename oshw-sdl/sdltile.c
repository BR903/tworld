/* sdltiles.c: Functions for rendering tile images.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>
#include	"SDL.h"
#include	"sdlgen.h"
#include	"../err.h"
#include	"../state.h"

/* The total number of tile images.
 */
#define	NTILES		128

#define	SIZE_EXTLEFT	0x01
#define	SIZE_EXTRIGHT	0x02
#define	SIZE_EXTUP	0x04
#define	SIZE_EXTDOWN	0x08
#define	SIZE_EXTALL	0x0F

typedef	struct tilemap {
    Uint32     *opaque;
    Uint32     *transp[16];
    char	celcount;
    char	transpsize;
} tilemap;

typedef	struct shorttileidmap {
    short	opaque;
    short	transp;
    char	celcount;
    char	transpsize;
} shorttileidmap;

typedef	struct tileidmap {
    signed char	xopaque;
    signed char	yopaque;
    signed char	xtransp;
    signed char	ytransp;
    signed char	xceloff;
    signed char	yceloff;
    signed char	celcount;
    char	transpsize;
} tileidmap;

static tileidmap const small_tileidmap[NTILES] = {
/* Nothing		*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Empty		*/ {  0,  0, -1, -1, 0, 0, 0, 0 },
/* Slide_North		*/ {  1,  2, -1, -1, 0, 0, 0, 0 },
/* Slide_West		*/ {  1,  4, -1, -1, 0, 0, 0, 0 },
/* Slide_South		*/ {  0, 13, -1, -1, 0, 0, 0, 0 },
/* Slide_East		*/ {  1,  3, -1, -1, 0, 0, 0, 0 },
/* Slide_Random		*/ {  3,  2, -1, -1, 0, 0, 0, 0 },
/* Ice			*/ {  0, 12, -1, -1, 0, 0, 0, 0 },
/* IceWall_Northwest	*/ {  1, 12, -1, -1, 0, 0, 0, 0 },
/* IceWall_Northeast	*/ {  1, 13, -1, -1, 0, 0, 0, 0 },
/* IceWall_Southwest	*/ {  1, 11, -1, -1, 0, 0, 0, 0 },
/* IceWall_Southeast	*/ {  1, 10, -1, -1, 0, 0, 0, 0 },
/* Gravel		*/ {  2, 13, -1, -1, 0, 0, 0, 0 },
/* Dirt			*/ {  0, 11, -1, -1, 0, 0, 0, 0 },
/* Water		*/ {  0,  3, -1, -1, 0, 0, 0, 0 },
/* Fire			*/ {  0,  4, -1, -1, 0, 0, 0, 0 },
/* Bomb			*/ {  2, 10, -1, -1, 0, 0, 0, 0 },
/* Beartrap		*/ {  2, 11, -1, -1, 0, 0, 0, 0 },
/* Burglar		*/ {  2,  1, -1, -1, 0, 0, 0, 0 },
/* HintButton		*/ {  2, 15, -1, -1, 0, 0, 0, 0 },
/* Button_Blue		*/ {  2,  8, -1, -1, 0, 0, 0, 0 },
/* Button_Green		*/ {  2,  3, -1, -1, 0, 0, 0, 0 },
/* Button_Red		*/ {  2,  4, -1, -1, 0, 0, 0, 0 },
/* Button_Brown		*/ {  2,  7, -1, -1, 0, 0, 0, 0 },
/* Teleport		*/ {  2,  9, -1, -1, 0, 0, 0, 0 },
/* Wall			*/ {  0,  1, -1, -1, 0, 0, 0, 0 },
/* Wall_North		*/ {  0,  6, -1, -1, 0, 0, 0, 0 },
/* Wall_West		*/ {  0,  7, -1, -1, 0, 0, 0, 0 },
/* Wall_South		*/ {  0,  8, -1, -1, 0, 0, 0, 0 },
/* Wall_East		*/ {  0,  9, -1, -1, 0, 0, 0, 0 },
/* Wall_Southeast	*/ {  3,  0, -1, -1, 0, 0, 0, 0 },
/* HiddenWall_Perm	*/ {  0,  5, -1, -1, 0, 0, 0, 0 },
/* HiddenWall_Temp	*/ {  2, 12, -1, -1, 0, 0, 0, 0 },
/* BlueWall_Real	*/ {  1, 15, -1, -1, 0, 0, 0, 0 },
/* BlueWall_Fake	*/ {  1, 14, -1, -1, 0, 0, 0, 0 },
/* SwitchWall_Open	*/ {  2,  6, -1, -1, 0, 0, 0, 0 },
/* SwitchWall_Closed	*/ {  2,  5, -1, -1, 0, 0, 0, 0 },
/* PopupWall		*/ {  2, 14, -1, -1, 0, 0, 0, 0 },
/* CloneMachine		*/ {  3,  1, -1, -1, 0, 0, 0, 0 },
/* Door_Red		*/ {  1,  7, -1, -1, 0, 0, 0, 0 },
/* Door_Blue		*/ {  1,  6, -1, -1, 0, 0, 0, 0 },
/* Door_Yellow		*/ {  1,  9, -1, -1, 0, 0, 0, 0 },
/* Door_Green		*/ {  1,  8, -1, -1, 0, 0, 0, 0 },
/* Socket		*/ {  2,  2, -1, -1, 0, 0, 0, 0 },
/* Exit			*/ {  1,  5, -1, -1, 0, 0, 0, 0 },
/* ICChip		*/ {  0,  2, -1, -1, 0, 0, 0, 0 },
/* Key_Red		*/ {  6,  5,  9,  5, 0, 0, 1, 0 },
/* Key_Blue		*/ {  6,  4,  9,  4, 0, 0, 1, 0 },
/* Key_Yellow		*/ {  6,  7,  9,  7, 0, 0, 1, 0 },
/* Key_Green		*/ {  6,  6,  9,  6, 0, 0, 1, 0 },
/* Boots_Ice		*/ {  6, 10,  9, 10, 0, 0, 1, 0 },
/* Boots_Slide		*/ {  6, 11,  9, 11, 0, 0, 1, 0 },
/* Boots_Fire		*/ {  6,  9,  9,  9, 0, 0, 1, 0 },
/* Boots_Water		*/ {  6,  8,  9,  8, 0, 0, 1, 0 },
/* Block_Static		*/ {  0, 10, -1, -1, 0, 0, 0, 0 },
/* Burned_Chip		*/ {  3,  4, -1, -1, 0, 0, 0, 0 },
/* Bombed_Chip		*/ {  3,  5, -1, -1, 0, 0, 0, 0 },
/* Exited_Chip		*/ {  3,  9, -1, -1, 0, 0, 0, 0 },
/* Exit_Extra_1		*/ {  3, 10, -1, -1, 0, 0, 0, 0 },
/* Exit_Extra_2		*/ {  3, 11, -1, -1, 0, 0, 0, 0 },
/* Overlay_Buffer	*/ {  2,  0, -1, -1, 0, 0, 0, 0 },
/* Floor_Reserved_3	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Floor_Reserved_2	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Floor_Reserved_1	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Chip			*/ {  6, 12,  9, 12, 0, 0, 1, 0 },
			   {  6, 13,  9, 13, 0, 0, 1, 0 },
			   {  6, 14,  9, 14, 0, 0, 1, 0 },
			   {  6, 15,  9, 15, 0, 0, 1, 0 },
/* Block		*/ {  0, 14, -1, -1, 0, 0, 0, 0 },
			   {  0, 15, -1, -1, 0, 0, 0, 0 },
			   {  1,  0, -1, -1, 0, 0, 0, 0 },
			   {  1,  1, -1, -1, 0, 0, 0, 0 },
/* Tank			*/ {  4, 12,  7, 12, 0, 0, 1, 0 },
			   {  4, 13,  7, 13, 0, 0, 1, 0 },
			   {  4, 14,  7, 14, 0, 0, 1, 0 },
			   {  4, 15,  7, 15, 0, 0, 1, 0 },
/* Ball			*/ {  4,  8,  7,  8, 0, 0, 1, 0 },
			   {  4,  9,  7,  8, 0, 0, 1, 0 },
			   {  4, 10,  7, 10, 0, 0, 1, 0 },
			   {  4, 11,  7, 11, 0, 0, 1, 0 },
/* Glider		*/ {  5,  0,  8,  0, 0, 0, 1, 0 },
			   {  5,  1,  8,  1, 0, 0, 1, 0 },
			   {  5,  2,  8,  2, 0, 0, 1, 0 },
			   {  5,  3,  8,  3, 0, 0, 1, 0 },
/* Fireball		*/ {  4,  4,  7,  4, 0, 0, 1, 0 },
			   {  4,  5,  7,  5, 0, 0, 1, 0 },
			   {  4,  6,  7,  6, 0, 0, 1, 0 },
			   {  4,  7,  7,  7, 0, 0, 1, 0 },
/* Walker		*/ {  5,  8,  8,  8, 0, 0, 1, 0 },
			   {  5,  9,  8,  9, 0, 0, 1, 0 },
			   {  5, 10,  8, 10, 0, 0, 1, 0 },
			   {  5, 11,  8, 11, 0, 0, 1, 0 },
/* Blob			*/ {  5, 12,  8, 12, 0, 0, 1, 0 },
			   {  5, 13,  8, 13, 0, 0, 1, 0 },
			   {  5, 14,  8, 14, 0, 0, 1, 0 },
			   {  5, 15,  8, 15, 0, 0, 1, 0 },
/* Teeth		*/ {  5,  4,  8,  4, 0, 0, 1, 0 },
			   {  5,  5,  8,  5, 0, 0, 1, 0 },
			   {  5,  6,  8,  6, 0, 0, 1, 0 },
			   {  5,  7,  8,  7, 0, 0, 1, 0 },
/* Bug			*/ {  4,  0,  7,  0, 0, 0, 1, 0 },
			   {  4,  1,  7,  1, 0, 0, 1, 0 },
			   {  4,  2,  7,  2, 0, 0, 1, 0 },
			   {  4,  3,  7,  3, 0, 0, 1, 0 },
/* Paramecium		*/ {  6,  0,  9,  0, 0, 0, 1, 0 },
			   {  6,  1,  9,  1, 0, 0, 1, 0 },
			   {  6,  2,  9,  2, 0, 0, 1, 0 },
			   {  6,  3,  9,  3, 0, 0, 1, 0 },
/* Swimming_Chip	*/ {  3, 12, -1, -1, 0, 0, 0, 0 },
			   {  3, 13, -1, -1, 0, 0, 0, 0 },
			   {  3, 14, -1, -1, 0, 0, 0, 0 },
			   {  3, 15, -1, -1, 0, 0, 0, 0 },
/* Pushing_Chip		*/ {  6, 12,  9, 12, 0, 0, 1, 0 },
			   {  6, 13,  9, 13, 0, 0, 1, 0 },
			   {  6, 14,  9, 14, 0, 0, 1, 0 },
			   {  6, 15,  9, 15, 0, 0, 1, 0 },
/* Entity_Reserved3	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Entity_Reserved2	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Water_Splash		*/ {  3,  3, -1, -1, 0, 0, 0, 0 },
/* Dirt_Splash		*/ {  3,  7, -1, -1, 0, 0, 0, 0 },
/* Bomb_Explosion	*/ {  3,  6, -1, -1, 0, 0, 0, 0 },
/* Animation_Reserved	*/ { -1, -1, -1, -1, 0, 0, 0, 0 }
};

static tileidmap const large_tileidmap[NTILES] = {
/* Nothing		*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Empty		*/ {  0,  0, -1, -1, 0, 0, 0, 0 },
/* Slide_North		*/ {  1,  0, -1, -1, 0, 0, 0, 0 },
/* Slide_West		*/ {  1,  1, -1, -1, 0, 0, 0, 0 },
/* Slide_South		*/ {  1,  2, -1, -1, 0, 0, 0, 0 },
/* Slide_East		*/ {  1,  3, -1, -1, 0, 0, 0, 0 },
/* Slide_Random		*/ {  1,  4, -1, -1, 0, 0, 0, 0 },
/* Ice			*/ {  1,  7, -1, -1, 0, 0, 0, 0 },
/* IceWall_Northwest	*/ {  1, 10, -1, -1, 0, 0, 0, 0 },
/* IceWall_Northeast	*/ {  1, 11, -1, -1, 0, 0, 0, 0 },
/* IceWall_Southwest	*/ {  1,  9, -1, -1, 0, 0, 0, 0 },
/* IceWall_Southeast	*/ {  1,  8, -1, -1, 0, 0, 0, 0 },
/* Gravel		*/ {  2,  0, -1, -1, 0, 0, 0, 0 },
/* Dirt			*/ {  2,  1, -1, -1, 0, 0, 0, 0 },
/* Water		*/ {  1,  5, -1, -1, 0, 0, 0, 0 },
/* Fire			*/ {  1,  6, -1, -1, 0, 0, 0, 0 },
/* Bomb			*/ {  2,  2, -1, -1, 0, 0, 0, 0 },
/* Beartrap		*/ {  4,  6, -1, -1, 0, 0, 0, 0 },
/* Burglar		*/ {  2,  3, -1, -1, 0, 0, 0, 0 },
/* HintButton		*/ {  4,  7, -1, -1, 0, 0, 0, 0 },
/* Button_Blue		*/ {  4,  8, -1, -1, 0, 0, 0, 0 },
/* Button_Green		*/ {  4,  9, -1, -1, 0, 0, 0, 0 },
/* Button_Red		*/ {  4, 10, -1, -1, 0, 0, 0, 0 },
/* Button_Brown		*/ {  4, 11, -1, -1, 0, 0, 0, 0 },
/* Teleport		*/ {  4,  5, -1, -1, 0, 0, 0, 0 },
/* Wall			*/ {  0,  1, -1, -1, 0, 0, 0, 0 },
/* Wall_North		*/ {  2,  8, -1, -1, 0, 0, 0, 0 },
/* Wall_West		*/ {  2,  9, -1, -1, 0, 0, 0, 0 },
/* Wall_South		*/ {  2, 10, -1, -1, 0, 0, 0, 0 },
/* Wall_East		*/ {  2, 11, -1, -1, 0, 0, 0, 0 },
/* Wall_Southeast	*/ {  2,  7, -1, -1, 0, 0, 0, 0 },
/* HiddenWall_Perm	*/ {  0,  0, -1, -1, 0, 0, 0, 0 },
/* HiddenWall_Temp	*/ {  0,  0, -1, -1, 0, 0, 0, 0 },
/* BlueWall_Real	*/ {  3,  7, -1, -1, 0, 0, 0, 0 },
/* BlueWall_Fake	*/ {  3,  7, -1, -1, 0, 0, 0, 0 },
/* SwitchWall_Open	*/ {  2,  6, -1, -1, 0, 0, 0, 0 },
/* SwitchWall_Closed	*/ {  3,  6, -1, -1, 0, 0, 0, 0 },
/* PopupWall		*/ {  2,  4, -1, -1, 0, 0, 0, 0 },
/* CloneMachine		*/ {  2,  5, -1, -1, 0, 0, 0, 0 },
/* Door_Red		*/ {  3,  1, -1, -1, 0, 0, 0, 0 },
/* Door_Blue		*/ {  3,  0, -1, -1, 0, 0, 0, 0 },
/* Door_Yellow		*/ {  3,  3, -1, -1, 0, 0, 0, 0 },
/* Door_Green		*/ {  3,  2, -1, -1, 0, 0, 0, 0 },
/* Socket		*/ {  3,  4, -1, -1, 0, 0, 0, 0 },
/* Exit			*/ {  3,  5, -1, -1, 0, 0, 0, 0 },
/* ICChip		*/ {  4,  4, -1, -1, 0, 0, 0, 0 },
/* Key_Red		*/ {  4,  1,  4,  0, 0, 0, 1, 0 },
/* Key_Blue		*/ {  4,  0,  4,  1, 0, 0, 1, 0 },
/* Key_Yellow		*/ {  4,  3,  4,  2, 0, 0, 1, 0 },
/* Key_Green		*/ {  4,  2,  4,  3, 0, 0, 1, 0 },
/* Boots_Ice		*/ {  3, 10,  3,  8, 0, 0, 1, 0 },
/* Boots_Slide		*/ {  3, 11,  3,  9, 0, 0, 1, 0 },
/* Boots_Fire		*/ {  3,  9,  3, 10, 0, 0, 1, 0 },
/* Boots_Water		*/ {  3,  8,  3, 11, 0, 0, 1, 0 },
/* Block_Static		*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Burned_Chip		*/ {  0,  2, -1, -1, 0, 0, 0, 0 },
/* Bombed_Chip		*/ {  0,  3, -1, -1, 0, 0, 0, 0 },
/* Exited_Chip		*/ {  0,  4, -1, -1, 0, 0, 0, 0 },
/* Exit_Extra_1		*/ {  0,  5, -1, -1, 0, 0, 0, 0 },
/* Exit_Extra_2		*/ {  0,  6, -1, -1, 0, 0, 0, 0 },
/* Overlay_Buffer	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Floor_Reserved3	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Floor_Reserved2	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Floor_Reserved1	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Chip			*/ { -1, -1,  5,  0, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 13,  0, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1,  9,  0, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 13,  1, 2, 0, 4, SIZE_EXTLEFT  },
/* Block		*/ { -1, -1,  5,  2, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 13,  2, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1,  9,  2, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 13,  3, 2, 0, 4, SIZE_EXTLEFT  },
/* Tank			*/ { -1, -1,  5,  4, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 13,  4, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1,  9,  4, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 13,  5, 2, 0, 4, SIZE_EXTLEFT  },
/* Ball			*/ { -1, -1, 21,  4, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 29,  4, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1, 25,  4, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 29,  5, 2, 0, 4, SIZE_EXTLEFT  },
/* Glider		*/ { -1, -1,  5,  6, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 13,  6, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1,  9,  6, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 13,  7, 2, 0, 4, SIZE_EXTLEFT  },
/* Fireball		*/ { -1, -1, 21,  6, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 29,  6, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1, 25,  6, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 29,  7, 2, 0, 4, SIZE_EXTLEFT  },
/* Walker		*/ { -1, -1,  5,  8, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 13,  8, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1,  9,  8, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 13,  9, 2, 0, 4, SIZE_EXTLEFT  },
/* Blob			*/ { -1, -1, 21,  8, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 29,  8, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1, 25,  8, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 29,  9, 2, 0, 4, SIZE_EXTLEFT  },
/* Teeth		*/ { -1, -1, 21,  2, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 29,  2, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1, 25,  2, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 29,  3, 2, 0, 4, SIZE_EXTLEFT  },
/* Bug			*/ { -1, -1,  5, 10, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 13, 10, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1,  9, 10, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 13, 11, 2, 0, 4, SIZE_EXTLEFT  },
/* Paramecium		*/ { -1, -1, 21, 10, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 29, 10, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1, 25, 10, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 29, 11, 2, 0, 4, SIZE_EXTLEFT  },
/* Swimming_Chip	*/ {  0,  8, -1, -1, 0, 0, 0, 0 },
			   {  0,  9, -1, -1, 0, 0, 0, 0 },
			   {  0, 10, -1, -1, 0, 0, 0, 0 },
			   {  0, 11, -1, -1, 0, 0, 0, 0 },
/* Pushing_Chip		*/ { -1, -1, 21,  0, 1, 0, 4, SIZE_EXTDOWN  },
			   { -1, -1, 29,  0, 2, 0, 4, SIZE_EXTRIGHT },
			   { -1, -1, 25,  0, 1, 0, 4, SIZE_EXTUP    },
			   { -1, -1, 29,  1, 2, 0, 4, SIZE_EXTLEFT  },
/* Entity_Reserved2	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Entity_Reserved1	*/ { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
			   { -1, -1, -1, -1, 0, 0, 0, 0 },
/* Water_Splash		*/ {  0,  7,  1, 12, 3, 0,12, SIZE_EXTALL },
/* Dirt_Splash		*/ { -1, -1,  1, 15, 3, 0,12, SIZE_EXTALL },
/* Bomb_Explosion	*/ { -1, -1,  1, 18, 3, 0,12, SIZE_EXTALL },
/* Animation_Reserved1	*/ { -1, -1, -1, -1, 0, 0, 0, 0 }
};

static Uint32	       *cctiles = NULL;
static tilemap		tileptr[NTILES];
static int		useanimation = FALSE;

/*
 * Functions for obtaining tile images.
 */

/* Return a pointer to a specific tile image.
 */
static Uint32 const *_gettileimage(int id, int transp)
{
    if (transp)
	return tileptr[id].transp[0] ? tileptr[id].transp[0]
				     : tileptr[id].opaque;
    else
	return tileptr[id].opaque ? tileptr[id].opaque
				  : tileptr[id].transp[0];
}

/* Return a pointer to a tile image for a creature, completing the
 * fields of the given rect.
 */
static Uint32 const *_getcreatureimage(SDL_Rect *rect,
				       int id, int dir, int moving)
{
    tilemap const      *q;

    rect->w = sdlg.wtile;
    rect->h = sdlg.htile;
    q = tileptr + id + diridx(dir);

    if (useanimation) {
	if (moving / 2 >= q->celcount)
	    die("requested cel #%d from a %d-cel sequence (%d+%d)",
		moving / 2, q->celcount, id, diridx(dir));
	if (q->transpsize & SIZE_EXTLEFT) {
	    rect->x -= sdlg.wtile;
	    rect->w += sdlg.wtile;
	}
	if (q->transpsize & SIZE_EXTRIGHT)
	    rect->w += sdlg.wtile;
	if (q->transpsize & SIZE_EXTUP) {
	    rect->y -= sdlg.htile;
	    rect->h += sdlg.htile;
	}
	if (q->transpsize & SIZE_EXTDOWN)
	    rect->h += sdlg.htile;
	return q->transp[moving / 2];
    } else {
	if (!isanimation(id) && moving > 0) {
	    switch (dir) {
	      case NORTH:	rect->y += moving * rect->h / 8;	break;
	      case WEST:	rect->x += moving * rect->w / 8;	break;
	      case SOUTH:	rect->y -= moving * rect->h / 8;	break;
	      case EAST:	rect->x -= moving * rect->w / 8;	break;
	    }
	}
	return q->transp[0] ? q->transp[0] : q->opaque;
    }
}

/* Return a pointer to a tile image for a cell. If the top image is
 * transparent, the appropriate image is created in the overlay
 * buffer.
 */
static Uint32 const *_getcellimage(int top, int bot, int timerval)
{
    Uint32     *src;
    Uint32     *dest;
    int		n;

    (void)timerval;
    if (bot == Nothing || bot == Empty || !tileptr[top].transp[0])
	return tileptr[top].opaque;
    src = tileptr[bot].opaque;
    dest = tileptr[Overlay_Buffer].opaque;
    memcpy(dest, src, sdlg.cbtile);
    src = tileptr[top].transp[0];
    for (n = 0 ; n < sdlg.cptile ; ++n)
	if (src[n] != (Uint32)sdlg.transpixel)
	    dest[n] = src[n];
    return dest;
}

/*
 *
 */

/*
 */
static SDL_Surface *copytilesto32(SDL_Surface *src, int wset, int hset)
{
    SDL_PixelFormat    *fmt;
    SDL_Surface	       *dest;
    SDL_Rect		rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = wset * sdlg.wtile;
    rect.h = hset * sdlg.htile;

    if (!sdlg.screen) {
	warn("inittileswithclrkey() called before creating 32-bit surface");
	fmt = SDL_GetVideoInfo()->vfmt;
    } else
	fmt = sdlg.screen->format;

    dest = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 32,
				fmt->Rmask, fmt->Gmask,
				fmt->Bmask, fmt->Amask);
    if (!dest)
	return NULL;

    SDL_BlitSurface(src, &rect, dest, &rect);

    return dest;
}

/*
 */
static SDL_Surface *extractmask(SDL_Surface *src,
				int xmask, int ymask, int wmask, int hmask)
{
    SDL_Surface	       *mask;
    SDL_Color		pal[2];
    SDL_Rect		srcrect, destrect;

    mask = SDL_CreateRGBSurface(SDL_SWSURFACE,
				wmask * sdlg.wtile, hmask * sdlg.htile, 8,
				0, 0, 0, 0);
    if (!mask)
	return NULL;
    pal[0].r = pal[0].g = pal[0].b = 0;
    pal[1].r = pal[1].g = pal[1].b = 255;
    SDL_SetPalette(mask, SDL_LOGPAL, pal, 0, 2);

    srcrect.x = xmask * sdlg.wtile;
    srcrect.y = ymask * sdlg.htile;
    srcrect.w = wmask * sdlg.wtile;
    srcrect.h = hmask * sdlg.htile;
    destrect.x = 0;
    destrect.y = 0;
    SDL_BlitSurface(src, &srcrect, mask, &destrect);

    return mask;
}

/* First, the tiles are transferred from the bitmap surface to a
 * 32-bit surface. Then, the mask is applied, setting the transparent
 * pixels.  Finally, the tiles are individually transferred to a
 * one-dimensional array.
 */
static int initsmalltileset(SDL_Surface *bmp, int wset, int hset,
			    int xmask, int ymask, int wmask, int hmask,
			    int xmaskdest, int ymaskdest)
{
    SDL_Surface	       *temp;
    SDL_Surface	       *masktemp;
    Uint8	       *mask;
    Uint32	       *src;
    Uint32	       *dest;
    Uint32		magenta;
    int			x, y, n;

    temp = copytilesto32(bmp, wset, hset);
    if (!temp)
	die("couldn't create temporary tile surface: %s", SDL_GetError());
    if (SDL_MUSTLOCK(temp))
	SDL_LockSurface(temp);

    if (xmask >= 0 && ymask >= 0) {
	masktemp = extractmask(bmp, xmask, ymask, wmask, hmask);
	if (!masktemp)
	    die("couldn't create temporary mask surface: %s", SDL_GetError());
	if (SDL_MUSTLOCK(masktemp))
	    SDL_LockSurface(masktemp);
	mask = (Uint8*)masktemp->pixels;
	dest = (Uint32*)((char*)temp->pixels + ymaskdest * temp->pitch);
	dest += xmaskdest * sdlg.wtile;
	for (y = 0 ; y < hmask * sdlg.htile ; ++y) {
	    for (x = 0 ; x < wmask * sdlg.wtile ; ++x)
		if (!mask[x])
		    dest[x] = (Uint32)sdlg.transpixel;
	    mask = (Uint8*)((char*)mask + masktemp->pitch);
	    dest = (Uint32*)((char*)dest + temp->pitch);
	}
	if (SDL_MUSTLOCK(masktemp))
	    SDL_UnlockSurface(masktemp);
	SDL_FreeSurface(masktemp);
    } else {
	magenta = SDL_MapRGB(temp->format, 255, 0, 255);
	dest = (Uint32*)((char*)temp->pixels + ymaskdest * temp->pitch);
	dest += xmaskdest * sdlg.wtile;
	for (y = 0 ; y < hmask * sdlg.htile ; ++y) {
	    for (x = 0 ; x < wmask * sdlg.wtile ; ++x)
		if (dest[x] == magenta)
		    dest[x] = sdlg.transpixel;
	    dest = (Uint32*)((char*)dest + temp->pitch);
	}
    }

    if (!(cctiles = calloc(wset * hset * sdlg.cptile, sizeof *cctiles)))
	memerrexit();
    dest = cctiles;
    for (x = 0 ; x < wset * sdlg.wtile ; x += sdlg.wtile) {
	for (y = 0 ; y < hset * sdlg.htile ; y += sdlg.htile) {
	    src = (Uint32*)((char*)temp->pixels + y * temp->pitch) + x;
	    for (n = sdlg.htile ; n ; --n, dest += sdlg.wtile) {
		memcpy(dest, src, sdlg.wtile * sizeof *dest);
		src = (Uint32*)((char*)src + temp->pitch);
	    }
	}
    }
    if (SDL_MUSTLOCK(temp))
	SDL_UnlockSurface(temp);
    SDL_FreeSurface(temp);

    for (n = 0 ; n < NTILES ; ++n) {
	if (small_tileidmap[n].xopaque >= 0)
	    tileptr[n].opaque = cctiles
			+ (small_tileidmap[n].xopaque * hset
				   + small_tileidmap[n].yopaque) * sdlg.cptile;
	else
	    tileptr[n].opaque = NULL;
	if (small_tileidmap[n].xtransp >= 0) {
	    tileptr[n].celcount = 1;
	    tileptr[n].transp[0] = cctiles
			+ (small_tileidmap[n].xtransp * hset
				   + small_tileidmap[n].ytransp) * sdlg.cptile;
	} else {
	    tileptr[n].celcount = 0;
	    tileptr[n].transp[0] = NULL;
	}
	tileptr[n].transpsize = 0;
    }

    useanimation = FALSE;
    return TRUE;
}

/*
 *
 */

/*
 */
static int initlargetileset(SDL_Surface *bmp, int wset, int hset)
{
    SDL_Surface	       *temp;
    Uint32	       *src;
    Uint32	       *dest;
    Uint32		magenta;
    int			x, y, w, h, z;
    int			i, j, n;

    temp = copytilesto32(bmp, wset, hset);
    if (!temp)
	die("couldn't create temporary tile surface: %s", SDL_GetError());
    magenta = SDL_MapRGB(temp->format, 255, 0, 255);
    if (SDL_MUSTLOCK(temp))
	SDL_LockSurface(temp);

    n = 0;
    for (i = 0 ; i < NTILES ; ++i) {
	if (large_tileidmap[i].xopaque >= 0)
	    ++n;
	if (large_tileidmap[i].xtransp >= 0) {
	    y = 1;
	    if (large_tileidmap[i].transpsize & SIZE_EXTUP)
		++y;
	    if (large_tileidmap[i].transpsize & SIZE_EXTDOWN)
		++y;
	    x = y;
	    if (large_tileidmap[i].transpsize & SIZE_EXTLEFT)
		x += y;
	    if (large_tileidmap[i].transpsize & SIZE_EXTRIGHT)
		x += y;
	    n += x * large_tileidmap[i].celcount;
	}
    }

    if (!(cctiles = calloc(n * sdlg.cptile, sizeof *cctiles)))
	memerrexit();

    dest = cctiles;
    for (n = 0 ; n < NTILES ; ++n) {
	if (large_tileidmap[n].xopaque >= 0) {
	    x = large_tileidmap[n].xopaque * sdlg.wtile;
	    y = large_tileidmap[n].yopaque * sdlg.htile;
	    src = (Uint32*)((char*)temp->pixels + y * temp->pitch) + x;
	    tileptr[n].opaque = dest;
	    for (j = 0 ; j < sdlg.htile ; ++j, dest += sdlg.wtile) {
		for (i = 0 ; i < sdlg.wtile ; ++i)
		    dest[i] = src[i] == magenta ? sdlg.transpixel : src[i];
		src = (Uint32*)((char*)src + temp->pitch);
	    }
	} else {
	    tileptr[n].opaque = NULL;
	}
	if (large_tileidmap[n].xtransp >= 0) {
	    x = large_tileidmap[n].xtransp * sdlg.wtile;
	    y = large_tileidmap[n].ytransp * sdlg.htile;
	    w = sdlg.wtile;
	    if (large_tileidmap[n].transpsize & SIZE_EXTLEFT)
		w += sdlg.wtile;
	    if (large_tileidmap[n].transpsize & SIZE_EXTRIGHT)
		w += sdlg.wtile;
	    h = sdlg.htile;
	    if (large_tileidmap[n].transpsize & SIZE_EXTUP)
		h += sdlg.htile;
	    if (large_tileidmap[n].transpsize & SIZE_EXTDOWN)
		h += sdlg.htile;
	    tileptr[n].celcount = large_tileidmap[n].celcount;
	    tileptr[n].transpsize = large_tileidmap[n].transpsize;
	    for (z = large_tileidmap[n].celcount ; z ; --z) {
		tileptr[n].transp[z - 1] = dest;
		src = (Uint32*)((char*)temp->pixels + y * temp->pitch) + x;
		for (j = 0 ; j < h ; ++j, dest += w) {
		    for (i = 0 ; i < w ; ++i)
			dest[i] = src[i] == magenta ? sdlg.transpixel : src[i];
		    src = (Uint32*)((char*)src + temp->pitch);
		}
		x += large_tileidmap[n].xceloff * sdlg.wtile;
		y += large_tileidmap[n].yceloff * sdlg.htile;
	    }
	} else {
	    tileptr[n].celcount = large_tileidmap[n].celcount;
	}
    }
    if (SDL_MUSTLOCK(temp))
	SDL_UnlockSurface(temp);
    SDL_FreeSurface(temp);

    useanimation = TRUE;
    return TRUE;
}

/*
 *
 */

void freetileset(void)
{
    free(cctiles);
    cctiles = NULL;
    sdlg.wtile = 0;
    sdlg.htile = 0;
    sdlg.cptile = 0;
    sdlg.cbtile = 0;
    memset(tileptr, 0, sizeof tileptr);
}

int loadtileset(char const *filename, int complain)
{
    SDL_Surface	       *bmp;
    int			large;
    int			xmask, ymask;
    int			x, y;

    bmp = SDL_LoadBMP(filename);
    if (!bmp) {
	if (complain)
	    errmsg(filename, "cannot read bitmap: %s", SDL_GetError());
	return FALSE;
    }

    if (bmp->w % 37 == 0 && bmp->h % 21 == 0) {
	x = bmp->w / 37;
	y = bmp->h / 21;
	xmask = ymask = -1;
	large = TRUE;
    } else if (bmp->w % 13 == 0 && bmp->h % 16 == 0) {
	x = bmp->w / 13;
	y = bmp->h / 16;
	xmask = 10;
	ymask = 0;
	large = FALSE;
    } else if (bmp->w % 10 == 0 && bmp->h % 16 == 0) {
	x = bmp->w / 10;
	y = bmp->h / 16;
	xmask = ymask = -1;
	large = FALSE;
    } else {
	errmsg(filename, "image file has invalid dimensions");
	goto failure;
    }
    if (x % 4 || y % 4) {
	errmsg(filename, "tile dimensions must be divisible by four");
	goto failure;
    }

    freetileset();
    sdlg.wtile = x;
    sdlg.htile = y;
    sdlg.cptile = x * y;
    sdlg.cbtile = sdlg.cptile * sizeof(Uint32);

    if (large) {
	if (!initlargetileset(bmp, 37, 21))
	    goto failure;
    } else {
	if (!initsmalltileset(bmp, 10, 16, xmask, ymask, 3, 16, 7, 0))
	    goto failure;
    }

    SDL_FreeSurface(bmp);
    return TRUE;

  failure:
    SDL_FreeSurface(bmp);
    return FALSE;
}

/*
 *
 */

int _sdltileinitialize(void)
{
    sdlg.gettileimagefunc = _gettileimage;
    sdlg.getcreatureimagefunc = _getcreatureimage;
    sdlg.getcellimagefunc = _getcellimage;
    return TRUE;
}
