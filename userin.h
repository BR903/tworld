#ifndef	_userin_h_
#define	_userin_h_

#include	"commands.h"

extern int inputinitialize(int permitrawkeysflag);
extern int setkeypolling(int polling);

extern int input(int wait);
extern int anykey(void);

#endif
