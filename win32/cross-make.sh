#!/bin/sh

PREFIX=/usr/local/cross-tools
TARGET=i686-w64-mingw32msvc
MSVCPREFIX=/usr/i586-mingw32msvc
PATH="$PREFIX/bin:$PREFIX/$TARGET/bin:$MSVCPREFIX/bin:$PATH"
export PATH

#CFLAGS = -I$MSVCPREFIX/include -I$PREFIX/$TARGET/include -mwindows
#LDFLAGS = -L$MSVCPREFIX/lib -L$PREFIX/$TARGET/lib -mwindows
#LOADLIBES = -lmingw32

exec make "$*"
