/* logic.h: The game logic
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_logic_h_
#define	_logic_h_

#include	"state.h"

extern int translatemapdata(gamestate *pstate);
extern int advancegame(gamestate *pstate);

#endif
