CC = gcc
#CFLAGS = -Wall -W -O2 -fomit-frame-pointer
CFLAGS = -Wall -W -ggdb
CFLAGS += $(shell sdl-config --cflags)

OBJS = sdloshw.o sdltimer.o sdlerr.o sdlin.o sdlout.o sdltext.o sdlerr.o

liboshw.a: $(OBJS)
	ar crs $@ $^
	cp liboshw.a ..

clean:
	rm -f $(OBJS) liboshw.a ../liboshw.a ../linkline

sdlerr.o  : sdlerr.c sdlgen.h ../defs.h ../gen.h ../oshw.h
sdlin.o   : sdlin.c sdlgen.h ../defs.h ../gen.h ../oshw.h
sdloshw.o : sdloshw.c sdlgen.h ../defs.h ../gen.h ../oshw.h
sdlout.o  : sdlout.c sdlgen.h sdltext.h ../defs.h ../gen.h ../oshw.h ../state.h
sdltext.o : sdltext.c sdltext.h sdlgen.h ../defs.h ../gen.h ../oshw.h
sdltimer.o: sdltimer.c sdlgen.h ../defs.h ../gen.h ../oshw.h