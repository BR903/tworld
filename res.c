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

#define	RES_SND_CHIP_LOSES	SND_CHIP_LOSES
#define	RES_SND_CHIP_WINS	SND_CHIP_WINS
#define	RES_SND_TIME_OUT	SND_TIME_OUT
#define	RES_SND_TIME_LOW	SND_TIME_LOW
#define	RES_SND_CANT_MOVE	SND_CANT_MOVE
#define	RES_SND_IC_COLLECTED	SND_IC_COLLECTED
#define	RES_SND_ITEM_COLLECTED	SND_ITEM_COLLECTED
#define	RES_SND_BOOTS_STOLEN	SND_BOOTS_STOLEN
#define	RES_SND_TELEPORTING	SND_TELEPORTING
#define	RES_SND_DOOR_OPENED	SND_DOOR_OPENED
#define	RES_SND_SOCKET_OPENED	SND_SOCKET_OPENED
#define	RES_SND_BUTTON_PUSHED	SND_BUTTON_PUSHED
#define	RES_SND_TILE_EMPTIED	SND_TILE_EMPTIED
#define	RES_SND_WALL_CREATED	SND_WALL_CREATED
#define	RES_SND_TRAP_SPRUNG	SND_TRAP_SPRUNG
#define	RES_SND_BLOCK_MOVING	SND_BLOCK_MOVING
#define	RES_SND_BOMB_EXPLODES	SND_BOMB_EXPLODES
#define	RES_SND_WATER_SPLASH	SND_WATER_SPLASH
#define	RES_SND_SLIDEWALKING	SND_SLIDEWALKING
#define	RES_SND_ICEWALKING	SND_ICEWALKING
#define	RES_SND_WATERWALKING	SND_WATERWALKING
#define	RES_SND_FIREWALKING	SND_FIREWALKING
#define	RES_SND_SKATING_FORWARD	SND_SKATING_FORWARD
#define	RES_SND_SKATING_TURN	SND_SKATING_TURN
#define	RES_SND_SLIDING		SND_SLIDING
#define	RES_LASTSOUND		SND_SLIDING

#define	RES_SILENT		(RES_LASTSOUND + 1)
#define	RES_TILEIMAGES		(RES_LASTSOUND + 2)
#define	RES_USEANIM		(RES_LASTSOUND + 3)

#define	RES_COUNT		(RES_LASTSOUND + 4)

typedef	struct rcitem {
    char const *name;
    int		numeric;
} rcitem;

typedef union resourceitem {
    int		num;
    char	str[256];
} resourceitem;

static rcitem rclist[] = {
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
    { "trapsprungsound",	FALSE },
    { "blockmovingsound",	FALSE },
    { "bombsound",		FALSE },
    { "splashsound",		FALSE },
    { "slidewalkingsound",	FALSE },
    { "icewalkingsound",	FALSE },
    { "waterwalkingsound",	FALSE },
    { "firewalkingsound",	FALSE },
    { "skatingforwardsound",	FALSE },
    { "skatingturnsound",	FALSE },
    { "slidingsound",		FALSE },
    { "silent",			TRUE },
    { "tileimages",		FALSE },
    { "useanimation",		TRUE }
};

static resourceitem	globalresources[RES_COUNT];
static resourceitem	ms_resources[RES_COUNT];
static resourceitem	lynx_resources[RES_COUNT];

/* The active ruleset.
 */
static int	currentruleset = Ruleset_None;

/* The directory containing all the resource files.
 */
char	       *resdir = NULL;

/*
 *
 */

