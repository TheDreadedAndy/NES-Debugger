# The names of the binaries to be created.
# A corresponding .cc file must exist for each entry.
MAINS = ndb

# The names of all non-main objects.
OBJECTS = $(addprefix build/,$(filter-out $(addsuffix .o,$(MAINS)),$(patsubst %.cc,%.o,$(wildcard */*.cc))))

# The objects to be compiled from .cc source files and the dependency
# files corresponding to these objects.
SRC = $(OBJECTS) $(addprefix build/,$(addsuffix .o,$(MAINS)))
DEPS = $(patsubst %.o,%.d,$(SRC))

# The binary files to be linked to the executable binary.
BINS = $(addprefix build/,$(patsubst %.bin,%.o,$(wildcard bins/*.bin)))

# The directories to be made before building.
DIRS = $(sort $(dir $(BINS) $(SRC)))

# The compiler to be used and its flags.
CXX = g++
override CXXFLAGS += -std=c++17 -Wall -Wshadow -Wextra -Wold-style-cast -Werror -pedantic -O3 $(shell sdl2-config --cflags)

# Libraries to be linked to the binary.
LIBS = $(shell sdl2-config --libs)

# Determine which OS is being compiled for.
ifeq ($(OS),Windows_NT)
    override CXXFLAGS += -D_NES_OSWIN
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Linux)
        override CXXFLAGS += -D_NES_OSLIN
    else
        $(error Fatal: Cannot determine target OS.)
    endif
endif

# Sets the default command to ndb
default: ndb

# Builds an object file and an associated dependency file.
$(SRC): build/%.o : %.cc build/%.d | $(DIRS)
	$(CXX) $(CXXFLAGS) -MMD -c $(patsubst build/%.o,%.cc,$@) -MF $(patsubst %.o,%.d,$@) -o $@

# Ensures that an object will be rebuilt if its dependency list is missing.
$(DEPS):;

# Includes all built dependency files as they are created.
# This ensures that files will be rebuilt when the headers they depend on change.
-include $(DEPS)

# Setup a binary file to be linked.
$(BINS): build/%.o : %.bin | $(DIRS)
	ld -r -b binary $(patsubst build/%.o,%.bin,$@) -o $@

# General format for building a main file.
$(MAINS): % : build/%.o $(OBJECTS) $(BINS) | $(DIRS)
	$(CXX) $(OBJECTS) $(BINS) build/$@.o -o $@ $(LIBS)

$(DIRS):
	mkdir -p $@

# Prevent issues with make commands.
.PHONY: install
.PHONY: uninstall
.PHONY: clean

# Install to program to local directories.
install:
ifneq (,$(wildcard ./ndb))
ifeq ($(UNAME),Linux)
	-mkdir -p /usr/local/share/applications
	-mkdir -p /usr/local/share/pixmaps
	-cp ./ndb /usr/local/bin/
	-cp ./resources/ndb.desktop /usr/local/share/applications/
	-cp ./resources/ndb.png /usr/local/share/pixmaps/
endif
endif

# Remove the program from local directories.
uninstall:
ifeq ($(UNAME),Linux)
	-rm /usr/local/bin/ndb
	-rm /usr/local/share/applications/ndb.desktop
	-rm /usr/local/share/pixmaps/ndb.png
endif

# Removes built files.
clean:
	-rm -f $(SRC) $(BINS)
	-rm -f $(DEPS)
	-rm -f $(MAINS)
