/* err.h: Error handling and reporting.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_err_h_
#define	_err_h_

/* Quick macros for dealing with memory allocation.
 */
#define	memerrexit()	(die("out of memory"))
#define	xalloc(p, n)	(((p) = realloc((p), (n))) || (memerrexit(), NULL))

/* Exit with an error message.
 */
extern void _die(char const *fmt, ...);

/* Log an error message and continue.
 */
extern void _warn(char const *fmt, ...);

/* Display a simple two-part error message.
 */
extern void _errmsg(char const *prefix, char const *fmt, ...);

/* A really ugly hack used to smuggle extra arguments into variadic
 * functions.
 */
extern char const      *_err_cfile;
extern unsigned long	_err_lineno;
#define	die	(_err_cfile = __FILE__, _err_lineno = __LINE__, _die)
#define	warn	(_err_cfile = __FILE__, _err_lineno = __LINE__, _warn)
#define	errmsg	(_err_cfile = __FILE__, _err_lineno = __LINE__, _errmsg)

#endif
