#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<errno.h>
#include	<stdarg.h>
#include	<unistd.h>
#include	<vga.h>
#include	<vgagl.h>
#include	"gen.h"
#include	"cc.h"
#include	"objects.h"
#include	"statestr.h"
#include	"userout.h"

#include	"font_9x16.c"
#include	"cctiles.c"

#define	CXSCREEN	640
#define	CYSCREEN	480
#define	CXFONT		9
#define	CYFONT		16

#define	CXMARGIN	16
#define	CYMARGIN	16
/*
#define	CXTILE		48
#define	CYTILE		48
*/
#define	CXDISPLAY	9
#define	CYDISPLAY	9
#define	CXVIEW		13
#define	CYVIEW		13

#define	XMIDDISPLAY	4
#define	YMIDDISPLAY	4
#define	XMIDVIEW	6
#define	YMIDVIEW	6

#define	XINFO		(CXMARGIN + CXDISPLAY * CXTILE + CXMARGIN)
#define	YINFO		CYMARGIN

#define	NTILES		(Count_Floors + 4 * Count_Entities)

static GraphicsContext *virtscreen = NULL;
static GraphicsContext *realscreen = NULL;
static int		origpalette[256 * 3];

static int		silence;

char const	       *programname = "a.out";
char const	       *currentfilename = NULL;

#define	floortile(id)		(cctiles + (id) * CXTILE * CYTILE)
#define	entitydirtile(id, dir)	\
    (cctiles + (Count_Floors + (id) * 4 + (dir)) * CXTILE * CYTILE)

/*
 *
 */

static void copytile(unsigned char *dest, unsigned char const *src)
{
    int	n;

    for (n = 0 ; n < CXTILE * CYTILE ; ++n)
	dest[n] = src[n] & 0x0F;
}

static void addtile(unsigned char *dest, unsigned char const *src)
{
    int	n;

    for (n = 0 ; n < CXTILE * CYTILE ; ++n)
	if (!(src[n] & 0xF0))
	    dest[n] = src[n];
}

static void puttile(unsigned char *view, int ypos, int xpos,
		    unsigned char const *tile)
{
    unsigned char const	       *src = tile;
    unsigned char	       *dest;
    int				x, y;

    dest = view + ypos * (CYTILE * CXTILE * CXVIEW) / 4 + xpos * CXTILE / 4;
    for (y = 0 ; y < CYTILE ; ++y, dest += CXTILE * CXVIEW)
	for (x = 0 ; x < CXTILE ; ++x, ++src)
	    if (!(*src & 0xF0))
		dest[x] = *src;
}

static void initializetiles(void)
{
    unsigned char      *p;

    if (sizeof cctiles < NTILES * CXTILE * CYTILE)
	die("%d bytes of tile data; expecting %d.\n",
	    sizeof cctiles, NTILES * CXTILE * CYTILE);

    for (p = cctiles ; p < cctiles + NTILES * CXTILE * CYTILE ; ++p)
	if (*p != ' ')
	    *p &= 0x0F;
}

/*
 *
 */

static void writemlstring(int x0, int y0, int cx, int cy, char const *text)
{
    int	index, width, y, n;

    width = cx / CXFONT;
    index = 0;
    for (y = 0 ; y + CYFONT <= cy ; y += CYFONT) {
	while (isspace(text[index]))
	    ++index;
	if (!text[index])
	    break;
	n = strlen(text + index);
	if (n > width) {
	    n = width;
	    while (!isspace(text[index + n]) && n >= 0)
		--n;
	    if (n < 0)
		n = width;
	}
	gl_writen(x0, y0 + y, n, (char*)text + index);
	index += n;
    }
}

