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

/* The total number of tile images.
 */
#define	NTILES		128

/* The definition of a font.
 */
typedef	struct fontinfo {
    short		w;	/* width of each character */
    short		h;	/* height of each character */
    unsigned char      *bits;	/* 256 characters times h bytes */
} fontinfo;

/* The data mantained for a scrolling list.
 */
typedef	struct scrollinfo {
    SDL_Rect		area;		/* the rectangle to draw in */
    Uint32		bkgnd;		/* the background color index */
    Uint32		highlight;	/* the highlight color index */
    int			itemcount;	/* the number of items in the list */
    short		linecount;	/* how many lines fit in area */
    short		maxlen;		/* how many chars fit in one line */
    int			topitem;	/* the uppermost visible line */
    int			index;		/* the line currently selected */
    char const	      **items;		/* the list of lines of text */
} scrollinfo;

/* Flags to the various puttext functions.
 */
#define	PT_CENTER	0x0001		/* center the text horizontally */
#define	PT_RIGHT	0x0002		/* right-align the text */
#define	PT_MULTILINE	0x0004		/* span lines & break at whitespace */
#define	PT_UPDATERECT	0x0008		/* return the unused area in rect */
#define	PT_CALCSIZE	0x0010		/* determine area needed for text */

/* Values global to this module. All the globals are placed in here,
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
    Uint32		textcolor;
    Uint32		bkgndcolor;
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

    /* Display a line (or more) of text.
     */
    void (*puttextfunc)(SDL_Rect *area, char const *text, int len, int flags);

    /* Create a scrolling list, initializing the given scrollinfo
     * structure with the other arguments.
     */
    int (*createscrollfunc)(scrollinfo *scroll, SDL_Rect const *area,
			    Uint32 highlightcolor,
			    int itemcount, char const **items);

    /* Change the index of the scrolling list's selection by delta.
     */
    int (*scrollmovefunc)(scrollinfo *scroll, int delta);

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
