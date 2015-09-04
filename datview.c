#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>

typedef	unsigned char	_uchar;
typedef	unsigned short	_ushrt;
typedef	unsigned long	_ulong;

#undef uchar
#define	uchar		_uchar
#undef ushrt
#define	ushrt		_ushrt
#undef ulong
#define ulong		_ulong

#define	DEFAULTDIR	"/usr/share/games/tworld/sets/"

#define	readval(var)	(fread(&(var), sizeof(var), 1, fp) == 1)

typedef	struct xy {
    uchar	x;
    uchar	y;
} xy;

typedef	struct xyconn {
    xy		from;
    xy		to;
} xyconn;

typedef	struct ccfilehead {
    ulong	fullsize;
    ushrt	levels;
} ccfilehead;

typedef	struct cclevelhead {
    ushrt	off_next;
    ushrt	levelnum;
    ushrt	timecount;
    ushrt	chipcount;
} cclevelhead;

typedef	struct ccmapcell {
    uchar	floor;
    uchar	movein;
    uchar	moveout;
    uchar	object;
    uchar	orientation;
    xy		machine;
} ccmapcell;

typedef	struct cclevel {
    int		num;
    int		time;
    int		chips;
    ccmapcell	map[32][32];
    uchar	map1[32][32];
    uchar	map2[32][32];
    uchar	name[256];
    uchar	pass[256];
    uchar	hint[256];
    int		creaturecount;
    int		trapcount;
    int		clonercount;
    xy	       *creatures;
    xyconn     *traps;
    xyconn     *cloners;
} cclevel;

typedef	struct field {
    int		type;
    int		size;
    uchar      *data;
} field;

