#ifndef	_userio_h_
#define	_userio_h_

#include	"cc.h"

extern char const      *programname;
extern char const      *currentfilename;

extern int ioinitialize(int silenceflag);

extern int displaygame(mapcell const *map, int chippos, int chipdir,
		       int icchipsleft, int timeleft, int state,
		       int levelnum, char const *title, char const *passwd,
		       int besttime, char const *hinttext,
		       short keys[4], short boots[4], char const *soundeffect,
		       int displayflags);

extern int displayendmessage(int completed);

extern int input(void);
extern int inputwait(void);

extern void ding(void);
extern int fileerr(char const *msg);
extern void die(char const *fmt, ...);

typedef	struct objhelptext {
    int		isfloor;
    int		item1;
    int		item2;
    char const *desc;
} objhelptext;

extern int displayhelp(char const *title,
		       objhelptext const *text, int textcount);

#endif
