/* fileio.c: Simple file/directory access functions with error-handling.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<dirent.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	"err.h"
#include	"fileio.h"

/* Windows has some minor differences from Unix when it comes to
 * directories.
 */
#ifdef WIN32
#define	DIRSEP_CHAR	'\\'
#define	createdir(name)	(mkdir(name) == 0)
#else
#define	DIRSEP_CHAR	'/'
#define	createdir(name)	(mkdir(name, 0755) == 0)
#endif

/* Determine a compile-time number to use as the maximum length of a
 * path. Use a value of 1023 if we can't get anything usable from the
 * header files.
 */
#include <limits.h>
#if !defined(PATH_MAX) || PATH_MAX <= 0
#  if defined(MAXPATHLEN) && MAXPATHLEN > 0
#    define PATH_MAX MAXPATHLEN
#  else
#    include <sys/param.h>
#    if !defined(PATH_MAX) || PATH_MAX <= 0
#      if defined(MAXPATHLEN) && MAXPATHLEN > 0
#        define PATH_MAX MAXPATHLEN
#      else
#        define PATH_MAX 1023
#      endif
#    endif
#  endif
#endif

/* The function used to display error messages relating to file I/O.
 */
int _fileerr(char const *cfile, unsigned long lineno,
	     fileinfo *file, char const *msg)
{
    if (msg) {
	_err_cfile = cfile;
	_err_lineno = lineno;
	_errmsg(file->name ? file->name : "file error",
		errno ? strerror(errno) : msg);
    }
    return FALSE;
}

/*
 * File-handling functions.
 */

/* Clear the fields of the fileinfo struct.
 */
void clearfileinfo(fileinfo *file)
{
    file->name = NULL;
    file->fp = NULL;
    file->alloc = FALSE;
}

/* Open a file. If the fileinfo structure does not already have a
 * filename assigned to it, make an independent copy of name.
 */
int fileopen(fileinfo *file, char const *name, char const *mode,
	     char const *msg)
{
    int	n;

    if (!file->name) {
	n = strlen(name) + 1;
	if ((file->name = malloc(n))) {
	    memcpy(file->name, name, n);
	    file->alloc = TRUE;
	} else {
	    file->name = (char*)name;
	    file->alloc = FALSE;
	}
    }
    file->fp = fopen(name, mode);
    if (file->fp)
	return TRUE;
    return fileerr(file, msg);
}

/* Close the file, clear the file pointer, and free the name buffer.
 */
void fileclose(fileinfo *file, char const *msg)
{
    if (!fclose(file->fp))
	fileerr(file, msg);
    file->fp = NULL;
    if (file->alloc) {
	free(file->name);
	file->name = NULL;
    }
}

/* fgetpos().
 */
int filegetpos(fileinfo *file, fpos_t *pos, char const *msg)
{
    if (!fgetpos(file->fp, pos))
	return TRUE;
    return fileerr(file, msg);
}

/* fsetpos().
 */
int filesetpos(fileinfo *file, fpos_t *pos, char const *msg)
{
    if (!fsetpos(file->fp, pos))
	return TRUE;
    return fileerr(file, msg);
}

/* rewind().
 */
int filerewind(fileinfo *file, char const *msg)
{
    (void)msg;
    rewind(file->fp);
    return TRUE;
}

/* feof().
 */
int filetestend(fileinfo *file)
{
    int	ch;

    if (feof(file->fp))
	return TRUE;
    ch = fgetc(file->fp);
    if (ch == EOF)
	return TRUE;
    ungetc(ch, file->fp);
    return FALSE;
}

/* read().
 */
int fileread(fileinfo *file, void *data, unsigned long size, char const *msg)
{
    if (fread(data, size, 1, file->fp) == 1)
	return TRUE;
    return fileerr(file, msg);
}

/* Read size bytes from the given file into a newly allocated buffer.
 */
void *filereadbuf(fileinfo *file, unsigned long size, char const *msg)
{
    void       *buf;

    if (!(buf = malloc(size))) {
	fileerr(file, msg);
	return NULL;
    }
    if (fread(buf, size, 1, file->fp) != 1) {
	fileerr(file, msg);
	free(buf);
	return NULL;
    }
    return buf;
}

/* write().
 */
int filewrite(fileinfo *file, void const *data, unsigned long size,
	      char const *msg)
{
    if (fwrite(data, size, 1, file->fp) == 1)
	return TRUE;
    return fileerr(file, msg);
}

/* Read one byte as an unsigned integer value.
 */
int filereadint8(fileinfo *file, unsigned char *val8, char const *msg)
{
    int	byte;

    if ((byte = fgetc(file->fp)) == EOF)
	return fileerr(file, msg);
    *val8 = (unsigned char)byte;
    return TRUE;
}

/* Write one byte as an unsigned integer value.
 */
int filewriteint8(fileinfo *file, unsigned char val8, char const *msg)
{
    if (fputc(val8, file->fp) != EOF)
	return TRUE;
    return fileerr(file, msg);
}

/* Read two bytes as an unsigned integer value stored in little-endian.
 */
