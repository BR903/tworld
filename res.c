/* res.c: Functions for loading resources from external files.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	"defs.h"
#include	"fileio.h"
#include	"err.h"
#include	"oshw.h"
#include	"res.h"

#define	RES_MSTILES	0
#define	RES_LYNXTILES	1
#define	RES_GENTILES	2
#define	RES_LYNXANIM	3

typedef	struct resourcespec {
    char const *name;
    int		numeric;
    int		defined;
    int		num;
    char	str[256];
} resourcespec;

static resourcespec resources[] = {
    { "MSTileImages",		FALSE, TRUE,  0, "ms_tiles.bmp" },
    { "LynxTileImages",		FALSE, TRUE,  0, "lx_tiles.bmp" },
    { "TileImages",		FALSE, TRUE,  0, "tiles.bmp" },
    { "UseAnimation",		TRUE,  TRUE,  1, "" },
    { "Silent",			TRUE,  TRUE,  0, "" },
    { "PickUpToolSound",	FALSE, TRUE,  0, "blip2.wav" },
    { "OpenDoorSound",		FALSE, TRUE,  0, "door.wav" },
    { "ChipDeathSound",		FALSE, TRUE,  0, "bummer.wav" },
    { "LevelCompleteSound",	FALSE, TRUE,  0, "ditty1.wav" },
    { "SocketSound",		FALSE, TRUE,  0, "chimes.wav" },
    { "BlockedMoveSound",	FALSE, TRUE,  0, "oof3.wav" },
    { "ThiefSound",		FALSE, TRUE,  0, "strike.wav" },
    { "PickUpChipSound",	FALSE, TRUE,  0, "click3.wav" },
    { "SwitchSound",		FALSE, TRUE,  0, "pop2.wav" },
    { "SplashSound",		FALSE, TRUE,  0, "water2.wav" },
    { "BombSound",		FALSE, TRUE,  0, "hit3.wav" },
    { "TeleportSound",		FALSE, TRUE,  0, "teleport.wav" },
    { "TickSound",		FALSE, TRUE,  0, "click1.wav" },
    { "ChipDeathByTimeSound",	FALSE, TRUE,  0, "bell.wav" },
};

/* The active ruleset.
 */
static int	currentruleset = Ruleset_None;

/* The directory containing all the resource files.
 */
char	       *resdir = NULL;

/*
 *
 */

#if 0
static int readrcfile(void)
{
    fileinfo	file;
    char	buf[256];
    char	name[256], value[256];
    int		lineno, i;

    memset(&file, 0, sizeof file);
    if (!openfileindir(&file, resdir, "rc", "r", NULL))
	return FALSE;

    lineno = 0;
    for (;;) {
	i = sizeof buf - 1;
	if (!filegetline(&file, buf, &i, NULL))
	    break;
	++lineno;
	if (!*buf || *buf == '#')
	    continue;
	if (sscanf(buf, "%s=%s", name, value) != 2) {
	    warn("rc:%d syntax error", lineno);
	    continue;
	}
	for (i = sizeof resources / sizeof *resources - 1 ; i >= 0 ; --i)
	    if (!strcmp(name, resources[i].name))
		break;
	if (i < 0) {
	    warn("rc:%d: illegal resource name \"%s\"", lineno, name);
	    continue;
	}
	if (resources[i].numeric)
	    resources[i].num = atoi(value);
	else
	    strcpy(resources[i].str, value);
	resources[i].defined = TRUE;
    }

    fileclose(&file, NULL);
    return TRUE;
}
#endif

/*
 *
 */

static int loadimages(void)
{
    char       *path;
    int		f;

    path = getpathbuffer();

    strcpy(path, resdir);
    switch (currentruleset) {
      case Ruleset_MS:
	f = FALSE;
	if (resources[RES_MSTILES].defined) {
	    combinepath(path, resources[RES_MSTILES].str);
	    f = loadsmalltileset(path, FALSE);
	}
	break;
      case Ruleset_Lynx:
	f = FALSE;
	if (resources[RES_LYNXTILES].defined) {
	    combinepath(path, resources[RES_LYNXTILES].str);
	    if (resources[RES_LYNXANIM].defined && resources[RES_LYNXANIM].num)
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

    strcpy(path, resdir);
    if (resources[RES_GENTILES].defined) {
	combinepath(path, resources[RES_GENTILES].str);
	f = loadsmalltileset(path, TRUE);
    }

    free(path);
    if (!f)
	errmsg(resdir, "no tilesets found");
    return f;
}

static int loadsounds(void)
{
    return FALSE;
}

/*
 *
 */

int loadgameresources(int ruleset)
{
    /*readrcfile();*/
    currentruleset = ruleset;
    if (!loadimages())
	return FALSE;
    loadsounds();
    return TRUE;
}
