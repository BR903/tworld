CC = gcc
CFLAGS = -ggdb -Wall '-DDATADIR="/home/breadbox/c/cc/web"'
LDFLAGS = -ggdb -Wall
#LOADLIBES = -lefence
LOADLIBES = -lvgagl -lvga

OBJS = \
chips.o state.o logic.o fileread.o parse.o solutions.o movelist.o timer.o \
random.o dirio.o userio.o help.o

chips: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES)
	chmod 4755 $@
	#/home/breadbox/c/svga/mksuid $@

clean:
	rm -f $(OBJS) chips


chips.o    : chips.c gen.h cc.h dirio.h fileread.h movelist.h state.h	\
	     solutions.h random.h timer.h userio.h help.h
state.o    : state.c state.h gen.h cc.h movelist.h timer.h random.h	\
	     logic.h fileread.h userio.h
logic.o    : logic.c logic.h gen.h cc.h objects.h state.h movelist.h	\
	     fileread.h random.h
fileread.o : fileread.c fileread.h gen.h cc.h movelist.h dirio.h	\
	     solutions.h parse.h
parse.o    : parse.c parse.h gen.h cc.h objects.h fileread.h movelist.h
solutions.o: solutions.c solutions.h gen.h cc.h movelist.h dirio.h	\
	     fileread.h
movelist.o : movelist.c movelist.h gen.h
random.o   : random.c random.h gen.h
timer.o    : timer.c timer.h gen.h
dirio.o    : dirio.c dirio.h gen.h
userio.o   : userio.c cctiles.c userio.h gen.h cc.h objects.h
help.o     : help.c help.h gen.h cc.h objects.h userio.h


cctiles.c: imgdir/*.xpm
	(cd imgdir ; ./mktiles > ../cctiles.c)
