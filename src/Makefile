# Creates a list of main files, headers, and objects (that are not main).
MAINS = ndb.o
HEADERS = $(wildcard *.h)
OBJECTS = $(filter-out $(MAINS),$(patsubst %.c,%.o,$(wildcard *.c)))
CC = gcc
override CFLAGS += -Wall -Wshadow -Wextra -Werror -std=c99 -pedantic

# Sets the default command to ndb
default: ndb

# Builds a general object file.
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Assigns all headers as the prereqs for all main files.
$(patsubst %.o,%.c,$(MAINS)): $(HEADERS)

# General format for building a main file.
$(patsubst %.o,%,$(MAINS)): $(OBJECTS)
	$(CC) $(CFLAGS) -c $@.c -o $@.o
	$(CC) $@.o $(OBJECTS) -o $@

# Prevent issues with clean.
.PHONY: clean

# Removes built files.
clean:
	-rm -f $(OBJECTS)
	-rm -f $(MAINS)
	-rm -f $(patsubst %.o,%,$(MAINS))
