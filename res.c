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

/* The directory containing all the resource files.
 */
char	       *resdir = NULL;

/*
 *
 */

static int loadimages(int ruleset)
{
    char       *path;
    int		f;

    path = getpathbuffer();
    strcpy(path, resdir);
    switch (ruleset) {
      case Ruleset_MS:
	combinepath(path, "ms_tiles.bmp");
	f = loadsmalltileset(path, FALSE);
	break;
      case Ruleset_Lynx:
	combinepath(path, "lx_tiles.bmp");
	f = loadlargetileset(path, FALSE);
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
    combinepath(path, "tiles.bmp");
    if (loadsmalltileset(path, TRUE)) {
	free(path);
	return TRUE;
    }
    errmsg(resdir, "no tilesets found");
    free(path);
    return FALSE;
}

static int loadsounds(int ruleset)
{
    return ruleset - ruleset;
}

/*
 *
 */

int loadgameresources(int ruleset)
{
    if (!loadimages(ruleset))
	return FALSE;
    loadsounds(ruleset);
    return TRUE;
}
