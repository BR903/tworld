#!/bin/sh

PREFIX=/usr/local/cross-tools
TARGET=i686-w64-mingw32
MSVCPREFIX=/usr/i586-mingw32msvc
PATH="$PREFIX/bin:$PREFIX/$TARGET/bin:$MSVCPREFIX/bin:$PATH"
export PATH

exec make "$@"
