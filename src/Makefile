# Creates a list of main files, headers, and objects (that are not main).
MAINS = ndb.o
OBJECTS = $(addprefix build/,$(filter-out $(MAINS),$(patsubst %.c,%.o,$(wildcard */*.c))))
HEADERS = $(wildcard */*.h)
BINS = $(addprefix build/,$(patsubst %.bin,%.o,$(wildcard bins/*.bin)))
DIRS = $(sort $(dir $(OBJECTS) $(BINS)))
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
$(OBJECTS): build/%.o : %.c $(DIRS) $(HEADERS)
	$(CC) $(CFLAGS) -c $(patsubst build/%.o,%.c,$@) -o $@

# Setup a binary file to be linked.
$(BINS): build/%.o : %.bin $(DIRS)
	ld -r -b binary $(patsubst build/%.o,%.bin,$@) -o $@

# General format for building a main file.
$(patsubst %.o,%,$(MAINS)): $(OBJECTS) $(BINS)
	$(CC) $(CFLAGS) -c $@.c -o build/$@.o
	$(CC) $(OBJECTS) $(BINS) build/$@.o -o $@ $(LIBS)

$(DIRS):
	mkdir -p $@

# Prevent issues with clean.
.PHONY: clean

# Removes built files.
clean:
	-rm -f $(OBJECTS) $(BINS) $(addprefix build/,$(MAINS))
#	-rm -f $(patsubst %.o,%,$(MAINS))
