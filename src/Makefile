# Creates a list of main files, headers, and objects (that are not main).
MAINS = ndb.o
OBJECTS = $(filter-out $(MAINS),$(patsubst %.c,%.o,$(wildcard */*.c)))
OBJOUT = $(addprefix build/,$(MAINS) $(OBJECTS))
DIRS = $(addprefix build/,$(sort $(dir $(OBJECTS))))
CC = gcc
LIBS = $(shell sdl2-config --libs)
override CFLAGS += -fwrapv -Wall -Wshadow -Wextra -Werror -std=c99 -pedantic -ggdb -DDEBUG $(shell sdl2-config --cflags)

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
%.o: %.c $(DIRS)
	$(CC) $(CFLAGS) -c $< -o build/$@

# General format for building a main file.
$(patsubst %.o,%,$(MAINS)): $(OBJECTS)
	$(CC) $(CFLAGS) -c $@.c -o build/$@.o
	$(CC) $(OBJOUT) -o $@ $(LIBS)

$(DIRS):
	mkdir -p $@

# Prevent issues with clean.
.PHONY: clean

# Removes built files.
clean:
	-rm -f $(OBJOUT)
#	-rm -f $(patsubst %.o,%,$(MAINS))