static char const *objects[] = {
/* 00 empty space		*/	" ",
/* 01 wall			*/	"\033[7m \033[0m",
/* 02 chip			*/	"#",
/* 03 water			*/	"\033[36;44m=\033[0m",
/* 04 fire			*/	"\033[31mx\033[0m",
/* 05 invisible wall, perm.	*/	"\033[7mx\033[0m",
/* 06 blocked north		*/	"\342\226\224",
/* 07 blocked west		*/	"\342\226\217",
/* 08 blocked south		*/	"\342\226\201",
/* 09 blocked east		*/	"\342\226\225",
/* 0A block			*/	"O",
/* 0B dirt			*/	"\033[33m\033(0a\033(B\033[0m",
/* 0C ice			*/	"\033[36m\033(0a\033(B\033[0m",
/* 0D force south		*/	"\033[32m\342\207\243\033[0m",
/* 0E cloning block north	*/	"O",
/* 0F cloning block west	*/	"O",
/* 10 cloning block south	*/	"O",
/* 11 cloning block east	*/	"O",
/* 12 force north		*/	"\033[32m\342\207\241\033[0m",
/* 13 force east		*/	"\033[32m\342\207\242\033[0m",
/* 14 force west		*/	"\033[32m\342\207\240\033[0m",
/* 15 exit			*/	"\033[34;46m\342\226\253\033[0m",
/* 16 blue door			*/	"\033[36m0\033[0m",
/* 17 red door			*/	"\033[31m0\033[0m",
/* 18 green door		*/	"\033[32m0\033[0m",
/* 19 yellow door		*/	"\033[1;33m0\033[0m",
/* 1A SE ice slide		*/	"\033[36m\033(0l\033(B\033[0m",
/* 1B SW ice slide		*/	"\033[36m\033(0k\033(B\033[0m",
/* 1C NE ice slide		*/	"\033[36m\033(0j\033(B\033[0m",
/* 1D NW ice slide		*/	"\033[36m\033(0m\033(B\033[0m",
/* 1E blue block, tile		*/	"\033[34m\033(0a\033(B\033[0m",
/* 1F blue block, wall		*/	"\033[44m \033[0m",
/* 20 not used			*/	"\033[1;31m?\033[0m",
/* 21 thief			*/	"T",
/* 22 socket			*/	"H",
/* 23 green button		*/	"\033[32m.\033[0m",
/* 24 red button		*/	"\033[31m.\033[0m",
/* 25 switch block, closed	*/	"\033[7;32m\342\254\232\033[0m",
/* 26 switch block, open	*/	"\033[32m\342\254\232\033[0m",
/* 27 brown button		*/	"\033[33m.\033[0m",
/* 28 blue button		*/	"\033[1;34m.\033[0m",
/* 29 teleport			*/	"\033[36m*\033[0m",
/* 2A bomb			*/	"\033[1;31m\303\262\033[0m",
/* 2B trap			*/	"\033[31m*\033[0m",
/* 2C invisible wall, temp.	*/	"\033[7mx\033[0m",
/* 2D gravel			*/	"\033(0a\033(B",
/* 2E pass once			*/	"*",
/* 2F hint			*/	"?",
/* 30 blocked SE		*/	"\033(0j\033(B",
/* 31 cloning machine		*/	"C",
/* 32 force all directions	*/	"\033[32m+\033[0m",
/* 33 drowning Chip		*/	"\033[36;44m*\033[0m",
/* 34 burned Chip		*/	"\033[1;31m\302\251\033[0m",
/* 35 burned Chip		*/	"\033[31m\302\251\033[0m",
/* 36 not used			*/	"\033[1;31m?\033[0m",
/* 37 not used			*/	"\033[1;31m?\033[0m",
/* 38 not used			*/	"\033[36mO\033[0m",
/* 39 Chip in exit		*/	"\033[1;37;44m\302\251\033[0m",
/* 3A exit - end game		*/	"\033[1;37;44m@\033[0m",
/* 3B exit - end game		*/	"\033[1;37;44m@\033[0m",
/* 3C Chip swimming north	*/	"\033[36;44m\302\251\033[0m",
/* 3D Chip swimming west	*/	"\033[36;44m\302\251\033[0m",
/* 3E Chip swimming south	*/	"\033[36;44m\302\251\033[0m",
/* 3F Chip swimming east	*/	"\033[36;44m\302\251\033[0m",
/* 40 Bug N			*/	"\033[35mB\033[0m",
/* 41 Bug W			*/	"\033[35mB\033[0m",
/* 42 Bug S			*/	"\033[35mB\033[0m",
/* 43 Bug E			*/	"\033[35mB\033[0m",
/* 44 Fireball N		*/	"\033[35mF\033[0m",
/* 45 Fireball W		*/	"\033[35mF\033[0m",
/* 46 Fireball S		*/	"\033[35mF\033[0m",
/* 47 Fireball E		*/	"\033[35mF\033[0m",
/* 48 Pink ball N		*/	"\033[35mo\033[0m",
/* 49 Pink ball W		*/	"\033[35mo\033[0m",
/* 4A Pink ball S		*/	"\033[35mo\033[0m",
/* 4B Pink ball E		*/	"\033[35mo\033[0m",
/* 4C Tank N			*/	"\033[35mH\033[0m",
/* 4D Tank W			*/	"\033[35mH\033[0m",
/* 4E Tank S			*/	"\033[35mH\033[0m",
/* 4F Tank E			*/	"\033[35mH\033[0m",
/* 50 Glider N			*/	"\033[35mG\033[0m",
/* 51 Glider W			*/	"\033[35mG\033[0m",
/* 52 Glider S			*/	"\033[35mG\033[0m",
/* 53 Glider E			*/	"\033[35mG\033[0m",
/* 54 Teeth N			*/	"\033[35mT\033[0m",
/* 55 Teeth W			*/	"\033[35mT\033[0m",
/* 56 Teeth S			*/	"\033[35mT\033[0m",
/* 57 Teeth E			*/	"\033[35mT\033[0m",
/* 58 Walker N			*/	"\033[35mX\033[0m",
/* 59 Walker W			*/	"\033[35mX\033[0m",
/* 5A Walker S			*/	"\033[35mX\033[0m",
/* 5B Walker E			*/	"\033[35mX\033[0m",
/* 5C Blob N			*/	"\033[35mb\033[0m",
/* 5D Blob W			*/	"\033[35mb\033[0m",
/* 5E Blob S			*/	"\033[35mb\033[0m",
/* 5F Blob E			*/	"\033[35mb\033[0m",
/* 60 Paramecium N		*/	"\033[35mP\033[0m",
/* 61 Paramecium W		*/	"\033[35mP\033[0m",
/* 62 Paramecium S		*/	"\033[35mP\033[0m",
/* 63 Paramecium E		*/	"\033[35mP\033[0m",
/* 64 Blue key			*/	"\033[36m\302\245\033[0m",
/* 65 Red key			*/	"\033[31m\302\245\033[0m",
/* 66 Green key			*/	"\033[32m\302\245\033[0m",
/* 67 Yellow key		*/	"\033[1;33m\302\245\033[0m",
/* 68 Flippers			*/	"\033[34m\302\244\033[0m",
/* 69 Fire boots		*/	"\033[31m\302\244\033[0m",
/* 6A Ice skates		*/	"\033[36m\302\244\033[0m",
/* 6B Suction boots		*/	"\033[32m\302\244\033[0m",
/* 6C Chip N			*/	"\302\251",
/* 6D Chip W			*/	"\302\251",
/* 6E Chip S			*/	"\302\251",
/* 6F Chip E			*/	"\302\251",
/* 70 Hiding space		*/	"]",
/* 71 Working trap		*/	"\033[33m*\033[0m",
};

