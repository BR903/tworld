/* res.c: Functions for loading resources from external files.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"defs.h"
#include	"fileio.h"
#include	"err.h"
#include	"oshw.h"
#include	"res.h"

#define	RES_IMG_TILES		0
#define	RES_IMG_FONT		1
#define	RES_IMG_LAST		RES_IMG_FONT

#define	RES_SND_BASE		(RES_IMG_LAST + 1)
#define	RES_SND_CHIP_LOSES	(RES_SND_BASE + SND_CHIP_LOSES)
#define	RES_SND_CHIP_WINS	(RES_SND_BASE + SND_CHIP_WINS)
#define	RES_SND_TIME_OUT	(RES_SND_BASE + SND_TIME_OUT)
#define	RES_SND_TIME_LOW	(RES_SND_BASE + SND_TIME_LOW)
#define	RES_SND_CANT_MOVE	(RES_SND_BASE + SND_CANT_MOVE)
#define	RES_SND_IC_COLLECTED	(RES_SND_BASE + SND_IC_COLLECTED)
#define	RES_SND_ITEM_COLLECTED	(RES_SND_BASE + SND_ITEM_COLLECTED)
#define	RES_SND_BOOTS_STOLEN	(RES_SND_BASE + SND_BOOTS_STOLEN)
#define	RES_SND_TELEPORTING	(RES_SND_BASE + SND_TELEPORTING)
#define	RES_SND_DOOR_OPENED	(RES_SND_BASE + SND_DOOR_OPENED)
#define	RES_SND_SOCKET_OPENED	(RES_SND_BASE + SND_SOCKET_OPENED)
#define	RES_SND_BUTTON_PUSHED	(RES_SND_BASE + SND_BUTTON_PUSHED)
#define	RES_SND_TILE_EMPTIED	(RES_SND_BASE + SND_TILE_EMPTIED)
#define	RES_SND_WALL_CREATED	(RES_SND_BASE + SND_WALL_CREATED)
#define	RES_SND_TRAP_ENTERED	(RES_SND_BASE + SND_TRAP_ENTERED)
#define	RES_SND_BOMB_EXPLODES	(RES_SND_BASE + SND_BOMB_EXPLODES)
#define	RES_SND_WATER_SPLASH	(RES_SND_BASE + SND_WATER_SPLASH)
#define	RES_SND_SKATING_TURN	(RES_SND_BASE + SND_SKATING_TURN)
#define	RES_SND_BLOCK_MOVING	(RES_SND_BASE + SND_BLOCK_MOVING)
#define	RES_SND_SKATING_FORWARD	(RES_SND_BASE + SND_SKATING_FORWARD)
#define	RES_SND_SLIDING		(RES_SND_BASE + SND_SLIDING)
#define	RES_SND_SLIDEWALKING	(RES_SND_BASE + SND_SLIDEWALKING)
#define	RES_SND_ICEWALKING	(RES_SND_BASE + SND_ICEWALKING)
#define	RES_SND_WATERWALKING	(RES_SND_BASE + SND_WATERWALKING)
#define	RES_SND_FIREWALKING	(RES_SND_BASE + SND_FIREWALKING)
#define	RES_SND_LAST		RES_SND_FIREWALKING

#define	RES_COUNT		(RES_SND_LAST + 1)

typedef	struct rcitem {
    char const *name;
    int		numeric;
} rcitem;

typedef union resourceitem {
    int		num;
    char	str[256];
} resourceitem;

static rcitem rclist[] = {
    { "tileimages",		FALSE },
    { "font",			FALSE },
    { "chipdeathsound",		FALSE },
    { "levelcompletesound",	FALSE },
    { "chipdeathbytimesound",	FALSE },
    { "ticksound",		FALSE },
    { "blockedmovesound",	FALSE },
    { "pickupchipsound",	FALSE },
    { "pickuptoolsound",	FALSE },
    { "thiefsound",		FALSE },
    { "teleportsound",		FALSE },
    { "opendoorsound",		FALSE },
    { "socketsound",		FALSE },
    { "switchsound",		FALSE },
    { "tileemptiedsound",	FALSE },
    { "wallcreatedsound",	FALSE },
    { "trapenteredsound",	FALSE },
    { "bombsound",		FALSE },
    { "splashsound",		FALSE },
    { "blockmovingsound",	FALSE },
    { "skatingforwardsound",	FALSE },
    { "skatingturnsound",	FALSE },
    { "slidingsound",		FALSE },
    { "slidewalkingsound",	FALSE },
    { "icewalkingsound",	FALSE },
    { "waterwalkingsound",	FALSE },
    { "firewalkingsound",	FALSE }
};

static resourceitem	allresources[Ruleset_Count][RES_COUNT];
static resourceitem    *globalresources = allresources[Ruleset_None];
static resourceitem    *resources = NULL;

/* The active ruleset.
 */
static int		currentruleset = Ruleset_None;

/* The directory containing all the resource files.
 */
char		       *resdir = NULL;

/*
 *
 */

