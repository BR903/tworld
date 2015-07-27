/* cmdline.h: a reentrant version of getopt(). Written 2006 by Brian
 * Raiter. This code is in the public domain.
 */

#ifndef	_cmdline_h_
#define	_cmdline_h_

#include	"fileio.h"

/* The information specifying a single cmdline option.
 */
typedef struct option {
    char const *name;		/* the option's long name, or "" if none */
    char	chname;		/* a single-char name, or zero if none */
    int		optval;		/* a unique value representing this option */
    int		arg;		/* 0 = no arg, 1 = arg req'd, 2 = optional */
} option;

/* Parse the given cmdline arguments. list is an array of option
 * structs, each entry specifying a valid option. The last struct in
 * the array must have name set to NULL. argc and argv give the
 * cmdline to parse. callback is the function to call for each option
 * and non-option found on the cmdline. data is a pointer that is
 * passed to each invocation of callback. The return value of callback
 * should be zero to continue processing the cmdline, or any other
 * value to abort. The return value of readoptions() is the value
 * returned from the last callback, or zero if no arguments were
 * found, or -1 if an error occurred.
 *
 * When readoptions() encounters a regular cmdline argument (i.e. a
 * non-option argument), callback() is invoked with opt equal to zero
 * and val pointing to the argument. When an option is found,
 * callback() is invoked with opt equal to the optval field in the
 * option struct corresponding to that option, and val points to the
 * option's paramter, or is NULL if the option does not take a
 * parameter. If readoptions() finds an option that does not appear in
 * the list of valid options, callback() is invoked with opt equal to
 * '?'. If readoptions() encounters an option that is missing its
 * required parameter, callback() is invoked with opt equal to ':'. If
 * readoptions() finds a parameter on a long option that does not
 * admit a parameter, callback() is invoked with opt equal to '='. In
 * each of these cases, val will point to the erroneous option
 * argument.
 */
extern int readoptions(option const* list, int argc, char **argv,
		       int (*callback)(int opt, char const *val, void *data),
		       void *data);

#endif
