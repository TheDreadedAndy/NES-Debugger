HEADERS = $(wildcard *.h)
MAINS = ndb.o
OBJECTS = $(filter-out $(MAINS),$(patsubst %.c,%.o,$(wildcard *.c)))
CC = gcc
override CFLAGS += -Wall -Wshadow -Wextra -Werror -std=c99 -pedantic

default: ndb

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(patsubst %.o,%.c,$(MAINS)): $(HEADERS)

$(patsubst %.o,%,$(MAINS)): $(OBJECTS)
	$(CC) $(CFLAGS) -c $@.c -o $@.o
	$(CC) $@.o $(OBJECTS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f $(MAINS)
	-rm -f $(patsubst %.o,%,$(MAINS))
