# Makefile for 'be' ("Bill's Editor") on Linux
# See 'bsdmake' for BSD version of this

OBJS=be.o commhand.o etools.o etools2.o keyhand.o showfile.o undo.o

CC=cc
CURSES_LIB=-lncursesw
RM=rm -f

ifdef X
 ADDED_CFLAGS=-DXCURSES -DPDC_WIDE -DPDC_FORCE_UTF8 -I../PDCurses
	CURSES_LIB=-lXCurses -lXaw -lXmu -lXt -lX11 -lSM -lICE -lXext -lXpm
endif

ifdef VT
	ADDED_CFLAGS=-DPDC_WIDE -DPDC_FORCE_UTF8 -DVT -I$(HOME)/PDCurses
	CURSES_LIB=$(HOME)/PDCurses/vt/libpdcurses.a
endif

ifdef W64
 CC=x86_64-w64-mingw32-g++
 EXE=.exe
 LIB_DIR=$(INSTALL_DIR)/win_lib
 LIBSADDED=-L $(LIB_DIR) -mwindows
	ADDED_CFLAGS=-DPDC_WIDE -DPDC_FORCE_UTF8 -I$(HOME)/PDCurses
	CURSES_LIB=$(HOME)/PDCurses/wingui/pdcurses.a
endif

ifdef CLANG
	CC=clang -Weverything
endif

CFLAGS=-c -O3 -Wall -Wextra -pedantic

all: be$(EXE)

be$(EXE): $(OBJS)
	$(CC) -o be$(EXE) $(OBJS) $(CURSES_LIB)

install:
	cp be$(EXE)         $(HOME)/bin

uninstall:
	$(RM) $(HOME)/bin/be$(EXE)

.c.o:
	$(CC) $(CFLAGS) $(ADDED_CFLAGS) $<

clean:
	$(RM) be$(EXE)
	$(RM) $(OBJS)
