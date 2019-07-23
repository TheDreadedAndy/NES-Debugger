# Creates a list of main files, headers, and objects (that are not main).
MAINS = ndb.o sdl.o
HEADERS = $(wildcard */*.h)
OBJECTS = $(filter-out $(MAINS),$(patsubst %.c,%.o,$(wildcard */*.c)))
CC = gcc
LIBS = $(shell sdl2-config --libs)
override CFLAGS += -Wall -Wshadow -Wextra -Werror -std=c99 -pedantic -O3 $(shell sdl2-config --cflags)

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
