/* sdlgen.h: The internal shared definitions of the SDL OS/hardware layer.
 * 
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_sdlgen_h_
#define	_sdlgen_h_

#include	"SDL.h"
#include	"../gen.h"
#include	"../oshw.h"

/* The data defining a font.
 */
typedef	struct fontinfo {
    signed char	h;		/* height of each character */
    signed char	w[256];		/* width of each character */
    void       *memory;		/* memory allocated for the font */
    char       *bits[256];	/* pointers to each glyph */
} fontinfo;

/* The data defining a font's coloring.
 */
typedef	struct fontcolors { Uint32 c[3]; } fontcolors;

#define	bkgndcolor(fc)	((fc).c[0])	/* the background color */
#define	halfcolor(fc)	((fc).c[1])	/* the antialiasing color */
#define	textcolor(fc)	((fc).c[2])	/* the main color of the glyphs */

/* Flags to the puttext functions.
 */
#define	PT_CENTER	0x0001		/* center the text horizontally */
#define	PT_RIGHT	0x0002		/* right-align the text */
#define	PT_MULTILINE	0x0004		/* span lines & break at whitespace */
#define	PT_UPDATERECT	0x0008		/* return the unused area in rect */
#define	PT_CALCSIZE	0x0010		/* determine area needed for text */
#define	PT_DIM		0x0020
#define	PT_HILIGHT	0x0040

/*
 * Values global to this module. All the globals are placed in here,
 * in order to minimize the namespace pollution of the calling module.
 */

typedef	struct oshwglobals
{
    /* 
     * Shared variables.
     */

    short		wtile;		/* width of one tile in pixels */
    short		htile;		/* height of one tile in pixels */
    short		cptile;		/* size of one tile in pixels */
    short		cbtile;		/* size of one tile in bytes */
    fontcolors		textclr;
    fontcolors		dimtextclr;
    fontcolors		hilightclr;
    Uint32		transpixel;	/* value of the transparent pixel */
    SDL_Surface	       *screen;		/* the display */
    fontinfo		font;		/* the font */
    SDL_Rect		textsfxrect;	/* where onomatopoeia are displayed */

    /* 
     * Shared functions.
     */

    /* Process all pending events. If no events are currently pending
     * and wait is TRUE, the function blocks until an event occurs.
     */
    void (*eventupdatefunc)(int wait);

    /* A function to be called every time a keyboard key is pressed or
     * released. scancode is an SDL key symbol. down is TRUE if the
     * key was pressed or FALSE if it was released.
     */
    void (*keyeventcallbackfunc)(int scancode, int down);

    /* Display a line (or more) of text in the font.
     */
    void (*puttextfunc)(SDL_Rect *area, char const *text, int len, int flags);

    /* Determine the widths of the columns of a table of text. Each
     * member of rows is one row of the table, with tabs separating
     * the individual columns. The width fields of the rects array are
     * filled in with the minimum width needed to display all the
     * entries in each column. The return value is the number of
     * columns in the entire table.
     */
    SDL_Rect *(*measuretablefunc)(SDL_Rect const *area,
				  tablespec const *table);

    int (*drawtablerowfunc)(tablespec const *table, SDL_Rect *cols,
			    int *row, int flags);

    /* Return a pointer to a specific tile image.
     */
    Uint32 const* (*gettileimagefunc)(int id, int transp);

    /* Return a pointer to a tile image for a creature.
     */
    Uint32 const* (*getcreatureimagefunc)(int id, int dir, int moving);

    /* Return a pointer to a tile image for a cell. If the top image
     * is transparent, the appropriate image is created in the overlay
     * buffer.
     */
    Uint32 const* (*getcellimagefunc)(int top, int bot);

} oshwglobals;

extern oshwglobals sdlg;

/* Some convenience macros for the above functions.
 */
#define eventupdate		(*sdlg.eventupdatefunc)
#define	keyeventcallback	(*sdlg.keyeventcallbackfunc)
#define	puttext			(*sdlg.puttextfunc)
#define	measuretable		(*sdlg.measuretablefunc)
#define	drawtablerow		(*sdlg.drawtablerowfunc)
#define	createscroll		(*sdlg.createscrollfunc)
#define	scrollmove		(*sdlg.scrollmovefunc)
#define	gettileimage		(*sdlg.gettileimagefunc)
#define	getcreatureimage	(*sdlg.getcreatureimagefunc)
#define	getcellimage		(*sdlg.getcellimagefunc)
#define	getonomatopoeia		(*sdlg.getonomatopoeiafunc)

/* The initialization functions.
 */
extern int _sdltimerinitialize(int showhistogram);
extern int _sdlresourceinitialize(void);
extern int _sdltextinitialize(void);
extern int _sdltileinitialize(void);
extern int _sdlinputinitialize(void);
extern int _sdloutputinitialize(void);
extern int _sdlsfxinitialize(int silence);

#endif