/*
 * bug       = bug		hug left
 * fire bug  = fireball		turn right
 * ball      = ball		turn back
 * tank      = tank		turn back on blue button
 * ghost     = glider		turn left
 * frog      = teeth		follow
 * dumbbell  = walker		turn random
 * blob      = blob		move random
 * centipede = paramecium	hug right
 */

static void printwrap(unsigned char const *str, int x0)
{
    char const *p;
    int		i, n, x;

    p = (char const*)str;
    n = strlen(p);
    x = x0;
    while (n > 78 - x) {
	for (i = 78 - x ; i >= 0 && p[i] != ' ' ; --i) ;
	i = i ? i + 1 : 79 - x;
	printf("%*.*s\n", i, i, p);
	p += i;
	n -= i;
	x = 0;
    }
    if (*p)
	fputs(p, stdout);
}

static int readlevel(FILE *fp, cclevelhead *lhead, field fields[16])
{
    int		n;
    ushrt	fieldnum, off;

    memset(fields, 0, sizeof(field[16]));

    if (!readval(lhead->off_next))
	return -1;
    n = lhead->off_next;
    if (!readval(lhead->levelnum) || !readval(lhead->timecount)
				  || !readval(lhead->chipcount))
	return 1;
    n -= sizeof lhead->levelnum + sizeof lhead->timecount
				+ sizeof lhead->chipcount;

    if (!readval(fieldnum) || !readval(off))
	return 2;
    if (fieldnum != 0 && fieldnum != 1)
	return 3;
    fields[1].type = 1;
    fields[1].size = off;
    fields[1].data = malloc(off);
    if (fread(fields[1].data, off, 1, fp) != 1)
	return 4;
    n -= sizeof fieldnum + sizeof off + off;
    if (!readval(off))
	return 5;
    fields[2].type = 2;
    fields[2].size = off;
    fields[2].data = malloc(off);
    if (fread(fields[2].data, off, 1, fp) != 1)
	return 6;
    n -= sizeof off + off;

    if (!readval(off))
	return 7;
    n -= sizeof off;
    if (off != n)
	return 8;

    while (n > 0) {
	fieldnum = (unsigned char)fgetc(fp);
	off = (unsigned char)fgetc(fp);
	fields[fieldnum].type = fieldnum;
	fields[fieldnum].size = off;
	fields[fieldnum].data = malloc(off);
	if (fread(fields[fieldnum].data, off, 1, fp) != 1)
	    return 9;
	n -= 2 + off;
    }
    if (n < 0)
	return 10;

    return 0;
}

