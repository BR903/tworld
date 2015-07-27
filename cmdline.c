/* cmdline.c: a reentrant version of getopt(). Written 2006 by Brian
 * Raiter. This code is in the public domain.
 */

#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<limits.h>
#include	"gen.h"
#include	"fileio.h"
#include	"cmdline.h"

#define	docallback(opt, val) \
	    do { if ((r = callback(opt, val, data)) != 0) return r; } while (0)

/* Initialize the cmdlineinfo state structure.
 */
int readoptions(option const* list, int argc, char **argv,
		int (*callback)(int, char const*, void*), void *data)
{
    char		argstring[] = "--";
    option const       *opt;
    char const	       *val;
    char const	       *p;
    int			stop = 0;
    int			argi, len, r;

    if (!list || !callback)
	return -1;

    for (argi = 1 ; argi < argc ; ++argi)
    {
	/* First, check for "--", which forces all remaining arguments
	 * to be treated as non-options.
	 */
	if (!stop && argv[argi][0] == '-' && argv[argi][1] == '-'
					  && argv[argi][2] == '\0') {
	    stop = 1;
	    continue;
	}

	/* Arguments that do not begin with '-' (or are only "-") are
	 * not options.
	 */
	if (stop || argv[argi][0] != '-' || argv[argi][1] == '\0') {
	    docallback(0, argv[argi]);
	    continue;
	}

	if (argv[argi][1] == '-')
	{
	    /* Arguments that begin with a double-dash are long
	     * options.
	     */
	    p = argv[argi] + 2;
	    val = strchr(p, '=');
	    if (val)
		len = val++ - p;
	    else
		len = strlen(p);

	    /* Is it on the list of valid options? If so, does it
	     * expect a parameter?
	     */
	    for (opt = list ; opt->optval ; ++opt)
		if (opt->name && !strncmp(p, opt->name, len)
			      && !opt->name[len])
		    break;
	    if (!opt->optval) {
		docallback('?', argv[argi]);
	    } else if (!val && opt->arg == 1) {
		docallback(':', argv[argi]);
	    } else if (val && opt->arg == 0) {
		docallback('=', argv[argi]);
	    } else {
		docallback(opt->optval, val);
	    }
	}
	else
	{
	    /* Arguments that begin with a single dash contain one or
	     * more short options. Each character in the argument is
	     * examined in turn, unless a parameter consumes the rest
	     * of the argument (or possibly even the following
	     * argument).
	     */
	    for (p = argv[argi] + 1 ; *p ; ++p) {
		for (opt = list ; opt->optval ; ++opt)
		    if (opt->chname == *p)
			break;
		if (!opt->optval) {
		    argstring[1] = *p;
		    docallback('?', argstring);
		    continue;
		} else if (opt->arg == 0) {
		    docallback(opt->optval, NULL);
		    continue;
		} else if (p[1]) {
		    docallback(opt->optval, p + 1);
		    break;
		} else if (argi + 1 < argc && strcmp(argv[argi + 1], "--")) {
		    ++argi;
		    docallback(opt->optval, argv[argi]);
		    break;
		} else if (opt->arg == 2) {
		    docallback(opt->optval, NULL);
		    continue;
		} else {
		    argstring[1] = *p;
		    docallback(':', argstring);
		    break;
		}
	    }
	}
    }
    return 0;
}

/* Verify that str points to a boolean value (optionally with
 * whitespace) and return the value present, or -1 if str's contents
 * are not recognized.
 */
static int readboolvalue(char const *str)
{
    char	d;

    while (isspace(*str))
	++str;
    if (!*str)
	return -1;
    d = *str++;
    while (isspace(*str))
	++str;
    if (*str)
	return -1;
    if (d == '0' || tolower(d) == 'n')
	return 0;
    else if (d == '1' || tolower(d) == 'y')
	return 1;
    else
	return -1;
}

/* Parse a configuration file.
 */
int readinitfile(option const* list, fileinfo *file,
		 int (*callback)(int, char const*, void*), void *data)
{
    char		buf[256];
    option const       *opt;
    char	       *name, *val, *p;
    int			len, f, r;

    for (;;)
    {
	/* Get a line from the file. If it's empty or it begins with a
	 * hash sign, skip it entirely.
	 */
	len = sizeof buf - 1;
	if (!filegetline(file, buf, &len, NULL))
	    break;
	while (len > 0 && isspace(buf[len - 1]))
	    --len;
	buf[len] = '\0';
	for (p = buf ; isspace(*p) ; ++p) ;
	if (!*p || *p == '#')
	    continue;

	/* Find the end of the option's name and the beginning of the
	 * parameter, if any.
	 */
	for (name = p ; *p && *p != '=' && !isspace(*p) ; ++p) ;
	len = p - name;
	for ( ; *p == '=' || isspace(*p) ; ++p) ;
	val = p;

	/* Is it on the list of valid options? Does it take a
	 * full parameter, or just an optional boolean?
	 */
	for (opt = list ; opt->optval ; ++opt)
	    if (opt->name && !strncmp(name, opt->name, len)
			  && !opt->name[len])
		    break;
	if (!opt->optval) {
	    docallback('?', name);
	} else if (!*val && opt->arg == 1) {
	    docallback(':', name);
	} else if (*val && opt->arg == 0) {
	    f = readboolvalue(val);
	    if (f < 0)
		docallback('=', name);
	    else if (f == 1)
		docallback(opt->optval, NULL);
	} else {
	    docallback(opt->optval, val);
	}
    }
    return 0;
}

/* Get an integer value from a string. Check for all possible error
 * conditions.
 */
int parseint(char const *str, int *value, int defaultvalue)
{
    char *p;
    long n;

    n = strtol(str, &p, 10);
    if (p == str || *p != '\0' || n < INT_MIN || n > INT_MAX) {
	*value = defaultvalue;
	return FALSE;
    } else {
	*value = n;
	return TRUE;
    }
}