static void initresourcedefaults(void)
{
    strcpy(globalresources[RES_SND_CHIP_LOSES].str,	"bummer.wav");
    strcpy(globalresources[RES_SND_CHIP_WINS].str,	"ditty1.wav");
    strcpy(globalresources[RES_SND_TIME_OUT].str,	"bell.wav");
    strcpy(globalresources[RES_SND_TIME_LOW].str,	"click1.wav");
    strcpy(globalresources[RES_SND_CANT_MOVE].str,	"oof3.wav");
    strcpy(globalresources[RES_SND_IC_COLLECTED].str,	"click3.wav");
    strcpy(globalresources[RES_SND_ITEM_COLLECTED].str,	"blip2.wav");
    strcpy(globalresources[RES_SND_BOOTS_STOLEN].str,	"strike.wav");
    strcpy(globalresources[RES_SND_TELEPORTING].str,	"teleport.wav");
    strcpy(globalresources[RES_SND_DOOR_OPENED].str,	"door.wav");
    strcpy(globalresources[RES_SND_SOCKET_OPENED].str,	"chimes.wav");
    strcpy(globalresources[RES_SND_BUTTON_PUSHED].str,	"pop2.wav");
    strcpy(globalresources[RES_SND_BOMB_EXPLODES].str,	"hit3.wav");
    strcpy(globalresources[RES_SND_WATER_SPLASH].str,	"water2.wav");
    strcpy(globalresources[RES_TILEIMAGES].str,		"tiles.bmp");
    globalresources[RES_SILENT].num = TRUE;
    globalresources[RES_USEANIM].num = FALSE;

    memcpy(ms_resources, globalresources, sizeof ms_resources);
    memcpy(lynx_resources, globalresources, sizeof lynx_resources);
}

static void lower(char *str)
{
    char       *p;

    for (p = str ; *p ; ++p)
	*p = tolower(*p);
}

static int readrcfile(void)
{
    resourceitem	item;
    fileinfo		file;
    char		buf[256];
    char		name[256];
    int			ruleset;
    int			lineno, i;

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
	    lower(name);
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
	lower(name);
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
	switch (ruleset) {
	  case Ruleset_MS:
	    ms_resources[i] = item;
	    break;
	  case Ruleset_Lynx:
	    lynx_resources[i] = item;
	    break;
	  case Ruleset_None:
	    globalresources[i] = item;
	    ms_resources[i] = item;
	    lynx_resources[i] = item;
	    break;
	}
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

    path = getpathbuffer();

    switch (currentruleset) {
      case Ruleset_MS:
	f = FALSE;
	if (*ms_resources[RES_TILEIMAGES].str) {
	    combinepath(path, resdir, ms_resources[RES_TILEIMAGES].str);
	    f = loadsmalltileset(path, FALSE);
	}
	break;
      case Ruleset_Lynx:
	f = FALSE;
	if (*lynx_resources[RES_TILEIMAGES].str) {
	    combinepath(path, resdir, lynx_resources[RES_TILEIMAGES].str);
	    if (lynx_resources[RES_USEANIM].num)
		f = loadlargetileset(path, FALSE);
	    else
		f = loadsmalltileset(path, FALSE);
	}
	break;
      default:
	f = FALSE;
	break;
    }
    if (f) {
	free(path);
	return TRUE;
    }

    if (*globalresources[RES_TILEIMAGES].str) {
	combinepath(path, resdir, globalresources[RES_TILEIMAGES].str);
	f = loadsmalltileset(path, TRUE);
    }

    free(path);
    if (!f)
	errmsg(resdir, "no tilesets found");
    return f;
}

#if 0

/*
 *
 */

static int loadfont(void)
{
    fileinfo		file;
    unsigned char      *fontdata;
    unsigned short	sig;
    unsigned char	n;

    memset(&file, 0, sizeof file);
    if (!openfileindir(&file, resdir, "font.psf", "rb", "cannot access font"))
	return FALSE;
    if (!filereadint16(&file, &sig, "cannot read font file"))
	goto failure;
    if (sig != PSF_SIG) {
	fileerr(&file, "not a PSF font file");
	goto failure;
    }
    if (!filereadint8(&file, &n, "not a valid PSF font file"))
	goto failure;
    if (!filereadint8(&file, &n, "not a valid PSF font file"))
	goto failure;
    if (n < 8 || n > 32) {
	fileerr(&file, "invalid font size");
	goto failure;
    }
    fontdata = filereadbuf(&file, n * 256, "invalid font file");
    if (!fontdata)
	goto failure;
    if (!setfontresource(9, n, fontdata))
	goto failure;

    fileclose(&file, NULL);
    return TRUE;

  failure:
    if (fontdata)
	free(fontdata);
    fileclose(&file, NULL);
    return FALSE;
}

#endif

/*
 *
 */

static int loadsounds(void)
{
    return FALSE;
}

/*
 *
 */

int loadgameresources(int ruleset)
{
    static int	initialized = FALSE;

    if (!initialized) {
	initialized = TRUE;
	initresourcedefaults();
	readrcfile();
    }

    currentruleset = ruleset;
    if (!loadimages())
	return FALSE;
    loadsounds();
    return TRUE;
}
