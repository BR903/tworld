#!/bin/sh

PATH=/usr/local/cross-tools/bin:/usr/local/cross-tools/i386-mingw32msvc/bin:$PATH \
	./configure --cache-file=config.cross.cache \
	--host=i386-mingw32msvc \
	--target=i386-mingw32msvc \
	--build=i386-linux \
	$*