int xviewshift = 0, yviewshift = 0;
int displaygame(gamestate const *state, int timeleft, int besttime)
{
    static unsigned char	view[CXVIEW * CYVIEW * CXTILE * CYTILE];
    creature const	       *cr;
    char			buf[32];
    int				xfrom, yfrom, xcenter, ycenter, x, y;
    int				pos;

    memset(view, 0, sizeof view);

    pos = state->creatures->pos;
    xcenter = state->creatures->pos % CXGRID;
    ycenter = state->creatures->pos / CXGRID;
    xcenter += xviewshift;
    ycenter += yviewshift;
    xfrom = xcenter - XMIDDISPLAY;
    yfrom = ycenter - YMIDDISPLAY;
    if (xfrom < 0)			xfrom = 0;
    if (yfrom < 0)			yfrom = 0;
    if (xfrom > CXGRID - CXDISPLAY)	xfrom = CXGRID - CXDISPLAY;
    if (yfrom > CYGRID - CYDISPLAY)	yfrom = CYGRID - CYDISPLAY;
    xfrom += XMIDDISPLAY - XMIDVIEW;
    yfrom += YMIDDISPLAY - YMIDVIEW;

    for (y = 1 ; y < CYVIEW - 1 ; ++y) {
	if (yfrom + y < 0 || yfrom + y >= CXGRID)
	    continue;
	pos = (yfrom + y) * CXGRID + xfrom + 1;
	for (x = 1 ; x < CXVIEW - 1 ; ++x, ++pos) {
	    if (xfrom + x < 0 || xfrom + x >= CXGRID)
		continue;
	    puttile(view, y * 4, x * 4, floortile(state->map[pos].floor));
	}
    }

    for (cr = state->creatures ; cr->id ; ++cr) {
	if (cr->id != Chip && (cr->state & 1))
	    continue;
	x = (cr->pos % CXGRID - xfrom) * 4;
	y = (cr->pos / CXGRID - yfrom) * 4;
	if (cr->moving > 0) {
	    switch (cr->dir) {
	      case NORTH:	y += cr->moving / 2;	break;
	      case WEST:	x += cr->moving / 2;	break;
	      case SOUTH:	y -= cr->moving / 2;	break;
	      case EAST:	x -= cr->moving / 2;	break;
	    }
	}
	if (x >= 0 && y >= 0 && x <= (CXVIEW - 1) * 4 && y <= (CYVIEW - 1) * 4)
	    puttile(view, y, x,
		    entitydirtile(cr->id,
				  cr->dir == (uchar)NIL ? 0 : cr->dir));
    }

    xcenter *= 4;
    ycenter *= 4;
    if (state->creatures->moving > 0) {
	switch (state->creatures->dir) {
	  case NORTH:	ycenter += state->creatures->moving / 2;	break;
	  case WEST:	xcenter += state->creatures->moving / 2;	break;
	  case SOUTH:	ycenter -= state->creatures->moving / 2;	break;
	  case EAST:	xcenter -= state->creatures->moving / 2;	break;
	}
    }
    x = xcenter - (xfrom + XMIDDISPLAY) * 4;
    y = ycenter - (yfrom + YMIDDISPLAY) * 4;
    if (xcenter < XMIDDISPLAY * 4)
	x = (XMIDVIEW - XMIDDISPLAY) * 4;
    if (xcenter > (CXGRID - XMIDDISPLAY - 1) * 4)
	x = (XMIDVIEW - XMIDDISPLAY) * 4;
    if (ycenter < YMIDDISPLAY * 4)
	y = (YMIDVIEW - YMIDDISPLAY) * 4;
    if (ycenter > (CYGRID - YMIDDISPLAY - 1) * 4)
	y = (YMIDVIEW - YMIDDISPLAY) * 4;
    x *= CXTILE / 4;
    y *= CYTILE / 4;


    gl_setcontext(virtscreen);
    gl_clearscreen(0);

    gl_putboxpart(CXMARGIN, CYMARGIN, CXDISPLAY * CXTILE, CYDISPLAY * CYTILE,
		  CXVIEW * CXTILE, CYVIEW * CYTILE, view, x, y);

    gl_setfont(CXFONT, CYFONT, font_9x16);
    gl_setwritemode(WRITEMODE_MASKED | FONT_COMPRESSED);
    gl_setfontcolors(0, 15);

    if (state->soundeffect && *state->soundeffect)
	gl_write(CXMARGIN, CYMARGIN + CYDISPLAY * CYTILE,
		 (char*)state->soundeffect);

    sprintf(buf, "Level %d", state->game->number);
    gl_write(XINFO, YINFO + 0 * CYFONT, buf);
    gl_write(XINFO, YINFO + 1 * CYFONT, (char*)state->game->name);
    gl_write(XINFO, YINFO + 2 * CYFONT, "Password:");
    gl_write(XINFO + 10 * CXFONT, YINFO + 2 * CYFONT,
	     (char*)state->game->passwd);
    if (besttime) {
	if (timeleft < 0)
	    sprintf(buf, "(Best time: %d)", besttime);
	else
	    sprintf(buf, "Best time: %3d", besttime);
	gl_write(XINFO, YINFO + 3 * CYFONT, buf);
    }

    sprintf(buf, "Chips %3d", state->chipsneeded);
    gl_write(XINFO, YINFO + 5 * CYFONT, buf);
    if (timeleft < 0)
	strcpy(buf, "Time  ---");
    else
	sprintf(buf, "Time  %3d", timeleft);
    gl_write(XINFO, YINFO + 6 * CYFONT, buf);

    if (state->displayflags & DF_SHOWHINT) {
	writemlstring(XINFO, YINFO + 9 * CYFONT,
		      CXSCREEN - XINFO, 8 * CYFONT,
		      state->game->hinttext);
    } else {
	for (y = 0 ; y < 4 ; ++y)
	    gl_putbox(XINFO + 1 * CXTILE, YINFO + 9 * CYFONT + y * CYTILE,
		      CXTILE, CYTILE,
		      floortile(state->keys[y] ? Key_Blue + y : Empty));
	for (y = 0 ; y < 4 ; ++y)
	    gl_putbox(XINFO + 2 * CXTILE, YINFO + 9 * CYFONT + y * CYTILE,
		      CXTILE, CYTILE,
		      floortile(state->boots[y] ? Boots_Slide + y : Empty));
    }

    vga_waitretrace();
    gl_copyscreen(realscreen);

    return TRUE;
}

