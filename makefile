# Makefile for 'be' ("Bill's Editor") on Linux
# See 'bsdmake' for BSD version of this
all: be

OBJS=be.o commhand.o etools.o etools2.o keyhand.o showfile.o undo.o

CC=gcc
CURSES_LIB=-lncursesw

ifdef X
 ADDED_CFLAGS=-DXCURSES -DPDC_WIDE -I../PDCurses
	CURSES_LIB=-lXCurses -lXaw -lXmu -lXt -lX11 -lSM -lICE -lXext -lXpm
endif

ifdef CLANG
	CC=clang -Weverything
endif

CFLAGS=-c -O3 -Wall -Wextra -pedantic

be: $(OBJS)
	$(CC) -o be $(OBJS) $(CURSES_LIB)

install:
	cp be         /usr/local/bin

uninstall:
	rm -f /usr/local/bin/be

.c.o:
	$(CC) $(CFLAGS) $(ADDED_CFLAGS) $<

clean:
	$(RM) be
	$(RM) $(OBJS)
