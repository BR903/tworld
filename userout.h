#ifndef	_userout_h_
#define	_userout_h_

#include	"cc.h"

extern char const      *programname;
extern char const      *currentfilename;

extern int outputinitialize(int silenceflag);

extern int displaygame(gamestate const *state, int timeleft, int besttime);

extern int displayendmessage(int completed);

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