int displayhelp(char const *title, objhelptext const *text, int textcount)
{
    unsigned char	tilebuf[2][CXTILE * CYTILE];
    int			y, n;

    gl_setcontext(virtscreen);
    gl_clearscreen(0);

    gl_setfont(CXFONT, CYFONT, font_9x16);
    gl_setwritemode(WRITEMODE_MASKED | FONT_COMPRESSED);
    gl_setfontcolors(0, 15);

    n = strlen(title);

    gl_write((CXSCREEN - n * CXFONT) / 2, 0, (char*)title);
    y = CYFONT * 2;
    for (n = 0 ; n < textcount ; ++n) {
	memset(tilebuf, 0, sizeof tilebuf);
	if (text[n].isfloor) {
	    copytile(tilebuf[1], floortile(text[n].item1));
	    if (text[n].item2)
		copytile(tilebuf[0], floortile(text[n].item2));
	} else {
	    copytile(tilebuf[1], floortile(Empty));
	    addtile(tilebuf[1], entitydirtile(text[n].item1, EAST));
	    if (text[n].item2) {
		copytile(tilebuf[0], floortile(Empty));
		addtile(tilebuf[0], entitydirtile(text[n].item2, EAST));
	    }
	}
	gl_putbox(0, y, CXTILE, CYTILE, tilebuf[0]);
	gl_putbox(CXTILE, y, CXTILE, CYTILE, tilebuf[1]);
	writemlstring(CXTILE * 2 + 16, y,
		      CXSCREEN - CXTILE * 2 - 16, CXTILE, text[n].desc);
	y += CXTILE;
    }
    gl_write(0, CYSCREEN - CYFONT, "Press any key to continue");

    gl_copyscreen(realscreen);

    return TRUE;
}

int displayendmessage(int completed)
{
    gl_setcontext(virtscreen);

    if (completed > 0) {
	gl_write(CXSCREEN - 16 * CXFONT, CYSCREEN - CYFONT * 2, "Press ENTER");
	gl_write(CXSCREEN - 16 * CXFONT, CYSCREEN - CYFONT, "to continue");
    } else {
	gl_write(CXSCREEN - 16 * CXFONT, CYSCREEN - CYFONT * 2, " \"Bummer\"");
	gl_write(CXSCREEN - 16 * CXFONT, CYSCREEN - CYFONT, "Press ENTER");
    }

    gl_copyscreen(realscreen);

    return TRUE;
}

/*
 *
 */

static void shutdown(void)
{
    if (vga_getcurrentmode() != TEXT) {
	vga_setpalvec(0, 256, origpalette);
	vga_setmode(TEXT);
	putchar('\n');
	fclose(stderr);
    }
}

int outputinitialize(int silenceflag)
{
    int	palette[] = {
	 0,  0,  0,   0,  0, 32,   0, 32,  0,   0, 32, 32,
	32,  0,  0,  32,  0, 32,  32, 32,  0,  48, 48, 48,
	32, 32, 32,   0,  0, 63,   0, 63,  0,   0, 63, 63,
	63,  0,  0,  63,  0, 63,  63, 63,  0,  63, 63, 63
    };

    silence = silenceflag;

    atexit(shutdown);
    vga_safety_fork(shutdown);

    if (vga_init()) {
	fprintf(stderr, "Cannot access virtual console!\n");
	exit(EXIT_FAILURE);
    }
    if (!vga_hasmode(G640x480x256)) {
	fprintf(stderr, "Cannot access 256-color VGA mode!\n");
	exit(EXIT_FAILURE);
    }

    freopen("./err.log", "w", stderr);

    virtscreen = gl_allocatecontext();
    realscreen = gl_allocatecontext();
    gl_setcontextvgavirtual(G640x480x256);
    vga_getpalvec(0, 256, origpalette);
    gl_getcontext(virtscreen);
    vga_setmode(G640x480x256);
    gl_setcontextvga(G640x480x256);
    vga_setpalvec(0, 16, palette);
    gl_getcontext(realscreen);

    initializetiles();

    return TRUE;
}

/*
 * Miscellaneous interface functions
 */

/* Ring the bell.
 */
void ding(void)
{
    if (!silence) {
	putchar('\a');
	fflush(stdout);
    }
}

/* Display an appropriate error message on stderr; use msg if errno
 * is zero.
 */
int fileerr(char const *msg)
{
    fputc('\n', stderr);
    fputs(currentfilename ? currentfilename : programname, stderr);
    fputs(": ", stderr);
    fputs(msg ? msg : errno ? strerror(errno) : "unknown error", stderr);
    fputc('\n', stderr);
    return FALSE;
}

/* Display a formatted message on stderr and exit cleanly.
 */
void die(char const *fmt, ...)
{
    va_list	args;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", programname);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(args);
    shutdown();
    exit(EXIT_FAILURE);
}

void warn(char const *fmt, ...)
{
    va_list	args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(args);
}