static void fixmapfield(field *f)
{
    uchar      *map;
    int		i, j;

    map = malloc(32 * 32);
    for (i = j = 0 ; i < f->size ; ++i) {
	if (j == 32 * 32) {
	    printf("warning: RLE encoding overrun: bailing early.\n");
	    break;
	}
	if (f->data[i] == 0xFF) {
	    if (j + f->data[i + 1] > 32 * 32) {
		printf("warning: RLE encoding overrun: (%02X %02X %02X)"
		       " extends %d bytes past end.\n",
		       f->data[i], f->data[i + 1], f->data[i + 2],
		       j + f->data[i + 1] - 32 * 32);
		f->data[i + 1] = 32 * 32 - j;
	    }
	    memset(map + j, f->data[i + 2], f->data[i + 1]);
	    j += f->data[i + 1];
	    i += 2;
	} else {
	    map[j++] = f->data[i];
	}
    }
    if (j != 32 * 32) {
	printf("RLE decoding failed.\n");
	exit(1);
    }
    free(f->data);
    f->data = map;
    f->size = 32 * 32;
}

static void fixpassword(field *f)
{
    uchar      *p;

    for (p = f->data ; *p ; ++p)
	*p ^= 0x99;
}

#if 0
static int isobject(uchar item)
{
    return item == 0x0A || (item >= 0x0E && item <= 0x11)
			|| (item >= 0x3C && item <= 0x63)
			|| (item >= 0x6C && item <= 0x6F);
}
#endif

static void makelevelmap(ccmapcell map[32][32],
			 uchar const map1[32][32], uchar const map2[32][32])
{
    int	x, y;

    for (y = 0 ; y < 32 ; ++y) {
	for (x = 0 ; x < 32 ; ++x) {
#if 1
	    map[x][y].floor = map1[x][y];
	    map[x][y].object = map2[x][y];
#else
	    if (map2[x][y]) {
		map[x][y].floor = map2[x][y];
		map[x][y].object = map1[x][y];
		if (map2[x][y] && !map1[x][y])
		    map[x][y].object = 0x70;
	    } else if (isobject(map1[x][y])) {
		map[x][y].floor = 0;
		map[x][y].object = map1[x][y];
	    } else {
		map[x][y].floor = map1[x][y];
		map[x][y].object = 0;
	    }
#endif
	}
    }
}

