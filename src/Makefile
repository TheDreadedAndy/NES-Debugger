# Creates a list of main files, headers, and objects (that are not main).
MAINS = ndb.o
OBJECTS = $(filter-out $(MAINS),$(patsubst %.c,%.o,$(wildcard */*.c)))
BINS = $(patsubst %.bin,%.o,$(wildcard bins/*.bin))
OBJOUT = $(addprefix build/,$(MAINS) $(OBJECTS))
BINOUT = $(addprefix build/,$(BINS))
DIRS = $(addprefix build/,$(sort $(dir $(OBJECTS) $(BINS))))
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
$(OBJECTS): $(patsubst %.o,%.c,$(OBJECTS)) $(DIRS)
	$(CC) $(CFLAGS) -c $(patsubst %.o,%.c,$@) -o build/$@

# Setup a binary file to be linked.
$(BINS): $(patsubst %.o,%.bin,$(BINS)) $(DIRS)
	ld -r -b binary $(patsubst %.o,%.bin,$@) -o build/$@

# General format for building a main file.
$(patsubst %.o,%,$(MAINS)): $(OBJECTS) $(BINS)
	$(CC) $(CFLAGS) -c $@.c -o build/$@.o
	$(CC) $(BINOUT) $(OBJOUT) -o $@ $(LIBS)

$(DIRS):
	mkdir -p $@

# Prevent issues with clean.
.PHONY: clean

# Removes built files.
clean:
	-rm -f $(OBJOUT) $(BINS)
#	-rm -f $(patsubst %.o,%,$(MAINS))
