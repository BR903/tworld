#include	<stdio.h>
#include	<stdlib.h>
#include	"gen.h"
#include	"objects.h"
#include	"parse.h"

enum {
    map_layer1	= 1,
    map_layer2	= 2,
    title	= 3,
    traps	= 4,
    cloners	= 5,
    passwd	= 6,
    hint	= 7,
    creatures	= 10
};

static int readushrt(FILE *fp)
{
    int	b1, b2;

    if ((b1 = fgetc(fp)) == EOF || (b2 = fgetc(fp)) == EOF)
	return -1;
    return (b1 & 0xFF) | ((b2 & 0xFF) << 8);
}

static void *readbytes(FILE *fp, int size)
{
    void       *buf;

    if (!(buf = malloc(size))) {
	fileerr("insufficient memory");
	return NULL;
    }
    if (fread(buf, size, 1, fp) != 1) {
	fileerr("unexpected EOF");
	return NULL;
    }
    return buf;
}

int readlevelmap(FILE *fp, gamesetup *game)
{
    uchar	data[CXGRID * CYGRID];
    /*uchar	layer2[CXGRID * CYGRID];*/
    int		levelsize, id, size, i;

    if ((levelsize = readushrt(fp)) < 0)
	return FALSE;
    if ((game->number = readushrt(fp)) < 0 || (game->time = readushrt(fp)) < 0
					  || (game->chips = readushrt(fp)) < 0)
	return fileerr("missing data");
    levelsize -= 6;

    while (levelsize > 0) {
	id = fgetc(fp);
	if (id == 1) {
	    (void)fgetc(fp);
	    game->map1size = readushrt(fp);
	    if (!(game->map1 = readbytes(fp, game->map1size)))
		return FALSE;
	    levelsize -= 4 + game->map1size;
	    /*fixmapfield(data, size);*/
	    game->map2size = readushrt(fp);
	    if (!(game->map2 = readbytes(fp, game->map2size)))
		return FALSE;
	    levelsize -= 2 + game->map2size;
	    /*fixmapfield(layer2, size);*/
	    /*combinemaps(game->map, data, layer2);*/
	    (void)readushrt(fp);
	    levelsize -= 2;
	} else {
	    size = (unsigned char)fgetc(fp);
	    if (fread(data, size, 1, fp) != 1)
		return FALSE;
	    levelsize -= 2 + size;
	    switch (id) {
	      case 3:
		memcpy(game->name, data, size);
		game->name[size - 1] = '\0';
		break;
	      case 4:
		game->trapcount = size / 10;
		for (i = 0 ; i < game->trapcount ; ++i) {
		    game->traps[i].from = data[i * 10 + 0]
					+ data[i * 10 + 2] * CXGRID;
		    game->traps[i].to   = data[i * 10 + 4]
					+ data[i * 10 + 6] * CXGRID;

		}
		break;
	      case 5:
		game->clonercount = size / 8;
		for (i = 0 ; i < game->clonercount ; ++i) {
		    game->cloners[i].from = data[i * 8 + 0]
					  + data[i * 8 + 2] * CXGRID;
		    game->cloners[i].to   = data[i * 8 + 4]
					  + data[i * 8 + 6] * CXGRID;
		}
		break;
	      case 6:
		for (i = 0 ; i < size - 1 ; ++i)
		    game->passwd[i] = data[i] ^ 0x99;
		game->passwd[i] = '\0';
		break;
	      case 7:
		memcpy(game->hinttext, data, size);
		game->hinttext[size - 1] = '\0';
		break;
	      case 10:
		game->creaturecount = size / 2;
		for (i = 0 ; i < game->creaturecount ; ++i)
		    game->creatures[i] = data[i * 2 + 0]
				       + data[i * 2 + 1] * CXGRID;
		break;
	      default:
		return fileerr("unrecognized file contents");
	    }
	}
    }
    return TRUE;
}
