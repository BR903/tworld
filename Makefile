CC = gcc
CFLAGS = -ggdb -Wall -W '-DDATADIR="/home/breadbox/c/cc/web"'
LDFLAGS = -ggdb -Wall -W
#LOADLIBES = -lefence
LOADLIBES = -lvgagl -lvga -lefence

OBJS = \
chips.o state.o logic.o fileread.o parse.o solutions.o movelist.o timer.o \
random.o dirio.o userin.o userout.o help.o

chips: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES)
	chmod 4755 $@

clean:
	rm -f $(OBJS) chips


chips.o    : chips.c gen.h cc.h dirio.h fileread.h movelist.h state.h	\
	     solutions.h random.h timer.h userin.h userout.h help.h
state.o    : state.c state.h statestr.h gen.h cc.h movelist.h timer.h	\
	     random.h logic.h fileread.h userin.h userout.h
logic.o    : logic.c logic.h gen.h cc.h objects.h statestr.h movelist.h	\
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
userin.o   : userin.c userin.h gen.h
userout.o  : userout.c cctiles.c userout.h gen.h cc.h objects.h statestr.h
help.o     : help.c help.h gen.h cc.h objects.h userout.h


cctiles.c: imgdir/*.xpm
	(cd imgdir ; ./mktiles > ../cctiles.c)
