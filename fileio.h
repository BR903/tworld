/* fileio.h: Simple file/directory access functions with error-handling.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_fileio_h_
#define	_fileio_h_

#include	"defs.h"

/* Reset a fileinfo structure to indicate no file.
 */
extern void clearfileinfo(fileinfo *file);

/* Open a file. If the fileinfo structure does not already have a
 * filename assigned to it, name will be used as the filename. If msg
 * is NULL, no error will be displayed if the file cannot be opened.
 * If msg points to a string, an error will be displayed. The text of
 * msg will be used only if errno is zero; otherwise a message
 * appropriate to the error will be used.
 */
extern int fileopen(fileinfo *file, char const *name, char const *mode,
		    char const *msg);

/* The following functions correspond directly to C's standard I/O
 * functions. The extra msg parameter works as described above for
 * fileopen().
 */
extern int filerewind(fileinfo *file, char const *msg);
extern int filegetpos(fileinfo *file, fpos_t *pos, char const *msg);
extern int filesetpos(fileinfo *file, fpos_t *pos, char const *msg);
extern int fileread(fileinfo *file, void *data, unsigned long size,
		    char const *msg);
extern int filewrite(fileinfo *file, void const *data, unsigned long size,
		     char const *msg);
extern void fileclose(fileinfo *file, char const *msg);

/* filetestend() forces a check for EOF by attempting to read a byte
 * from the file, and ungetting the byte if one is successfully read.
 */
extern int filetestend(fileinfo *file);

/* The following functions read an unsigned integer from the current
 * position in the given file. For the multi-byte values, the value
 * is always assumed to be stored in little-endian.
 */
extern int filereadint8(fileinfo *file, unsigned char *val8,
			char const *msg);
extern int filewriteint8(fileinfo *file, unsigned char val8,
			 char const *msg);
extern int filereadint16(fileinfo *file, unsigned short *val16,
			 char const *msg);
extern int filewriteint16(fileinfo *file, unsigned short val16,
			  char const *msg);
extern int filereadint32(fileinfo *file, unsigned long *val32,
			 char const *msg);
extern int filewriteint32(fileinfo *file, unsigned long val32,
			  char const *msg);

/* Read size bytes from the given file and return the bytes in a
 * newly allocated buffer.
 */
extern void *filereadbuf(fileinfo *file, unsigned long size, char const *msg);

/* Read one full line from fp and store the first len characters,
 * including any trailing newline. len receives the length of the line
 * stored in buf upon return.
 */
extern int filegetline(fileinfo *file, char *buf, int *len, char const *msg);

/* Return the maximum size of a legal pathname.
 */
extern int getpathbufferlen(void);

/* Return a buffer big enough to hold any legal pathname.
 */
extern char *getpathbuffer(void);

/* Return TRUE if name contains a path but is not a directory itself.
 */
extern int haspathname(char const *name);

/* Append the path and/or file contained in path to dir.
 */
extern int combinepath(char *dir, char const *path);

/* Verify that the given directory exists, or create it if it doesn't.
 */
extern int finddir(char const *dir);

/* Open a file, using dir as the directory if filename is not already
 * a complete pathname.
 */
extern int openfileindir(fileinfo *file, char const *dir, char const *filename,
			 char const *mode, char const *msg);

/* Call filecallback once for every file in dir; the first argument to
 * the callback function is an allocated buffer containing the
 * filename. If the callback's return value is zero, the buffer is
 * deallocated normally; if the return value is positive, the callback
 * function inherits the buffer and the responsibility of freeing it.
 * If the return value is negative, the function stops scanning the
 * directory and returns to the original caller.
 */
extern int findfiles(char const *dir, void *data,
		     int (*filecallback)(char*, void*));

/* Display a simple error message prefixed by the name of the given
 * file. If errno is set, a message appropriate to the value is used;
 * otherwise the text pointed to by msg is used. If msg is NULL, no
 * error message is display. The return value is always FALSE.
 */
extern int _fileerr(char const *cfile, unsigned long lineno,
		    fileinfo *file, char const *msg);
#define	fileerr(file, msg)	(_fileerr(__FILE__, __LINE__, (file), (msg)))

#endif
