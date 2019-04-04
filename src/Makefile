HEADERS = 2A03.h contracts.h cpu_regs.h machinecode.h memory.h microops.h state.h util.h
OBJECTS = 2A03.o cpu_regs.o memory.o state.o util.o
CC = gcc
override CFLAGS += -Wall -Wshadow -Wextra -Werror -std=c99 -pedantic

default: ndb

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

ndb: ndb.o $(OBJECTS)
	$(CC) ndb.o $(OBJECTS) -o ndb

clean:
	-rm -f $(OBJECTS)
	-rm -f ndb.o
	-rm -f ndb