static void initresourcedefaults(void)
{
    strcpy(allresources[Ruleset_None][RES_IMG_TILES].str, "tiles.bmp");
    strcpy(allresources[Ruleset_None][RES_IMG_FONT].str, "font.bmp");
#if 0
    strcpy(globalresources[RES_IMG_TILES].str, "tiles.bmp");
    strcpy(globalresources[RES_IMG_FONT].str, "font.bmp");
    strcpy(globalresources[RES_SND_CHIP_LOSES].str, "bummer.wav");
    strcpy(globalresources[RES_SND_CHIP_WINS].str, "ditty1.wav");
    strcpy(globalresources[RES_SND_TIME_OUT].str, "bell.wav");
    strcpy(globalresources[RES_SND_TIME_LOW].str, "click1.wav");
    strcpy(globalresources[RES_SND_CANT_MOVE].str, "oof3.wav");
    strcpy(globalresources[RES_SND_IC_COLLECTED].str, "click3.wav");
    strcpy(globalresources[RES_SND_ITEM_COLLECTED].str, "blip2.wav");
    strcpy(globalresources[RES_SND_BOOTS_STOLEN].str, "strike.wav");
    strcpy(globalresources[RES_SND_TELEPORTING].str, "teleport.wav");
    strcpy(globalresources[RES_SND_DOOR_OPENED].str, "door.wav");
    strcpy(globalresources[RES_SND_SOCKET_OPENED].str, "chimes.wav");
    strcpy(globalresources[RES_SND_BUTTON_PUSHED].str, "pop2.wav");
    strcpy(globalresources[RES_SND_BOMB_EXPLODES].str, "hit3.wav");
    strcpy(globalresources[RES_SND_WATER_SPLASH].str, "water2.wav");
#endif
    memcpy(&allresources[Ruleset_MS], &allresources[Ruleset_None],
				sizeof allresources[Ruleset_None]);
    memcpy(&allresources[Ruleset_Lynx], &allresources[Ruleset_None],
				sizeof allresources[Ruleset_None]);
}

static int readrcfile(void)
{
    resourceitem	item;
    fileinfo		file;
    char		buf[256];
    char		name[256];
    char	       *p;
    int			ruleset;
    int			lineno, i, j;

    memset(&file, 0, sizeof file);
    if (!openfileindir(&file, resdir, "rc", "r", NULL))
	return FALSE;

    ruleset = Ruleset_None;
    for (lineno = 1 ; ; ++lineno) {
	i = sizeof buf - 1;
	if (!filegetline(&file, buf, &i, NULL))
	    break;
	if (*buf == '\n' || *buf == '#')
	    continue;
	if (sscanf(buf, "[%[^]]]", name) == 1) {
	    for (p = name ; (*p = tolower(*p)) != '\0' ; ++p) ;
	    if (!strcmp(name, "ms"))
		ruleset = Ruleset_MS;
	    else if (!strcmp(name, "lynx"))
		ruleset = Ruleset_Lynx;
	    else if (!strcmp(name, "all"))
		ruleset = Ruleset_None;
	    else
		warn("rc:%d: syntax error", lineno);
	    continue;
	}
	if (sscanf(buf, "%[^=]=%s", name, item.str) != 2) {
	    warn("rc:%d: syntax error", lineno);
	    continue;
	}
	for (p = name ; (*p = tolower(*p)) != '\0' ; ++p) ;
	for (i = sizeof rclist / sizeof *rclist - 1 ; i >= 0 ; --i)
	    if (!strcmp(name, rclist[i].name))
		break;
	if (i < 0) {
	    warn("rc:%d: illegal resource name \"%s\"", lineno, name);
	    continue;
	}
	if (rclist[i].numeric) {
	    i = atoi(item.str);
	    item.num = i;
	}
	allresources[ruleset][i] = item;
	if (ruleset == Ruleset_None)
	    for (j = Ruleset_None ; j < Ruleset_Count ; ++j)
		allresources[j][i] = item;
    }

    fileclose(&file, NULL);
    return TRUE;
}

/*
 *
 */

static int loadimages(void)
{
    char       *path;
    int		f;

    f = FALSE;
    path = getpathbuffer();
    if (*resources[RES_IMG_TILES].str) {
	combinepath(path, resdir, resources[RES_IMG_TILES].str);
	f = loadtileset(path, TRUE);
    }
    if (!f && resources != globalresources
	   && *globalresources[RES_IMG_TILES].str) {
	combinepath(path, resdir, globalresources[RES_IMG_TILES].str);
	f = loadtileset(path, TRUE);
    }
    free(path);

    if (!f)
	errmsg(resdir, "no valid tilesets found");
    return f;
}

/*
 *
 */

static int loadfont(void)
{
    char       *path;
    int		f;

    f = FALSE;
    path = getpathbuffer();
    if (*resources[RES_IMG_FONT].str) {
	combinepath(path, resdir, resources[RES_IMG_FONT].str);
	f = loadfontfromfile(path);
    }
    if (!f && resources != globalresources
	   && *globalresources[RES_IMG_FONT].str) {
	combinepath(path, resdir, globalresources[RES_IMG_FONT].str);
	f = loadfontfromfile(path);
    }
    free(path);
    return f;
}

/*
 *
 */

static int loadsounds(void)
{
    char       *path;
    int		count;
    int		n, f;

    path = getpathbuffer();
    count = 0;
    for (n = 0 ; n < SND_COUNT ; ++n) {
	f = FALSE;
	if (*resources[RES_SND_BASE + n].str) {
	    combinepath(path, resdir, resources[RES_SND_BASE + n].str);
	    f = loadsfxfromfile(n, path);
	}
	if (!f && resources != globalresources
	       && *globalresources[RES_SND_BASE + n].str) {
	    combinepath(path, resdir, globalresources[RES_SND_BASE + n].str);
	    f = loadsfxfromfile(n, path);
	}
	if (f)
	    ++count;
    }
    free(path);
    return count;
}

/*
 *
 */

int loadgameresources(int ruleset)
{
    currentruleset = ruleset;
    resources = allresources[ruleset];
    loadfont();
    if (!loadimages())
	return FALSE;
    loadsounds();
    return TRUE;
}

int initresources(void)
{
    initresourcedefaults();
    resources = allresources[Ruleset_None];
    return readrcfile() && loadfont();
}

void freeallresources(void)
{
    int	n;

    freefont();
    freetileset();
    for (n = 0 ; n < SND_COUNT ; ++n)
	 freesfx(n);
}