int filereadint16(fileinfo *file, unsigned short *val16, char const *msg)
{
    int	byte;

    if ((byte = fgetc(file->fp)) != EOF) {
	*val16 = (unsigned char)byte;
	if ((byte = fgetc(file->fp)) != EOF) {
	    *val16 |= (unsigned char)byte << 8;
	    return TRUE;
	}
    }
    return fileerr(file, msg);
}

/* Write two bytes as an unsigned integer value in little-endian.
 */
int filewriteint16(fileinfo *file, unsigned short val16, char const *msg)
{
    if (fputc(val16 & 0xFF, file->fp) != EOF
			&& fputc((val16 >> 8) & 0xFF, file->fp) != EOF)
	return TRUE;
    return fileerr(file, msg);
}

/* Read four bytes as an unsigned integer value stored in little-endian.
 */
int filereadint32(fileinfo *file, unsigned long *val32, char const *msg)
{
    int	byte;

    if ((byte = fgetc(file->fp)) != EOF) {
	*val32 = (unsigned char)byte;
	if ((byte = fgetc(file->fp)) != EOF) {
	    *val32 |= (unsigned char)byte << 8;
	    if ((byte = fgetc(file->fp)) != EOF) {
		*val32 |= (unsigned char)byte << 16;
		if ((byte = fgetc(file->fp)) != EOF) {
		    *val32 |= (unsigned char)byte << 24;
		    return TRUE;
		}
	    }
	}
    }
    return fileerr(file, msg);
}

/* Write four bytes as an unsigned integer value in little-endian.
 */
int filewriteint32(fileinfo *file, unsigned long val32, char const *msg)
{
    if (fputc(val32 & 0xFF, file->fp) != EOF
			&& fputc((val32 >> 8) & 0xFF, file->fp) != EOF
			&& fputc((val32 >> 16) & 0xFF, file->fp) != EOF
			&& fputc((val32 >> 24) & 0xFF, file->fp) != EOF)
	return TRUE;
    return fileerr(file, msg);
}

/*
 * Directory-handling functions.
 */

/* Return the size of a buffer big enough to hold a pathname.
 */
int getpathbufferlen(void)
{
    return PATH_MAX;
}

/* Return a buffer big enough to hold a pathname.
 */
char *getpathbuffer(void)
{
    char       *buf;

    if (!(buf = malloc(PATH_MAX + 1)))
	memerrexit();
    return buf;
}

/* Return TRUE if name contains a path but is not a directory itself.
 */
int haspathname(char const *name)
{
    struct stat	st;

    if (!strchr(name, DIRSEP_CHAR))
	return FALSE;
    if (stat(name, &st) || S_ISDIR(st.st_mode))
	return FALSE;
    return TRUE;
}

/* Append the path and/or file contained in path to dir. If path is
 * an absolute path, the original contents of dir are discarded.
 */
int combinepath(char *dir, char const *path)
{
    int	m, n;

    if (path[0] == DIRSEP_CHAR) {
	strcpy(dir, path);
	return TRUE;
    }
    n = strlen(dir);
    if (dir[n - 1] != DIRSEP_CHAR)
	dir[n++] = DIRSEP_CHAR;
    m = strlen(path);
    if (m + n + 1 > PATH_MAX) {
	errno = ENAMETOOLONG;
	return FALSE;
    }
    memcpy(dir + n, path, m + 1);
    return TRUE;
}

/* Create the directory dir if it doesn't already exist.
 */
int finddir(char const *dir)
{
    struct stat	st;

    return stat(dir, &st) ? createdir(dir) : S_ISDIR(st.st_mode);
}

/* Open a file, using dir as the directory if filename is not a path.
 */
int openfileindir(fileinfo *file, char const *dir, char const *filename,
		  char const *mode, char const *msg)
{
    char	buf[PATH_MAX + 1];
    int		m, n;

    if (!dir || !*dir || strchr(filename, DIRSEP_CHAR))
	return fileopen(file, filename, mode, msg);

    n = strlen(dir);
    m = strlen(filename);
    if (m + n + 1 > PATH_MAX) {
	errno = ENAMETOOLONG;
	return fileerr(file, NULL);
    }
    memcpy(buf, dir, n);
    buf[n++] = DIRSEP_CHAR;
    memcpy(buf + n, filename, m + 1);
    return fileopen(file, buf, mode, msg);
}

/* Read the given directory and call filecallback once for each file
 * contained in it.
 */
int findfiles(char const *dir, void *data,
	      int (*filecallback)(char*, void*))
{
    char	       *filename = NULL;
    DIR		       *dp;
    struct dirent      *dent;
    int			r;

    if (!(dp = opendir(dir))) {
	fileinfo tmp;
	tmp.name = (char*)dir;
	return fileerr(&tmp, "couldn't access directory");
    }

    while ((dent = readdir(dp))) {
	if (dent->d_name[0] == '.')
	    continue;
	if (!(filename = realloc(filename, strlen(dent->d_name) + 1)))
	    memerrexit();
	strcpy(filename, dent->d_name);
	r = (*filecallback)(filename, data);
	if (r < 0)
	    break;
	else if (r > 0)
	    filename = NULL;
    }

    if (filename)
	free(filename);
    closedir(dp);
    return TRUE;
}
