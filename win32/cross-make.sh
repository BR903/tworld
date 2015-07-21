#!/bin/sh

PREFIX=/usr
TARGET=i586-mingw32msvc
PATH="$PREFIX/$TARGET/bin:$PATH"
export PATH
exec make $*