static cclevel makelevel(cclevelhead lhead, field fields[16])
{
    cclevel	level;
    ccmapcell  *cell;
    int		i;

    level.num = lhead.levelnum;
    level.time = lhead.timecount;
    level.chips = lhead.chipcount;
    fixmapfield(&fields[1]);
    fixmapfield(&fields[2]);
    makelevelmap(level.map, (uchar const(*)[32])fields[1].data,
			    (uchar const(*)[32])fields[2].data);
    free(fields[1].data);
    memset(fields + 1, 0, sizeof fields[1]);
    free(fields[2].data);
    memset(fields + 2, 0, sizeof fields[2]);
    memcpy(level.name, fields[3].data, fields[3].size);
    free(fields[3].data);
    memset(fields + 3, 0, sizeof fields[3]);
    fixpassword(&fields[6]);
    memcpy(level.pass, fields[6].data, fields[6].size);
    free(fields[6].data);
    memset(fields + 6, 0, sizeof fields[6]);
    if (fields[7].type) {
	memcpy(level.hint, fields[7].data, fields[7].size);
	free(fields[7].data);
	memset(fields + 7, 0, sizeof fields[7]);
    } else
	*level.hint = '\0';

    if (fields[4].type) {
	level.trapcount = fields[4].size / 10;
	level.traps = malloc(level.trapcount * sizeof *level.traps);
	for (i = 0 ; i < level.trapcount ; ++i) {
	    level.traps[i].from.x = fields[4].data[i * 10 + 0];
	    level.traps[i].from.y = fields[4].data[i * 10 + 2];
	    level.traps[i].to.x   = fields[4].data[i * 10 + 4];
	    level.traps[i].to.y   = fields[4].data[i * 10 + 6];
	    cell = &level.map[level.traps[i].to.y][level.traps[i].to.x];
	    if (cell->floor == 0x2B)
		cell->floor = 0x71;
	    if (cell->object == 0x2B)
		cell->object = 0x71;
	}
	free(fields[4].data);
	memset(fields + 4, 0, sizeof fields[4]);
    } else {
	level.trapcount = 0;
	level.traps = NULL;
    }
    if (fields[5].type) {
	level.clonercount = fields[5].size / 8;
	level.cloners = malloc(level.clonercount * sizeof *level.cloners);
	for (i = 0 ; i < level.clonercount ; ++i) {
	    level.cloners[i].from.x = fields[5].data[i * 8 + 0];
	    level.cloners[i].from.y = fields[5].data[i * 8 + 2];
	    level.cloners[i].to.x   = fields[5].data[i * 8 + 4];
	    level.cloners[i].to.y   = fields[5].data[i * 8 + 6];
	}
	free(fields[5].data);
	memset(fields + 5, 0, sizeof fields[5]);
    } else {
	level.clonercount = 0;
	level.cloners = NULL;
    }
    if (fields[10].type) {
	level.creaturecount = fields[10].size / 2;
	level.creatures = malloc(level.creaturecount *
					sizeof *level.creatures);
	for (i = 0 ; i < level.creaturecount ; ++i) {
	    level.creatures[i].x = fields[10].data[i * 2 + 0];
	    level.creatures[i].y = fields[10].data[i * 2 + 1];
	}
	free(fields[10].data);
	memset(fields + 10, 0, sizeof fields[10]);
    } else {
	level.creaturecount = 0;
	level.creatures = NULL;
    }
    return level;
}

static void freelevel(cclevel level)
{
    if (level.creatures)
	free(level.creatures);
    if (level.traps)
	free(level.traps);
    if (level.cloners)
	free(level.cloners);
}

