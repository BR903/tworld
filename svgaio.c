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
#include	"userio.h"

#include	"font_9x16.c"
#include	"cctiles.c"

#define	CXSCREEN	640
#define	CYSCREEN	480

#define	CXFONT		9
#define	CYFONT		16
#define	CXTILE		48
#define	CYTILE		48

#define	CXVIEW		9
#define	CYVIEW		9

#define	XVIEW		16
#define	YVIEW		16
#define	XINFO		(16 + (CXVIEW * CXTILE) + 16)
#define	YINFO		16

#define	NTILES		(Count_Floors + 4 * Count_Entities)

static GraphicsContext *virtscreen = NULL;
static GraphicsContext *realscreen = NULL;

static int silence = FALSE;

char const     *programname = "a.out";
char const     *currentfilename = NULL;

#define	floortile(id)		(cctiles + (id) * CXTILE * CYTILE)
#define	entitytile(id)		(cctiles + (Count_Floors + (id)) * CXTILE * CYTILE)
#define	entitydirtile(id, dir)	\
    (cctiles + (Count_Floors + (id) * 4 + dir) * CXTILE * CYTILE)

#define	copytile(dest, src)	(memcpy((dest), (src), CXTILE * CYTILE))

static void addtile(unsigned char *dest, unsigned char const *src)
{
    int	i;

    for (i = 0 ; i < CXTILE * CYTILE ; ++i)
	if (!(src[i] & 0xF0))
	    dest[i] = src[i];
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

int displaygame(mapcell const *map, int chippos, int chipdir,
		int icchipsleft, int timeleft, int state,
		int levelnum, char const *title, char const *passwd,
		int besttime, char const *hinttext,
		short keys[4], short boots[4], char const *soundeffect,
		int displayflags)
{
    unsigned char	tilebuf[CXTILE * CYTILE];
    char		buf[32];
    int			xfrom, yfrom, pos, x, y;

    x = chippos % CXGRID;
    y = chippos / CXGRID;
    xfrom = x - CXVIEW / 2;
    yfrom = y - CYVIEW / 2;
    if (xfrom < 0)			xfrom = 0;
    if (yfrom < 0)			yfrom = 0;
    if (xfrom > CXGRID - CXVIEW)	xfrom = CXGRID - CXVIEW;
    if (yfrom > CYGRID - CYVIEW)	yfrom = CYGRID - CYVIEW;

    gl_setcontext(virtscreen);
    gl_clearscreen(0);

#if 0
    gl_putboxpart(XVIEW, 0,
		  CXVIEW * CXTILE, YVIEW, (CXVIEW + 3) * CXTILE, YVIEW,
		  horzframe, (xfrom & 3) * CXTILE, 0);
    gl_putboxpart(XVIEW, YVIEW + (CYVIEW * CYTILE),
		  CXVIEW * CXTILE, YVIEW, (CXVIEW + 3) * CXTILE, YVIEW,
		  horzframe, (xfrom & 3) * CXTILE, 0);
    gl_putboxpart(0, YVIEW,
		  XVIEW, CYVIEW * CYTILE, XVIEW, (CYVIEW + 3) * CYTILE,
		  vertframe, 0, (yfrom & 3) * CYTILE);
    gl_putboxpart(XVIEW + (CXVIEW * CYTILE), YVIEW,
		  XVIEW, CYVIEW * CYTILE, XVIEW, (CYVIEW + 3) * CYTILE,
		  vertframe, 0, (yfrom & 3) * CYTILE);
#endif

    for (x = 0 ; x < CXVIEW ; ++x) {
	for (y = 0 ; y < CYVIEW ; ++y) {
	    pos = (yfrom + y) * CXGRID + (xfrom + x);
	    copytile(tilebuf, floortile(map[pos].floor));
	    if (pos == chippos)
		addtile(tilebuf, entitydirtile(Chip, chipdir));
	    if (map[pos].entity)
		addtile(tilebuf, entitytile(map[pos].entity));
	    gl_putbox(XVIEW + x * CXTILE, YVIEW + y * CYTILE,
		      CXTILE, CYTILE, tilebuf);
	}
    }

    gl_setfont(CXFONT, CYFONT, font_9x16);
    gl_setwritemode(WRITEMODE_MASKED | FONT_COMPRESSED);
    gl_setfontcolors(0, vga_white());

    if (soundeffect && *soundeffect)
	gl_write(XVIEW, YVIEW + CYVIEW * CYTILE, (char*)soundeffect);

    sprintf(buf, "Level %d", levelnum);
    gl_write(XINFO, YINFO + 0 * CYFONT, buf);
    gl_write(XINFO, YINFO + 1 * CYFONT, (char*)title);
    gl_write(XINFO, YINFO + 2 * CYFONT, "Password:");
    gl_write(XINFO + 10 * CXFONT, YINFO + 2 * CYFONT, (char*)passwd);
    if (besttime) {
	if (timeleft < 0)
	    sprintf(buf, "(Best time: %d)", besttime);
	else
	    sprintf(buf, "Best time: %3d", besttime);
	gl_write(XINFO, YINFO + 3 * CYFONT, buf);
    }


    sprintf(buf, "Chips %3d", icchipsleft);
    gl_write(XINFO, YINFO + 5 * CYFONT, buf);
    if (timeleft < 0)
	strcpy(buf, "Time  ---");
    else
	sprintf(buf, "Time  %3d", timeleft);
    gl_write(XINFO, YINFO + 6 * CYFONT, buf);

    if (displayflags & DF_SHOWHINT) {
	writemlstring(XINFO, YINFO + 9 * CYFONT,
		      CXSCREEN - XINFO, 8 * CYFONT,
		      hinttext);
    } else {
	for (y = 0 ; y < 4 ; ++y)
	    gl_putbox(XINFO + 1 * CXTILE, YINFO + 9 * CYFONT + y * CYTILE,
		      CXTILE, CYTILE,
		      floortile(keys[y] ? Key_Blue + y : Empty));
	for (y = 0 ; y < 4 ; ++y)
	    gl_putbox(XINFO + 2 * CXTILE, YINFO + 9 * CYFONT + y * CYTILE,
		      CXTILE, CYTILE,
		      floortile(boots[y] ? Boots_Slide + y : Empty));
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
    gl_setfontcolors(0, vga_white());

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

int inputwait(void)
{
    fd_set	in;

    FD_ZERO(&in);
    FD_SET(STDIN_FILENO, &in);
    return select(STDIN_FILENO + 1, &in, NULL, NULL, NULL) > 0;
}

int input(void)
{
    static int	lastkey = 0, penkey = 0;
    int		key;

    if (lastkey) {
	key = lastkey;
	lastkey = penkey;
	penkey = 0;
    } else {
	key = vga_getkey();
	if (key < 0) {
	    key = 'q';
	} else if (key == '\033') {
	    lastkey = vga_getkey();
	    if (lastkey == '[' || lastkey == 'O') {
		penkey = vga_getkey();
		switch (penkey) {
		  case 'A':	key = 'k'; lastkey = penkey = 0;	break;
		  case 'B':	key = 'j'; lastkey = penkey = 0;	break;
		  case 'C':	key = 'l'; lastkey = penkey = 0;	break;
		  case 'D':	key = 'h'; lastkey = penkey = 0;	break;
		}
	    }
	}
    }
    return key;
}

/*
 *
 */

static void shutdown(void)
{
    if (vga_getcurrentmode() != TEXT) {
	vga_setmode(TEXT);
	putchar('\n');
	fflush(stdout);
    }
}

int ioinitialize(int silenceflag)
{
    silence = silenceflag;

    vga_safety_fork(shutdown);
    if (vga_init()) {
	fprintf(stderr, "Cannot access virtual console!\n");
	exit(EXIT_FAILURE);
    }
    atexit(shutdown);

    if (!vga_hasmode(G640x480x16)) {
	fprintf(stderr, "Cannot access standard VGA mode!\n");
	exit(EXIT_FAILURE);
    }

    freopen("./err.log", "a", stderr);

    virtscreen = gl_allocatecontext();
    realscreen = gl_allocatecontext();
    gl_setcontextvgavirtual(G640x480x16);
    gl_getcontext(virtscreen);
    vga_setmode(G640x480x16);
    gl_setcontextvga(G640x480x16);
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

    shutdown();
    va_start(args, fmt);
    fprintf(stderr, "%s: ", programname);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}
