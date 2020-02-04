# NES Debugger/NES, I guess?

NES Debugger is an application which allows for the runtime disassembly and
debugging of NES games within the terminal. Almost none of the code for the
debugging aspect of this program has been written yet. The files currently
in this repo represent "Nes, I guess?", which is the name of the emulator
which NES Debugger will built on top of.

"NES, I guess?" was written from scratch using the documentation available
on nesdev.com. The project is written entirely in C++, using the Google
style guide. Currently, games which use INES Mappers 0-2 can be played.

In order to compile the master branch, you need only the build tools for your
distribution of Linux, g++, and SDL2-Dev. It can then be compiled using
the given make file. The emulator will be created as the executable "ndb".
Compiling on windows is untested as of now, though I've attempted not to rule
out support for it.

The current status of the project is as follows:  
Working on: Rewrite of the processor emulation.  
Compiles?: No (Last working commit is 62733c72dad6140ccf35530a42f0daaa564012a2).  
Next up: PPU rewrite or Memory rewrite to begin work on the disassembler.  