#if 0
static void outputlevel(cclevel level)
{
    static char const *objstr[] = {
	"0", "WALL", "ICCHIP", "WATER", "FIRE", "INVW_P",
	"WALLN", "WALLW", "WALLS", "WALLE", "BLOCK", "DIRT", "ICE",
	"SLIDES", "BLOK_N", "BLOK_W", "BLOK_S", "BLOK_E",
	"SLIDEN", "SLIDEE", "SLIDEW", "EXIT",
	"BL_DOR", "RD_DOR", "GR_DOR", "YL_DOR",
	"ICEWSE", "ICEWSW", "ICEWNW", "ICEWNE", "BBFAKE", "BBWALL", "-0x20",
	"THIEF", "SOCKET", "GR_BTN", "RD_BTN", "SWB_CL", "SWB_OP",
	"BN_BTN", "BL_BTN", "TELEPT", "BOMB", "TRAP", "INVW_T", "GRAVEL",
	"POPUPW", "HINT", "WALLSE", "CLONER", "SLDALL",
	"-0x33", "-0x34", "-0x35", "-0x36", "-0x37", "-0x38", "-0x39",
	"EXIT", "EXIT", "-0x3C", "-0x3D", "-0x3E", "-0x3F",
	"BUG_N", "BUG_W", "BUG_S", "BUG_E",
	"FRBL_N", "FRBL_W", "FRBL_S", "FRBL_E",
	"BALL_N", "BALL_W", "BALL_S", "BALL_E",
	"TANK_N", "TANK_W", "TANK_S", "TANK_E",
	"GLDR_N", "GLDR_W", "GLDR_S", "GLDR_E",
	"TETH_N", "TETH_W", "TETH_S", "TETH_E",
	"WLKR_N", "WLKR_W", "WLKR_S", "WLKR_E",
	"BLOB_N", "BLOB_W", "BLOB_S", "BLOB_E",
	"PARA_N", "PARA_W", "PARA_S", "PARA_E",
	"BL_KEY", "RD_KEY", "GR_KEY", "YL_KEY",
	"WT_BTS", "FR_BTS", "IC_BTS", "SL_BTS",
	"CHIP_N", "CHIP_W", "CHIP_S", "CHIP_E"
    };

    FILE       *fp;
    char	buf[256];
    int		i, y, x;

    sprintf(buf, "levels/%03d", level.num);
    if (!(fp = fopen(buf, "w"))) {
	perror(buf);
	return;
    }
    fprintf(fp, "$title = \"%s\";\n", level.name);
    fprintf(fp, "$passwd = \"%s\";\n", level.pass);
    fprintf(fp, "$hinttext = \"%s\";\n", level.hint);
    fprintf(fp, "$chipsneeded = %d;\n", level.chips);
    fprintf(fp, "$leveltime = %d;\n", level.time);

    fprintf(fp, "@map = (\n");
    for (y = 0 ; y < 32 ; ++y) {
	fprintf(fp, "  [\n");
	for (x = 0 ; x < 32 ; ++x) {
	    fprintf(fp, "    { floor => %s, creature => %s },\n",
			objstr[level.map[y][x].floor],
			objstr[level.map[y][x].object]);
	}
	fprintf(fp, "  ],\n");
    }
    fprintf(fp, ");\n");

    fprintf(fp, "undef @traps;\n");
    for (i = 0 ; i < level.trapcount ; ++i)
	fprintf(fp, "push @traps, [ %d, %d, %d, %d ];\n",
		    level.traps[i].from.y, level.traps[i].from.x,
		    level.traps[i].to.y, level.traps[i].to.x);
    fprintf(fp, "undef @cloners;\n");
    for (i = 0 ; i < level.clonercount ; ++i)
	fprintf(fp, "push @cloners, [ %d, %d, %d, %d ];\n",
		    level.cloners[i].from.y, level.cloners[i].from.x,
		    level.cloners[i].to.y, level.cloners[i].to.x);
    fprintf(fp, "@creatures = ( ");
    for (i = 0 ; i < level.creaturecount ; ++i)
	fprintf(fp, "[ %d, %d ], ",
		    level.creatures[i].y, level.creatures[i].x);
    fprintf(fp, ");\n");

}
#endif

