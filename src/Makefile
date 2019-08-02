# Creates a list of main files, headers, and objects (that are not main).
MAINS = ndb.o
HEADERS = $(wildcard */*.h)
OBJECTS = $(filter-out $(MAINS),$(patsubst %.c,%.o,$(wildcard */*.c)))
CC = gcc
LIBS = $(shell sdl2-config --libs)
override CFLAGS += -Wall -Wshadow -Wextra -Werror -std=c99 -pedantic -O3 $(shell sdl2-config --cflags)

# Determine which OS is being compiled for.
ifeq ($(OS),Windows_NT)
    override CFLAGS += -D_NES_OSWIN
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Linux)
        override CFLAGS += -D_NES_OSLIN
    else
        $(error Fatal: Cannot determine target OS.)
    endif
endif

# Sets the default command to ndb
default: ndb

# Builds a general object file.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# General format for building a main file.
$(patsubst %.o,%,$(MAINS)): $(OBJECTS)
	$(CC) $(CFLAGS) -c $@.c -o $@.o
	$(CC) $@.o $(OBJECTS) -o $@ $(LIBS)

# Prevent issues with clean.
.PHONY: clean

# Removes built files.
clean:
	-rm -f $(OBJECTS)
	-rm -f $(MAINS)
#	-rm -f $(patsubst %.o,%,$(MAINS))