int main(int argc, char *argv[])
{
    FILE       *fp;
    cclevel	level;
    cclevelhead	lhead;
    field	fields[16];
    ushrt	levelcount;
    int		floor, object;
    int		from = 0, to = 0;
    int		i, x, y;

    if (argc < 2 || argc > 4) {
	fprintf(stderr, "Usage: datview DATFILE [FIRSTLEVEL [LASTLEVEL]]\n");
	return EXIT_FAILURE;
    } else if (argc < 3) {
	from = 0;
	to = 65535;
    } else if (argc == 3) {
	from = atoi(argv[2]);
	to = from;
    } else if (argc > 3) {
	from = atoi(argv[2]);
	to = atoi(argv[3]);
    }

    if (!strchr(argv[1], '/'))
	if (chdir(DEFAULTDIR))
	    fprintf(stderr, "warn: data directory missing: %s\n", DEFAULTDIR);
    fp = fopen(argv[1], "r");
    if (!fp)
	return 1;

    if (fread(&i, sizeof i, 1, fp) != 1)
	return 1;
    if (fread(&levelcount, sizeof levelcount, 1, fp) != 1)
	return 1;
    if (i == 0x0102AAAC)
	printf("using the Lynx ruleset          \t");
    else if (i == 0x0002AAAC)
	printf("using the MS ruleset            \t");
    else
	printf("unrecognized signature: %08X\t", i);
    printf("last level: %u\n", levelcount);

    while (!(i = readlevel(fp, &lhead, fields))) {
	level = makelevel(lhead, fields);
	x = printf("%3u  time ", level.num);
	if (level.time)
	    x += printf("%03u", level.time);
	else
	    x += printf("---");
	x += printf("  chips %03u  pass %s", level.chips, level.pass);
	x += printf(" (%u) ", lhead.off_next);
	printwrap(level.name, x);
	printf("\n");

	if (level.num >= from && level.num <= to) {
	    for (y = 0 ; y < 32 ; ++y) {
		for (x = 0 ; x < 32 ; ++x) {
		    floor = level.map[y][x].floor;
		    object = level.map[y][x].object;
		    if (object == 0) {
			fputs(objects[floor], stdout);
			fputs(" ", stdout);
		    } else {
			fputs(objects[object], stdout);
			fputs(objects[floor], stdout);
		    }
		}
		if (y < level.trapcount)
		    printf(" %02d %02d -> %02d %02d",
			   level.traps[y].from.x, level.traps[y].from.y,
			   level.traps[y].to.x, level.traps[y].to.y);
		else if (y < level.trapcount + level.clonercount)
		    printf(" %02d %02d => %02d %02d",
			   level.cloners[y - level.trapcount].from.x,
			   level.cloners[y - level.trapcount].from.y,
			   level.cloners[y - level.trapcount].to.x,
			   level.cloners[y - level.trapcount].to.y);
		else if (y == level.trapcount + level.clonercount
						&& level.creaturecount)
		    printf(" %d creature%s", level.creaturecount,
					level.creaturecount == 1 ? "" : "s");
		printf("\n");
	    }
	}
	if (*level.hint) {
	    printf("hint: ");
	    printwrap(level.hint, 6);
	    printf("\n");
	}

	for (i = 0 ; i < (int)(sizeof fields / sizeof *fields) ; ++i)
	    if (fields[i].type || fields[i].size)
		printf("*** Missed field %d on level %u.\n", i, level.num);
#if 0
	outputlevel(level);
#endif
	freelevel(level);
    }

    if (i > 0)
	printf("Bailing out due to error %d.\n", i);

    return 0;
}


#if 0
 196    FF A4 00   FF 0F 01   FF 11 00
   6    01  00  0A  03  02  01
 229    FF E5 00
   5    01  00  15  00  01
  17    FF 11 00
   6    01  00  0A  01  01  01 
   4    FF 04 03
   5    01  00  22  00  01
  17    FF 11 00
  14    01  00  0A  03  02  22  00  04  03  01  01  03  01  01
  18    FF 0E 00   FF 04 01
   5    00  0A  01  01  01
   4    FF 04 00
   5    01  00  00  00  01
  14    FF 0E 00
   8    01  6E  00  2F  00  0A  00  00
   6    FF 06 0C
   4    00  00  00  01
  18    FF 0E 00   FF 04 01
   5    00  0A  01  01  01 
   4    FF 04 00
   5    01  00  00  00  01
  17    FF 11 00
   6    01  00  0A  03  02  01 
   4    FF 04 03
   5    01  01  03  01  01
  17    FF 11 00
   6    01  00  0A  01  01  01
   4    FF 04 03
   5    01  00  22  00  01
  17    FF 11 00
   6    01  00  0A  03  02  01
   4    FF 04 03
   5    01  00  15  00  01
 557    FF 11 00   FF 0F 01   FF FF 00   FF FF 00   FF 0F 00
1248
#endif
